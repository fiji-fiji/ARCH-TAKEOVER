#include "BotController.h"
#include "FPSCharacter.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "BombSite.h"
#include "BombActor.h"
#include "CalloutMarker.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "ParisBombMap.h"

ABotController::ABotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	bSetControlRotationFromPawnOrientation = false;
	Rng.Initialize(FMath::Rand());
	Skill = Rng.FRandRange(0.35f, 0.85f);
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
	bWentViaWaypoint = false;
	EnemySeenSince = -1.f;
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}

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

void ABotController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (Task == EBotTask::Travel)
	{
		Task = EBotTask::Hold;
		HoldUntil = GetWorld()->GetTimeSeconds() + Rng.FRandRange(6.f, 18.f);
	}
}

bool ABotController::CanSee(const AFPSCharacter* Other) const
{
	const AFPSCharacter* M = Me();
	if (!M || !Other) return false;
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

	if (Team == EBombTeam::Attackers)
	{
		if (GS->bBombPlanted && G->Bomb)
		{
			// guard the bomb from a spot nearby
			GoTo(RandomNavPointNear(G->Bomb->GetActorLocation(), 900.f));
			return;
		}
		if (GS->bBombDropped && G->Bomb && !M->bHasBomb)
		{
			GoTo(G->Bomb->GetActorLocation(), 60.f);
			return;
		}
		ABombSite* Site = G->AttackSite;
		if (!Site) return;
		TargetSite = Site;
		if (!bWentViaWaypoint && G->Callouts.Num() > 0 && Rng.FRand() < 0.6f)
		{
			// take a detour through a random callout on the way (spreads the bots over the lanes)
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
			// assign by player-state index so the sites get a 3 / 2 split
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

	// ---- combat has priority
	AFPSCharacter* Seen = FindVisibleEnemy();
	if (Seen)
	{
		if (Enemy.Get() != Seen)
		{
			Enemy = Seen;
			EnemySeenSince = Now;
			NextBurstAt = Now + FMath::Lerp(0.55f, 0.15f, Skill);   // reaction time
		}
		LastEnemySeen = Now;
		if (Task != EBotTask::Engage)
		{
			StopMovement();
			M->StopInteract();
			Task = EBotTask::Engage;
			NextStrafeAt = Now;
		}
		// aim at the chest with skill-based error
		const float Err = FMath::Lerp(60.f, 6.f, Skill);
		const FVector AimPt = Seen->GetActorLocation() + FVector(0.f, 0.f, 35.f) + FVector(Rng.FRandRange(-Err, Err), Rng.FRandRange(-Err, Err), Rng.FRandRange(-Err, Err));
		SetFocalPoint(AimPt, EAIFocusPriority::Gameplay);
		// bursts
		if (Now >= NextBurstAt && Now >= BurstEndAt)
		{
			BurstEndAt = Now + Rng.FRandRange(0.25f, 0.6f);
			NextBurstAt = BurstEndAt + Rng.FRandRange(0.15f, 0.45f);
		}
		const bool bShoot = Now < BurstEndAt && Now >= EnemySeenSince + FMath::Lerp(0.5f, 0.12f, Skill);
		if (bShoot) M->StartFire(); else M->StopFire();
		if (M->AmmoInMag == 0) M->Reload();
		// strafe a little
		if (Now >= NextStrafeAt)
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
		// stuck detection
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
		// sprint on long trips
		if (FVector::Dist2D(Pos, ObjectiveLoc) > 1500.f) M->StartSprint(); else M->StopSprint();
		return;
	}
	if (Task == EBotTask::Hold)
	{
		M->StopSprint();
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
