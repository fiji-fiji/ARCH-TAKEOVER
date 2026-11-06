#include "BombGameMode.h"
#include "GameSounds.h"
#include "BombGameState.h"
#include "BombPlayerState.h"
#include "FPSCharacter.h"
#include "FPSPlayerController.h"
#include "BotController.h"
#include "BombHUD.h"
#include "BombSite.h"
#include "BombActor.h"
#include "TeamPlayerStart.h"
#include "CalloutMarker.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ParisBombMap.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "NavigationSystem.h"

static const TCHAR* BotNames[] = {
	TEXT("Pierre"), TEXT("Amina"), TEXT("Lucas"), TEXT("Chloe"), TEXT("Karim"), TEXT("Manon"), TEXT("Theo"),
	TEXT("Ines"), TEXT("Nico"), TEXT("Sofia"), TEXT("Hugo"), TEXT("Lea"), TEXT("Yanis"), TEXT("Zoe")
};

ABombGameMode::ABombGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameStateClass = ABombGameState::StaticClass();
	PlayerStateClass = ABombPlayerState::StaticClass();
	DefaultPawnClass = AFPSCharacter::StaticClass();
	PlayerControllerClass = AFPSPlayerController::StaticClass();
	HUDClass = ABombHUD::StaticClass();
	Rng.Initialize(20251106);
}

ABombGameState* ABombGameMode::GS() const
{
	return GetGameState<ABombGameState>();
}

void ABombGameMode::CollectWorldActors()
{
	Starts.Empty(); Sites.Empty(); Callouts.Empty();
	for (TActorIterator<ATeamPlayerStart> It(GetWorld()); It; ++It) Starts.Add(*It);
	for (TActorIterator<ABombSite> It(GetWorld()); It; ++It) Sites.Add(*It);
	for (TActorIterator<ACalloutMarker> It(GetWorld()); It; ++It) Callouts.Add(*It);
	Sites.Sort([](const ABombSite& A, const ABombSite& B) { return A.SiteId < B.SiteId; });
	UE_LOG(LogParisBomb, Log, TEXT("World: %d team starts, %d bomb sites, %d callouts"), Starts.Num(), Sites.Num(), Callouts.Num());
}

ATeamPlayerStart* ABombGameMode::StartFor(EBombTeam Team, int32 Index) const
{
	TArray<ATeamPlayerStart*> Mine;
	for (ATeamPlayerStart* S : Starts)
	{
		if (S && S->TeamId == (int32)Team) Mine.Add(S);
	}
	if (Mine.Num() == 0) return nullptr;
	return Mine[Index % Mine.Num()];
}

AActor* ABombGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (Starts.Num() == 0) CollectWorldActors();
	ABombPlayerState* PS = Player ? Player->GetPlayerState<ABombPlayerState>() : nullptr;
	if (PS && PS->Team == EBombTeam::None)
	{
		PS->Team = HumanTeam;
	}
	const EBombTeam Team = PS ? PS->Team : HumanTeam;
	if (ATeamPlayerStart* S = StartFor(Team, 0))
	{
		return S;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ABombGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (ABombPlayerState* PS = NewPlayer->GetPlayerState<ABombPlayerState>())
	{
		PS->SetPlayerName(TEXT("You"));
		if (PS->Team == EBombTeam::None) PS->Team = HumanTeam;
	}
	if (AFPSCharacter* C = Cast<AFPSCharacter>(NewPlayer->GetPawn()))
	{
		Characters.AddUnique(C);
		C->ApplyTeamVisuals();
	}
}

void ABombGameMode::BeginPlay()
{
	Super::BeginPlay();
	CollectWorldActors();
	if (ABombGameState* S = GS())
	{
		S->HumanTeam = HumanTeam;
		S->RoundNumber = 0;
	}
	// safety net: make sure a nav mesh exists for the bots (runtime generation is Dynamic)
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Nav->Build();
		UE_LOG(LogParisBomb, Log, TEXT("Navigation: build requested, nav data %s"), Nav->GetDefaultNavDataInstance() ? TEXT("present") : TEXT("missing"));
	}
	else
	{
		UE_LOG(LogParisBomb, Warning, TEXT("Navigation: no navigation system in this world"));
	}
	SpawnBots();
	SetPhase(ERoundPhase::Warmup, WarmupTime);
	float Shot = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("pbm_autoshot="), Shot))
	{
		AutoShotAt = Shot;
		UE_LOG(LogParisBomb, Log, TEXT("Auto screenshot scheduled at %.1fs"), Shot);
	}
	for (AFPSCharacter* C : Characters)
	{
		if (C) C->SetFrozen(true);
	}
}

void ABombGameMode::SpawnBots()
{
	for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		const EBombTeam Team = (EBombTeam)TeamIdx;
		int32 Have = 0;
		for (AFPSCharacter* C : Characters)
		{
			if (C && C->GetTeam() == Team) ++Have;
		}
		for (int32 i = Have; i < TeamSize; ++i)
		{
			ATeamPlayerStart* S = StartFor(Team, i);
			if (!S) continue;
			FActorSpawnParameters P;
			P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AFPSCharacter* C = GetWorld()->SpawnActor<AFPSCharacter>(AFPSCharacter::StaticClass(), S->GetActorLocation(), S->GetActorRotation(), P);
			if (!C) continue;
			C->bIsBot = true;
			ABotController* BC = GetWorld()->SpawnActor<ABotController>(ABotController::StaticClass(), S->GetActorLocation(), S->GetActorRotation(), P);
			if (!BC) { C->Destroy(); continue; }
			BC->Possess(C);
			if (ABombPlayerState* PS = BC->GetPlayerState<ABombPlayerState>())
			{
				PS->Team = Team;
				PS->SetIsABot(true);
				PS->SetPlayerName(FString::Printf(TEXT("%s [bot]"), BotNames[BotNameIndex++ % UE_ARRAY_COUNT(BotNames)]));
			}
			C->ApplyTeamVisuals();
			Characters.Add(C);
		}
	}
	UE_LOG(LogParisBomb, Log, TEXT("Spawned bots: %d characters total"), Characters.Num());
}

void ABombGameMode::SetPhase(ERoundPhase Phase, float Duration)
{
	if (ABombGameState* S = GS())
	{
		S->Phase = Phase;
		S->PhaseTimeRemaining = Duration;
	}
}

void ABombGameMode::StartRound()
{
	ABombGameState* S = GS();
	if (!S) return;
	if (bPendingTeamSwitch)
	{
		bPendingTeamSwitch = false;
		HumanTeam = OtherTeam(HumanTeam);
		S->HumanTeam = HumanTeam;
		// move the human to the other team and hand one bot the other way
		AFPSCharacter* H = HumanCharacter();
		ABombPlayerState* HPS = H ? H->GetPlayerState<ABombPlayerState>() : nullptr;
		if (HPS)
		{
			for (AFPSCharacter* C : Characters)
			{
				ABombPlayerState* PS = C ? C->GetPlayerState<ABombPlayerState>() : nullptr;
				if (C && C->bIsBot && PS && PS->Team == HumanTeam)
				{
					PS->Team = OtherTeam(HumanTeam);
					break;
				}
			}
			HPS->Team = HumanTeam;
		}
	}
	S->RoundNumber++;
	S->bBombPlanted = false;
	S->bBombDropped = false;
	S->BombTimeRemaining = 0.f;
	S->PlantedSiteId.Empty();
	S->RoundResultText.Empty();
	PlantedSite = nullptr;
	if (Bomb) { Bomb->Destroy(); Bomb = nullptr; }
	AttackSite = Sites.Num() > 0 ? Sites[Rng.RandRange(0, Sites.Num() - 1)] : nullptr;

	// respawn everyone at their team start
	int32 Idx[2] = {0, 0};
	TArray<AFPSCharacter*> Attackers;
	for (AFPSCharacter* C : Characters)
	{
		if (!C) continue;
		const EBombTeam Team = C->GetTeam();
		const int32 T = Team == EBombTeam::Attackers ? 0 : 1;
		ATeamPlayerStart* St = StartFor(Team, Idx[T]++);
		const FTransform X = St ? St->GetActorTransform() : C->GetActorTransform();
		C->ResetForRound(X);
		C->SetFrozen(true);
		if (Team == EBombTeam::Attackers) Attackers.Add(C);
		if (ABotController* BC = Cast<ABotController>(C->GetController()))
		{
			BC->OnRoundStart();
		}
	}
	if (Attackers.Num() > 0)
	{
		AFPSCharacter* Carrier = Attackers[Rng.RandRange(0, Attackers.Num() - 1)];
		Carrier->bHasBomb = true;
		S->BombCarrierName = Carrier->GetPlayerState() ? Carrier->GetPlayerState()->GetPlayerName() : TEXT("?");
	}
	if (APlayerController* PC = HumanController())
	{
		if (APawn* P = PC->GetPawn()) PC->SetViewTarget(P);
	}
	UpdateAliveCounts();
	SetPhase(ERoundPhase::Freeze, FreezeTime);
	S->AddFeed(FString::Printf(TEXT("Round %d"), S->RoundNumber), FLinearColor::White);
	GameSounds::UI(this, TEXT("S_RoundStart"), 0.8f);
}

void ABombGameMode::EndRound(EBombTeam Winner, const FString& Reason)
{
	ABombGameState* S = GS();
	if (!S || S->Phase != ERoundPhase::Live) return;
	if (Winner == EBombTeam::Attackers) S->ScoreAttackers++; else S->ScoreDefenders++;
	S->RoundResultText = FString::Printf(TEXT("%s WIN - %s"), *TeamName(Winner), *Reason);
	S->AddFeed(S->RoundResultText, TeamColor(Winner));
	GameSounds::UI(this, Winner == HumanTeam ? TEXT("S_RoundWin") : TEXT("S_RoundLose"), 0.8f);
	for (AFPSCharacter* C : Characters)
	{
		if (C && !C->bDead) { C->StopFire(); C->StopInteract(); }
	}
	const bool bMatchOver = S->ScoreAttackers >= RoundsToWin || S->ScoreDefenders >= RoundsToWin;
	if (bMatchOver)
	{
		const EBombTeam MatchWinner = S->ScoreAttackers > S->ScoreDefenders ? EBombTeam::Attackers : EBombTeam::Defenders;
		S->MatchResultText = FString::Printf(TEXT("%s WIN THE MATCH  %d - %d"), *TeamName(MatchWinner), S->ScoreAttackers, S->ScoreDefenders);
		SetPhase(ERoundPhase::MatchOver, MatchOverTime);
		GameSounds::UI(this, TEXT("S_MatchEnd"), 0.9f);
		return;
	}
	if (S->RoundNumber == HalfTimeRound)
	{
		S->RoundResultText += TEXT("   |   HALF-TIME: SWITCHING SIDES");
		SwapSides();
	}
	SetPhase(ERoundPhase::PostRound, PostRoundTime);
}

void ABombGameMode::SwapSides()
{
	ABombGameState* S = GS();
	for (AFPSCharacter* C : Characters)
	{
		if (ABombPlayerState* PS = C ? C->GetPlayerState<ABombPlayerState>() : nullptr)
		{
			PS->Team = OtherTeam(PS->Team);
		}
	}
	HumanTeam = OtherTeam(HumanTeam);
	if (S)
	{
		S->HumanTeam = HumanTeam;
		Swap(S->ScoreAttackers, S->ScoreDefenders);
	}
}

void ABombGameMode::ResetMatch()
{
	if (ABombGameState* S = GS())
	{
		S->ScoreAttackers = 0;
		S->ScoreDefenders = 0;
		S->RoundNumber = 0;
		S->MatchResultText.Empty();
		for (AFPSCharacter* C : Characters)
		{
			if (ABombPlayerState* PS = C ? C->GetPlayerState<ABombPlayerState>() : nullptr)
			{
				PS->Kills = 0; PS->Deaths = 0; PS->Plants = 0; PS->Defuses = 0;
			}
		}
	}
}

int32 ABombGameMode::CountAlive(EBombTeam Team) const
{
	int32 N = 0;
	for (AFPSCharacter* C : Characters)
	{
		if (C && !C->bDead && C->GetTeam() == Team) ++N;
	}
	return N;
}

void ABombGameMode::UpdateAliveCounts()
{
	if (ABombGameState* S = GS())
	{
		S->AliveAttackers = CountAlive(EBombTeam::Attackers);
		S->AliveDefenders = CountAlive(EBombTeam::Defenders);
	}
}

AFPSCharacter* ABombGameMode::HumanCharacter() const
{
	for (AFPSCharacter* C : Characters)
	{
		if (C && !C->bIsBot) return C;
	}
	return nullptr;
}

APlayerController* ABombGameMode::HumanController() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

void ABombGameMode::OnCharacterDied(AFPSCharacter* Victim, AController* Killer)
{
	ABombGameState* S = GS();
	if (!S) return;
	if (ABombPlayerState* VPS = Victim->GetPlayerState<ABombPlayerState>()) VPS->Deaths++;
	FString KillerName = TEXT("the world");
	if (Killer)
	{
		if (ABombPlayerState* KPS = Killer->GetPlayerState<ABombPlayerState>())
		{
			KPS->Kills++;
			KillerName = KPS->GetPlayerName();
		}
	}
	const FString VictimName = Victim->GetPlayerState() ? Victim->GetPlayerState()->GetPlayerName() : TEXT("?");
	S->AddFeed(FString::Printf(TEXT("%s  >  %s"), *KillerName, *VictimName), TeamColor(Victim->GetTeam()));

	if (Victim->bHasBomb)
	{
		DropBomb(Victim);
	}
	UpdateAliveCounts();
	if (S->Phase == ERoundPhase::Live)
	{
		if (S->AliveDefenders == 0)
		{
			EndRound(EBombTeam::Attackers, TEXT("defenders eliminated"));
		}
		else if (S->AliveAttackers == 0 && !S->bBombPlanted)
		{
			EndRound(EBombTeam::Defenders, TEXT("attackers eliminated"));
		}
	}
}

void ABombGameMode::UpdateSpectating()
{
	AFPSCharacter* H = HumanCharacter();
	APlayerController* PC = HumanController();
	if (!H || !PC || !H->bDead) return;
	AActor* Current = PC->GetViewTarget();
	AFPSCharacter* CurChar = Cast<AFPSCharacter>(Current);
	if (CurChar && !CurChar->bDead && CurChar != H) return;
	for (AFPSCharacter* C : Characters)
	{
		if (C && !C->bDead && C->GetTeam() == H->GetTeam())
		{
			PC->SetViewTargetWithBlend(C, 0.5f);
			return;
		}
	}
	for (AFPSCharacter* C : Characters)
	{
		if (C && !C->bDead)
		{
			PC->SetViewTargetWithBlend(C, 0.5f);
			return;
		}
	}
}

void ABombGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ABombGameState* S = GS();
	if (!S) return;
	if (AutoShotAt >= 0.f)
	{
		const float T = GetWorld()->GetTimeSeconds();
		if (!bAutoShotTaken && T >= AutoShotAt - 0.5f && FParse::Param(FCommandLine::Get(), TEXT("pbm_lookmate")))
		{
			// dev: turn the human towards the nearest living teammate so the screenshot shows a soldier model
			AFPSCharacter* H = HumanCharacter(); APlayerController* PC = HumanController();
			if (H && PC)
			{
				AFPSCharacter* Best = nullptr; float BestD = 1e9f;
				for (AFPSCharacter* C : Characters)
				{
					if (!C || C == H || C->bDead || C->GetTeam() != H->GetTeam()) continue;
					const float D = FVector::Dist(C->GetActorLocation(), H->GetActorLocation());
					if (D < BestD) { BestD = D; Best = C; }
				}
				if (Best) PC->SetControlRotation((Best->GetActorLocation() + FVector(0, 0, 20.f) - H->GetEyeLocation()).Rotation());
			}
		}
		if (!bAutoShotTaken && T >= AutoShotAt)
		{
			bAutoShotTaken = true;
			FScreenshotRequest::RequestScreenshot(FString(TEXT("autoshot.png")), false, false);
			UE_LOG(LogParisBomb, Log, TEXT("Auto screenshot taken at %.1fs; alive %d v %d"), T, S->AliveAttackers, S->AliveDefenders);
			for (AFPSCharacter* C : Characters)
			{
				if (!C) continue;
				const APlayerState* PS = C->GetPlayerState();
				UE_LOG(LogParisBomb, Log, TEXT("  %-16s team %d at (%.0f, %.0f) speed %.0f dead %d bomb %d"),
					PS ? *PS->GetPlayerName() : TEXT("?"), (int32)C->GetTeam(), C->GetActorLocation().X, C->GetActorLocation().Y,
					C->GetVelocity().Size(), C->bDead ? 1 : 0, C->bHasBomb ? 1 : 0);
			}
		}
		if (bAutoShotTaken && T >= AutoShotAt + 4.f)
		{
			FPlatformMisc::RequestExit(false);
		}
	}
	S->PhaseTimeRemaining -= DeltaSeconds;
	const float Now = GetWorld()->GetTimeSeconds();
	S->KillFeed.RemoveAll([Now](const ABombGameState::FFeedEntry& E) { return E.Expires < Now; });

	switch (S->Phase)
	{
	case ERoundPhase::Warmup:
		if (S->PhaseTimeRemaining <= 0.f) StartRound();
		break;
	case ERoundPhase::Freeze:
		if (S->PhaseTimeRemaining <= 0.f)
		{
			for (AFPSCharacter* C : Characters) if (C) C->SetFrozen(false);
			SetPhase(ERoundPhase::Live, RoundTime);
			S->AddFeed(TEXT("GO GO GO"), FLinearColor::Yellow);
		}
		break;
	case ERoundPhase::Live:
		if (S->bBombPlanted)
		{
			S->BombTimeRemaining -= DeltaSeconds;
			if (S->BombTimeRemaining <= 0.f)
			{
				S->BombTimeRemaining = 0.f;
				// everyone close to the bomb dies in the blast
				if (Bomb)
				{
					for (AFPSCharacter* C : Characters)
					{
						if (C && !C->bDead && FVector::Dist(C->GetActorLocation(), Bomb->GetActorLocation()) < 1800.f)
						{
							C->Die(nullptr, Bomb);
						}
					}
				}
				if (Bomb) GameSounds::At(this, TEXT("S_Explosion"), Bomb->GetActorLocation(), 2.0f);
				EndRound(EBombTeam::Attackers, TEXT("the bomb exploded"));
			}
		}
		else if (S->PhaseTimeRemaining <= 0.f)
		{
			EndRound(EBombTeam::Defenders, TEXT("time ran out"));
		}
		UpdateSpectating();
		break;
	case ERoundPhase::PostRound:
		if (S->PhaseTimeRemaining <= 0.f) StartRound();
		break;
	case ERoundPhase::MatchOver:
		if (S->PhaseTimeRemaining <= 0.f)
		{
			ResetMatch();
			StartRound();
		}
		break;
	}
}

// ---------------------------------------------------------------- bomb

void ABombGameMode::TickInteract(AFPSCharacter* C, float DeltaSeconds)
{
	ABombGameState* S = GS();
	if (!S || !C || C->bDead || S->Phase != ERoundPhase::Live) return;
	const EBombTeam Team = C->GetTeam();
	if (Team == EBombTeam::Attackers && C->bHasBomb && !S->bBombPlanted)
	{
		for (ABombSite* Site : Sites)
		{
			if (Site && Site->IsInside(C->GetActorLocation()))
			{
				C->InteractProgress += DeltaSeconds / PlantTime;
				if (!C->bIsBot && FMath::FloorToInt(C->InteractProgress * 8.f) != FMath::FloorToInt((C->InteractProgress - DeltaSeconds / PlantTime) * 8.f)) GameSounds::UI(this, TEXT("S_Tick"), 0.5f);
				C->InteractHint = FString::Printf(TEXT("PLANTING AT %s"), *Site->SiteId);
				if (C->InteractProgress >= 1.f)
				{
					PlantBomb(C, Site);
				}
				return;
			}
		}
		C->InteractProgress = 0.f;
		return;
	}
	if (Team == EBombTeam::Defenders && S->bBombPlanted && Bomb)
	{
		if (FVector::Dist(C->GetActorLocation(), Bomb->GetActorLocation()) < 220.f)
		{
			C->InteractProgress += DeltaSeconds / DefuseTime;
			if (!C->bIsBot && FMath::FloorToInt(C->InteractProgress * 8.f) != FMath::FloorToInt((C->InteractProgress - DeltaSeconds / DefuseTime) * 8.f)) GameSounds::UI(this, TEXT("S_Tick"), 0.5f);
			C->InteractHint = TEXT("DEFUSING");
			Bomb->DefuseProgress = C->InteractProgress;
			if (C->InteractProgress >= 1.f)
			{
				DefuseBomb(C);
			}
			return;
		}
	}
	C->InteractProgress = 0.f;
}

void ABombGameMode::PlantBomb(AFPSCharacter* Planter, ABombSite* Site)
{
	ABombGameState* S = GS();
	if (!S || S->bBombPlanted) return;
	Planter->bHasBomb = false;
	Planter->StopInteract();
	if (ABombPlayerState* PS = Planter->GetPlayerState<ABombPlayerState>()) PS->Plants++;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Loc = Planter->GetActorLocation();
	Loc.Z -= Planter->GetDefaultHalfHeight() - 10.f;
	if (Bomb) Bomb->Destroy();
	Bomb = GetWorld()->SpawnActor<ABombActor>(ABombActor::StaticClass(), Loc, FRotator(0.f, Planter->GetActorRotation().Yaw, 0.f), P);
	if (Bomb) Bomb->SetPlanted(true);
	PlantedSite = Site;
	S->bBombPlanted = true;
	S->bBombDropped = false;
	S->BombTimeRemaining = BombTime;
	S->PlantedSiteId = Site->SiteId;
	S->BombCarrierName.Empty();
	S->AddFeed(FString::Printf(TEXT("BOMB PLANTED AT %s"), *Site->SiteId), FLinearColor::Red);
	GameSounds::UI(this, TEXT("S_BombPlanted"), 0.9f);
	for (AFPSCharacter* C : Characters)
	{
		if (ABotController* BC = C ? Cast<ABotController>(C->GetController()) : nullptr) { /* bots re-plan on their next think */ }
	}
}

void ABombGameMode::DefuseBomb(AFPSCharacter* Defuser)
{
	ABombGameState* S = GS();
	if (!S || !S->bBombPlanted) return;
	Defuser->StopInteract();
	if (ABombPlayerState* PS = Defuser->GetPlayerState<ABombPlayerState>()) PS->Defuses++;
	S->bBombPlanted = false;
	if (Bomb) { Bomb->SetPlanted(false); Bomb->Destroy(); Bomb = nullptr; }
	GameSounds::UI(this, TEXT("S_BombDefused"), 0.9f);
	EndRound(EBombTeam::Defenders, TEXT("bomb defused"));
}

void ABombGameMode::DropBomb(AFPSCharacter* Carrier)
{
	ABombGameState* S = GS();
	if (!S) return;
	Carrier->bHasBomb = false;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector Loc = Carrier->GetActorLocation();
	Loc.Z -= Carrier->GetDefaultHalfHeight() - 12.f;
	if (Bomb) Bomb->Destroy();
	Bomb = GetWorld()->SpawnActor<ABombActor>(ABombActor::StaticClass(), Loc, FRotator::ZeroRotator, P);
	if (Bomb) Bomb->SetPlanted(false);
	S->bBombDropped = true;
	S->BombCarrierName = TEXT("(dropped)");
	S->AddFeed(TEXT("Bomb dropped"), FLinearColor::Yellow);
}

void ABombGameMode::PickupBomb(AFPSCharacter* C)
{
	ABombGameState* S = GS();
	if (!S || !Bomb || Bomb->bPlanted || !C || C->bDead) return;
	Bomb->Destroy();
	Bomb = nullptr;
	C->bHasBomb = true;
	S->bBombDropped = false;
	S->BombCarrierName = C->GetPlayerState() ? C->GetPlayerState()->GetPlayerName() : TEXT("?");
	S->AddFeed(FString::Printf(TEXT("%s picked up the bomb"), *S->BombCarrierName), FLinearColor::Yellow);
}

void ABombGameMode::RequestHumanTeamSwitch()
{
	bPendingTeamSwitch = !bPendingTeamSwitch;
	if (ABombGameState* S = GS())
	{
		S->AddFeed(bPendingTeamSwitch ? TEXT("You will switch team next round (M to cancel)") : TEXT("Team switch cancelled"), FLinearColor::White);
	}
}

FString ABombGameMode::NearestCallout(const FVector& Loc) const
{
	float Best = 1e12f;
	FString Name;
	for (const ACalloutMarker* M : Callouts)
	{
		if (!M) continue;
		const float D = FVector::DistSquared2D(M->GetActorLocation(), Loc);
		if (D < Best) { Best = D; Name = M->CalloutName; }
	}
	return Best < FMath::Square(6500.f) ? Name : FString();
}
