#include "BotController.h"
#include "FPSCharacter.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "BombPlayerState.h"
#include "BombSite.h"
#include "BombActor.h"
#include "CalloutMarker.h"
#include "WeaponPickup.h"
#include "Weapons.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "ParisBombMap.h"

ABotController::ABotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	bSetControlRotationFromPawnOrientation = false;
	Rng.Initialize(FMath::Rand());
	Skill = Rng.FRandRange(0.35f, 0.85f);
	bCunning = Rng.FRand() < 0.4f;
}

void ABotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AFPSCharacter* C = Cast<AFPSCharacter>(InPawn))
	{
		C->bIsBot = true;
	}
}

AFPSCharacter* ABotController::Me() const { return Cast<AFPSCharacter>(GetPawn()); }
ABombGameMode* ABotController::GM() const { return Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)); }

void ABotController::OnRoundStart()
{
	Task = EBotTask::Idle;
	Enemy = nullptr;
	TargetSite = nullptr;
	TargetDrop = nullptr;
	bWentViaWaypoint = false;
	bSiteNadesThrown = false;
	EnemySeenSince = -1.f;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

// ---------------------------------------------------------------- buying (FPS Reborn modes.js _botsBuy)

namespace
{
	FName WeightedPick(const TArray<TPair<FName, float>>& Opts, FRandomStream& Rng)
	{
		float Total = 0.f;
		for (const auto& O : Opts) Total += O.Value;
		float R = Rng.FRand() * Total;
		for (const auto& O : Opts) { R -= O.Value; if (R <= 0.f) return O.Key; }
		return Opts.Last().Key;
	}
}

void ABotController::BuyPhase()
{
	AFPSCharacter* B = Me();
	ABombGameMode* G = GM();
	ABombPlayerState* PS = B ? B->GetPlayerState<ABombPlayerState>() : nullptr;
	if (!B || !G || !PS || B->bDead) return;
	const bool bPistolRound = G->IsPistolRound();
	auto Money = [PS]() { return PS->Money; };

	if (!B->HasMain() && !bPistolRound)
	{
		const int32 M = Money();
		TArray<TPair<FName, float>> Opts;
		if (M >= 5500) Opts.Emplace(TEXT("lr50"), 1.f);
		if (M >= 3300) { Opts.Emplace(TEXT("ar7"), 4.f); Opts.Emplace(TEXT("ar7s"), 2.f); }
		if (M >= 1800) Opts.Emplace(TEXT("ps12"), 1.f);
		if (M >= 1500) Opts.Emplace(TEXT("sr30"), M < 3300 ? 1.f : 0.3f);
		if (M >= 900) Opts.Emplace(TEXT("smg9"), M < 2900 ? 2.f : 0.5f);
		const bool bSaving = M < 2000 && Rng.FRand() < 0.5f;
		if (Opts.Num() && !bSaving) G->Buy(B, WeightedPick(Opts, Rng));
		else if (bSaving && M >= 650 && Rng.FRand() < 0.3f) G->Buy(B, TEXT("so12"));     // a cheap sawn-off while saving
	}
	if (B->Guns[1].IsValid() && B->Guns[1].Def->Key == TEXT("p9"))
	{
		const int32 M = Money();
		TArray<TPair<FName, float>> Opts;
		if (bPistolRound && Rng.FRand() < 0.7f)
		{
			if (M >= 800) Opts.Emplace(TEXT("r44"), 1.f);
			if (M >= 500) Opts.Emplace(TEXT("sp7"), 2.f);
			if (M >= 400) Opts.Emplace(TEXT("mp11"), 1.5f);
		}
		else if (!bPistolRound && M >= 4500) { Opts.Emplace(TEXT("r44"), 1.f); Opts.Emplace(TEXT("sp7"), 1.f); }
		if (Opts.Num()) G->Buy(B, WeightedPick(Opts, Rng));
	}
	if (B->Armor < 50 && Money() >= 1000 && !bPistolRound) G->Buy(B, TEXT("heavy"));
	else if (B->Armor < 25 && Money() >= 400) G->Buy(B, TEXT("light"));
	// grenades: attackers like smokes + flashes to take a site, anyone may grab a frag
	const bool bAttack = B->GetTeam() == EBombTeam::Attackers;
	if (bAttack)
	{
		if (Money() >= 200 && Rng.FRand() < 0.8f) G->Buy(B, TEXT("smoke"));
		if (Money() >= 250 && Rng.FRand() < (bCunning ? 0.9f : 0.5f)) G->Buy(B, TEXT("flash"));
	}
	else if (bCunning && Money() >= 250 && Rng.FRand() < 0.7f) G->Buy(B, TEXT("flash"));
	if (Money() >= 400 && Rng.FRand() < 0.35f) G->Buy(B, TEXT("frag"));
	ChooseWeapon();
}

void ABotController::ChooseWeapon()
{
	AFPSCharacter* M = Me();
	if (!M || M->bDead) return;
	const EHeldSlot Want = M->HasMain() ? EHeldSlot::Main : EHeldSlot::Side;
	if (M->Held != Want) M->SwitchTo(Want);
}

// ---------------------------------------------------------------- movement helpers

void ABotController::GoTo(const FVector& Loc, float Accept)
{
	ObjectiveLoc = Loc;
	const EPathFollowingRequestResult::Type R = MoveToLocation(Loc, Accept, true, true, true, true);
	if (R == EPathFollowingRequestResult::Failed)
	{
		Task = EBotTask::Idle;   // choose something else next think
	}
	else
	{
		Task = EBotTask::Travel;
	}
	StuckSince = GetWorld()->GetTimeSeconds();
	LastPos = Me() ? Me()->GetActorLocation() : FVector::ZeroVector;
}

FVector ABotController::RandomNavPointNear(const FVector& Origin, float Radius) const
{
	FNavLocation Out;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (Nav->GetRandomReachablePointInRadius(Origin, Radius, Out))
		{
			return Out.Location;
		}
		if (Nav->ProjectPointToNavigation(Origin, Out, FVector(300.f, 300.f, 500.f)))
		{
			return Out.Location;
		}
	}
	return Origin;
}

AWeaponPickup* ABotController::NearestDrop(float MaxDist) const
{
	const AFPSCharacter* M = Me();
	if (!M) return nullptr;
	AWeaponPickup* Best = nullptr; float BestD = MaxDist;
	for (TActorIterator<AWeaponPickup> It(GetWorld()); It; ++It)
	{
		if (!It->Gun.IsValid()) continue;
		const float D = FVector::Dist(It->GetActorLocation(), M->GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	return Best;
}

void ABotController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (Task == EBotTask::Travel)
	{
		Task = EBotTask::Hold;
		HoldUntil = GetWorld()->GetTimeSeconds() + Rng.FRandRange(6.f, 18.f);
	}
	else if (Task == EBotTask::Fetch)
	{
		Task = EBotTask::Idle;
	}
}

bool ABotController::CanSee(const AFPSCharacter* Other) const
{
	const AFPSCharacter* M = Me();
	if (!M || !Other) return false;
	if (GetWorld()->GetTimeSeconds() < M->BlindUntil) return false;      // flashed
	const FVector Eye = M->GetEyeLocation();
	const FVector To = Other->GetEyeLocation() - Eye;
	const float Dist = To.Size();
	if (Dist > 9000.f) return false;
	const FVector Fwd = M->GetActorForwardVector();
	const float Cos = FVector::DotProduct(Fwd, To.GetSafeNormal());
	if (Dist > 350.f && Cos < 0.17f) return false;   // ~80 deg half-angle field of view
	FCollisionQueryParams P(SCENE_QUERY_STAT(BotSight), false, M);
	P.AddIgnoredActor(Other);
	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Other->GetEyeLocation(), ECC_Visibility, P);
	if (!bBlocked) return true;
	const bool bBlocked2 = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Other->GetActorLocation(), ECC_Visibility, P);
	return !bBlocked2;
}

AFPSCharacter* ABotController::FindVisibleEnemy() const
{
	const AFPSCharacter* M = Me();
	ABombGameMode* G = GM();
	if (!M || !G) return nullptr;
	AFPSCharacter* Best = nullptr;
	float BestDist = 1e9f;
	for (AFPSCharacter* C : G->Characters)
	{
		if (!C || C == M || C->bDead || C->GetTeam() == M->GetTeam()) continue;
		const float D = FVector::Dist(C->GetActorLocation(), M->GetActorLocation());
		if (D < BestDist && CanSee(C))
		{
			Best = C;
			BestDist = D;
		}
	}
	return Best;
}

void ABotController::ChooseObjective()
{
	AFPSCharacter* M = Me();
	ABombGameMode* G = GM();
	ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	if (!M || !G || !GS) return;
	const EBombTeam Team = M->GetTeam();

	// no main gun: grab a dropped one if it is close
	if (!M->HasMain())
	{
		if (AWeaponPickup* Drop = NearestDrop(Weapons::DropBotRange * 100.f))
		{
			TargetDrop = Drop;
			GoTo(Drop->GetActorLocation(), 60.f);
			Task = EBotTask::Fetch;
			return;
		}
	}

	if (Team == EBombTeam::Attackers)
	{
		if (GS->bBombPlanted && G->Bomb)
		{
			GoTo(RandomNavPointNear(G->Bomb->GetActorLocation(), 900.f));
			return;
		}
		if (GS->bBombDropped && G->Bomb && !M->bHasBomb && G->bBotsMayTakeBomb)
		{
			GoTo(G->Bomb->GetActorLocation(), 60.f);
			return;
		}
		ABombSite* Site = G->AttackSite;
		if (!Site) return;
		TargetSite = Site;
		if (!bWentViaWaypoint && G->Callouts.Num() > 0 && Rng.FRand() < 0.6f)
		{
			const ACalloutMarker* Via = G->Callouts[Rng.RandRange(0, G->Callouts.Num() - 1)];
			bWentViaWaypoint = true;
			GoTo(RandomNavPointNear(Via->GetActorLocation(), 400.f), 200.f);
			return;
		}
		bWentViaWaypoint = true;
		if (M->bHasBomb)
		{
			GoTo(RandomNavPointNear(Site->GetActorLocation(), 500.f), 60.f);
		}
		else
		{
			GoTo(RandomNavPointNear(Site->GetActorLocation(), 1400.f), 120.f);
		}
	}
	else
	{
		if (GS->bBombPlanted && G->Bomb)
		{
			GoTo(G->Bomb->GetActorLocation(), 70.f);
			return;
		}
		ABombSite* Site = TargetSite.Get();
		if (!Site && G->Sites.Num() > 0)
		{
			const int32 Idx = G->Characters.IndexOfByKey(M);
			Site = G->Sites[(Idx % 5 < 3) ? 0 : 1 % G->Sites.Num()];
			if (Rng.FRand() < 0.25f)
			{
				Site = G->Sites[Rng.RandRange(0, G->Sites.Num() - 1)];
			}
			TargetSite = Site;
		}
		if (Site)
		{
			GoTo(RandomNavPointNear(Site->GetActorLocation(), 1600.f), 120.f);
		}
	}
}

void ABotController::Think()
{
	AFPSCharacter* M = Me();
	ABombGameMode* G = GM();
	ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	if (!M || !G || !GS) return;
	const float Now = GetWorld()->GetTimeSeconds();

	if (M->bDead || GS->Phase != ERoundPhase::Live)
	{
		if (Task != EBotTask::Idle)
		{
			StopMovement();
			M->StopFire();
			M->StopInteract();
			ClearFocus(EAIFocusPriority::Gameplay);
			Task = EBotTask::Idle;
		}
		return;
	}
	if (M->Held == EHeldSlot::Nade || M->Held == EHeldSlot::Knife) ChooseWeapon();

	// ---- combat has priority
	AFPSCharacter* Seen = FindVisibleEnemy();
	if (Seen)
	{
		const bool bNew = Enemy.Get() != Seen;
		if (bNew)
		{
			Enemy = Seen;
			EnemySeenSince = Now;
			NextBurstAt = Now + FMath::Lerp(0.55f, 0.15f, Skill);   // reaction time
		}
		LastEnemySeen = Now;
		if (GM() && M->GetTeam() == GM()->HumanTeam) Seen->SpottedUntil = FMath::Max(Seen->SpottedUntil, Now + 1.2f);   // shows on your minimap
		if (Task != EBotTask::Engage)
		{
			StopMovement();
			M->StopInteract();
			Task = EBotTask::Engage;
			NextStrafeAt = Now;
		}
		const float DistM = FVector::Dist(Seen->GetActorLocation(), M->GetActorLocation()) / 100.f;
		// a frag at a fresh target 5-25 m away
		if (bNew && DistM > 5.f && DistM < 25.f && Now - LastNadeTime > 4.f && Rng.FRand() < 0.6f && M->NadeCount(TEXT("frag")) > 0)
		{
			if (M->ThrowNadeTo(TEXT("frag"), Seen->GetActorLocation() + FVector(0, 0, 30.f))) LastNadeTime = Now;
		}
		// aim at the chest with skill-based error
		const float Err = FMath::Lerp(60.f, 6.f, Skill);
		const FVector AimPt = Seen->GetActorLocation() + FVector(0.f, 0.f, 35.f) + FVector(Rng.FRandRange(-Err, Err), Rng.FRandRange(-Err, Err), Rng.FRandRange(-Err, Err));
		SetFocalPoint(AimPt, EAIFocusPriority::Gameplay);
		const FWeaponDef* D = M->HeldDef();
		const bool bSniper = D && D->HasScope();
		if (Now >= NextBurstAt && Now >= BurstEndAt)
		{
			BurstEndAt = Now + (bSniper ? 0.2f : Rng.FRandRange(0.25f, 0.6f));
			NextBurstAt = BurstEndAt + (bSniper ? Rng.FRandRange(0.6f, 1.1f) : Rng.FRandRange(0.15f, 0.45f));
		}
		const bool bShoot = Now < BurstEndAt && Now >= EnemySeenSince + FMath::Lerp(0.5f, 0.12f, Skill);
		if (bShoot) M->StartFire(); else M->StopFire();
		FGunState* Gun = M->HeldGun();
		if (Gun && !Gun->Def->bMelee && Gun->Ammo == 0) M->Reload();
		// strafe a little (snipers hold still to stay "scoped")
		if (!bSniper && Now >= NextStrafeAt)
		{
			NextStrafeAt = Now + Rng.FRandRange(0.8f, 1.8f);
			const FVector Right = M->GetActorRightVector() * (Rng.FRand() < 0.5f ? 1.f : -1.f) * Rng.FRandRange(150.f, 350.f);
			MoveToLocation(M->GetActorLocation() + Right, 30.f, false, true, false, false);
		}
		return;
	}

	// lost sight
	if (Task == EBotTask::Engage)
	{
		M->StopFire();
		if (Now - LastEnemySeen > 2.5f)
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			Enemy = nullptr;
			Task = EBotTask::Idle;
		}
		else
		{
			return;   // keep looking where the enemy was for a moment
		}
	}

	// ---- objectives
	const EBombTeam Team = M->GetTeam();

	// attackers: smoke the site and flash it before going in
	if (Team == EBombTeam::Attackers && !bSiteNadesThrown && G->AttackSite && !GS->bBombPlanted)
	{
		const FVector SiteLoc = G->AttackSite->GetActorLocation();
		const float DistM = FVector::Dist2D(SiteLoc, M->GetActorLocation()) / 100.f;
		if (DistM < 30.f)
		{
			bSiteNadesThrown = true;
			if (M->NadeCount(TEXT("smoke")) > 0) M->ThrowNadeTo(TEXT("smoke"), SiteLoc);
			if (M->NadeCount(TEXT("flash")) > 0 && Rng.FRand() < 0.7f) M->ThrowNadeTo(TEXT("flash"), SiteLoc + FVector(0, 0, 100.f));
			ChooseWeapon();
		}
	}

	// fetching a dropped gun
	if (Task == EBotTask::Fetch)
	{
		AWeaponPickup* Drop = TargetDrop.Get();
		if (!Drop || M->HasMain()) { Task = EBotTask::Idle; }
		else if (FVector::Dist(Drop->GetActorLocation(), M->GetActorLocation()) < 150.f)
		{
			M->TakeDroppedGun(Drop);
			ChooseWeapon();
			Task = EBotTask::Idle;
		}
		else
		{
			return;
		}
	}

	// planting
	if (Team == EBombTeam::Attackers && M->bHasBomb && !GS->bBombPlanted)
	{
		for (ABombSite* S : G->Sites)
		{
			if (S && S->IsInside(M->GetActorLocation()))
			{
				if (Task != EBotTask::Interact)
				{
					StopMovement();
					Task = EBotTask::Interact;
					M->StartInteract();
				}
				return;
			}
		}
		if (Task == EBotTask::Interact)
		{
			M->StopInteract();
			Task = EBotTask::Idle;
		}
	}
	// defusing
	if (Team == EBombTeam::Defenders && GS->bBombPlanted && G->Bomb)
	{
		const float D = FVector::Dist(M->GetActorLocation(), G->Bomb->GetActorLocation());
		if (D < 160.f)
		{
			if (Task != EBotTask::Interact)
			{
				StopMovement();
				Task = EBotTask::Interact;
				M->StartInteract();
				SetFocalPoint(G->Bomb->GetActorLocation(), EAIFocusPriority::Gameplay);
			}
			return;
		}
		if (Task != EBotTask::Travel || FVector::Dist(ObjectiveLoc, G->Bomb->GetActorLocation()) > 200.f)
		{
			M->StopInteract();
			GoTo(G->Bomb->GetActorLocation(), 70.f);
		}
		return;
	}
	if (Task == EBotTask::Interact)
	{
		M->StopInteract();
		Task = EBotTask::Idle;
	}

	if (Task == EBotTask::Idle)
	{
		ChooseObjective();
		return;
	}
	if (Task == EBotTask::Travel)
	{
		const FVector Pos = M->GetActorLocation();
		if (FVector::Dist2D(Pos, LastPos) > 60.f)
		{
			LastPos = Pos;
			StuckSince = Now;
		}
		else if (Now - StuckSince > 4.f)
		{
			Task = EBotTask::Idle;
			StopMovement();
		}
		return;
	}
	if (Task == EBotTask::Hold)
	{
		if (Now >= NextLookAt)
		{
			NextLookAt = Now + Rng.FRandRange(1.5f, 4.f);
			const FVector LookDir = FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f).Vector();
			SetFocalPoint(M->GetActorLocation() + LookDir * 800.f + FVector(0.f, 0.f, 60.f), EAIFocusPriority::Gameplay);
		}
		if (Now >= HoldUntil)
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			Task = EBotTask::Idle;   // reposition / advance
		}
	}
}

void ABotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextThink)
	{
		NextThink = Now + 0.15f;
		Think();
	}
}
