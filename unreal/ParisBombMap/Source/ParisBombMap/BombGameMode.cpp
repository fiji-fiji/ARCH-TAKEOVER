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
#include "GrenadeActor.h"
#include "WeaponPickup.h"
#include "Weapons.h"
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

extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;

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

ABombPlayerState* ABombGameMode::PSOf(const AFPSCharacter* C)
{
	return C ? C->GetPlayerState<ABombPlayerState>() : nullptr;
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

bool ABombGameMode::SpawnFence(EBombTeam Team, FVector& OutCenter, float& OutRadius) const
{
	FVector Sum = FVector::ZeroVector; int32 N = 0;
	for (ATeamPlayerStart* S : Starts)
	{
		if (S && S->TeamId == (int32)Team) { Sum += S->GetActorLocation(); ++N; }
	}
	if (N == 0) return false;
	OutCenter = Sum / N;
	OutRadius = SpawnFenceRadius;
	return true;
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
		PS->Money = Weapons::StartMoney;
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
				PS->Money = Weapons::StartMoney;
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

void ABombGameMode::ClearRoundActors()
{
	TArray<AActor*> Gone;
	for (TActorIterator<AWeaponPickup> It(GetWorld()); It; ++It) Gone.Add(*It);
	for (TActorIterator<AGrenadeActor> It(GetWorld()); It; ++It) Gone.Add(*It);
	for (TActorIterator<ASmokeCloud> It(GetWorld()); It; ++It) Gone.Add(*It);
	for (AActor* A : Gone) A->Destroy();
}

bool ABombGameMode::IsPistolRound() const
{
	const ABombGameState* S = GS();
	return S && (S->RoundNumber == 1 || S->RoundNumber == HalfTimeRound + 1);
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
	const bool bHalftime = S->RoundNumber == HalfTimeRound + 1;
	const bool bOvertime = bWinByTwo && S->RoundNumber > HalfTimeRound * 2;
	S->bOvertime = bOvertime;
	if (bHalftime || bOvertime)
	{
		SwapSides();
	}
	S->bBombPlanted = false;
	S->bBombDropped = false;
	S->BombTimeRemaining = 0.f;
	S->PlantedSiteId.Empty();
	S->RoundResultText.Empty();
	PlantedSite = nullptr;
	if (Bomb) { Bomb->Destroy(); Bomb = nullptr; }
	ClearRoundActors();
	AttackSite = Sites.Num() > 0 ? Sites[Rng.RandRange(0, Sites.Num() - 1)] : nullptr;
	if (bHalftime) { LossStreak[0] = LossStreak[1] = 0; }

	// respawn everyone at their team start; the dead (and everyone at halftime) are back to the pistol
	int32 Idx[2] = {0, 0};
	TArray<AFPSCharacter*> Attackers;
	for (AFPSCharacter* C : Characters)
	{
		if (!C) continue;
		ABombPlayerState* PS = PSOf(C);
		const EBombTeam Team = C->GetTeam();
		const int32 T = Team == EBombTeam::Attackers ? 0 : 1;
		ATeamPlayerStart* St = StartFor(Team, Idx[T]++);
		const FTransform X = St ? St->GetActorTransform() : C->GetActorTransform();
		if (bHalftime)
		{
			C->ResetLoadout();
			if (PS) PS->Money = Weapons::StartMoney;
		}
		else if (!PS || !PS->bSurvived)
		{
			C->ResetLoadout();
		}
		C->RefillLoadout();
		C->ResetForRound(X);
		C->SetFrozen(true);
		if (PS) PS->bSurvived = false;
		if (Team == EBombTeam::Attackers) Attackers.Add(C);
		if (ABotController* BC = Cast<ABotController>(C->GetController()))
		{
			BC->OnRoundStart();
		}
	}
	// the bomb: if you attack, it waits on the ground next to you; otherwise a random attacker carries it
	AFPSCharacter* H = HumanCharacter();
	bBombForHuman = H && !H->bDead && H->GetTeam() == EBombTeam::Attackers;
	bBotsMayTakeBomb = !bBombForHuman;
	if (bBombForHuman)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector Loc = H->GetActorLocation() + H->GetActorForwardVector() * 150.f;
		Loc.Z -= H->GetDefaultHalfHeight() - 12.f;
		Bomb = GetWorld()->SpawnActor<ABombActor>(ABombActor::StaticClass(), Loc, FRotator::ZeroRotator, P);
		if (Bomb) Bomb->SetPlanted(false);
		S->bBombDropped = true;
		S->BombCarrierName = TEXT("(on the ground next to you)");
	}
	else if (Attackers.Num() > 0)
	{
		AFPSCharacter* Carrier = Attackers[Rng.RandRange(0, Attackers.Num() - 1)];
		Carrier->OnBombPickedUp();
		S->BombCarrierName = Carrier->GetPlayerState() ? Carrier->GetPlayerState()->GetPlayerName() : TEXT("?");
	}
	if (APlayerController* PC = HumanController())
	{
		if (APawn* P = PC->GetPawn()) PC->SetViewTarget(P);
	}
	// the human may walk around inside the spawn fence and shop during the buy phase
	if (H && !H->bDead) H->SetFrozen(false);
	UpdateAliveCounts();
	SetPhase(ERoundPhase::Freeze, S->RoundNumber == 1 ? FirstRoundBuyTime : BuyTime);
	// bots spend their money (the shop is open now that the phase is Freeze)
	for (AFPSCharacter* C : Characters)
	{
		if (ABotController* BC = C ? Cast<ABotController>(C->GetController()) : nullptr) BC->BuyPhase();
	}
	FString Title = FString::Printf(TEXT("Round %d"), S->RoundNumber);
	if (bOvertime) Title += TEXT("  OVERTIME");
	if (bHalftime) Title += TEXT("  HALF-TIME: sides switched");
	S->AddFeed(Title, FLinearColor::White);
	GameSounds::UI(this, TEXT("S_RoundStart"), 0.8f);
}

void ABombGameMode::EndRound(EBombTeam Winner, const FString& Reason)
{
	ABombGameState* S = GS();
	if (!S || S->Phase != ERoundPhase::Live) return;
	const EBombTeam Loser = OtherTeam(Winner);
	if (Winner == EBombTeam::Attackers) S->ScoreAttackers++; else S->ScoreDefenders++;
	const int32 WScore = Winner == EBombTeam::Attackers ? S->ScoreAttackers : S->ScoreDefenders;
	const int32 LScore = Winner == EBombTeam::Attackers ? S->ScoreDefenders : S->ScoreAttackers;
	S->RoundResultText = FString::Printf(TEXT("%s WIN - %s"), *TeamName(Winner), *Reason);
	S->AddFeed(S->RoundResultText, TeamColor(Winner));
	GameSounds::UI(this, Winner == HumanTeam ? TEXT("S_RoundWin") : TEXT("S_RoundLose"), 0.8f);

	// money for the next round
	const int32 LossPay = Weapons::LossMoney[FMath::Min(LossStreak[(int32)Loser], 2)];
	LossStreak[(int32)Loser]++;
	LossStreak[(int32)Winner] = 0;
	for (AFPSCharacter* C : Characters)
	{
		if (!C) continue;
		if (ABombPlayerState* PS = PSOf(C)) PS->bSurvived = !C->bDead;
		Pay(C, C->GetTeam() == Winner ? Weapons::WinMoney : LossPay);
		if (!C->bDead) { C->StopFire(); C->StopInteract(); }
	}

	const bool bMatchOver = WScore >= RoundsToWin && (!bWinByTwo || WScore - LScore >= 2);
	if (bMatchOver)
	{
		S->MatchResultText = FString::Printf(TEXT("%s WIN THE MATCH  %d - %d"), *TeamName(Winner), S->ScoreAttackers, S->ScoreDefenders);
		SetPhase(ERoundPhase::MatchOver, MatchOverTime);
		GameSounds::UI(this, TEXT("S_MatchEnd"), 0.9f);
		return;
	}
	if (S->RoundNumber == HalfTimeRound)
	{
		S->RoundResultText += TEXT("   |   HALF-TIME: SWITCHING SIDES");
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
	Swap(LossStreak[0], LossStreak[1]);
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
		S->bOvertime = false;
		S->MatchResultText.Empty();
		LossStreak[0] = LossStreak[1] = 0;
		for (AFPSCharacter* C : Characters)
		{
			if (ABombPlayerState* PS = C ? C->GetPlayerState<ABombPlayerState>() : nullptr)
			{
				PS->Kills = 0; PS->Deaths = 0; PS->Plants = 0; PS->Defuses = 0;
				PS->Money = Weapons::StartMoney; PS->bSurvived = false;
			}
			if (C) C->ResetLoadout();
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

// ---------------------------------------------------------------- economy

void ABombGameMode::Pay(AFPSCharacter* C, int32 Amount)
{
	ABombPlayerState* PS = PSOf(C);
	if (!PS || Amount <= 0) return;
	PS->Money = FMath::Min(Weapons::MaxMoney, PS->Money + Amount);
	PS->LastGain = Amount;
	PS->LastGainTime = GetWorld()->GetTimeSeconds();
	if (C && !C->bIsBot) GameSounds::UI(this, TEXT("S_Money"), 0.5f);
}

bool ABombGameMode::CanBuy(const AFPSCharacter* C) const
{
	const ABombGameState* S = GS();
	return S && S->Phase == ERoundPhase::Freeze && C && !C->bDead;
}

int32 ABombGameMode::MinNextMoney(const AFPSCharacter* C) const
{
	const ABombGameState* S = GS();
	const ABombPlayerState* PS = PSOf(C);
	if (!S || !PS) return 0;
	if (S->RoundNumber == HalfTimeRound) return Weapons::StartMoney;          // halftime: everyone restarts
	const int32 LossPay = Weapons::LossMoney[FMath::Min(LossStreak[(int32)C->GetTeam()], 2)];
	return FMath::Min(Weapons::MaxMoney, PS->Money + LossPay);
}

bool ABombGameMode::CanAfford(const AFPSCharacter* C, FName Item) const
{
	const ABombPlayerState* PS = PSOf(C);
	if (!PS) return false;
	const int32 Price = Weapons::PriceOf(Item);
	if (Weapons::FindGrenade(Item)) return PS->Money >= Price;
	FName Was = NAME_None;
	if (const FWeaponDef* W = Weapons::Find(Item)) Was = W->Slot == EWeaponSlot::Main ? PS->RoundBuyMain : PS->RoundBuySide;
	else if (Weapons::FindArmor(Item)) Was = PS->RoundBuyArmor;
	const int32 Refund = Was.IsNone() ? 0 : Weapons::PriceOf(Was);     // bought this phase: refunded
	return PS->Money + Refund >= Price;
}

bool ABombGameMode::Buy(AFPSCharacter* C, FName Item)
{
	ABombPlayerState* PS = PSOf(C);
	if (!PS || !CanBuy(C) || !CanAfford(C, Item)) return false;
	const int32 Price = Weapons::PriceOf(Item);
	if (const FGrenadeDef* G = Weapons::FindGrenade(Item))
	{
		if (C->NadeCount(Item) >= G->Max) return false;
		PS->Money -= Price;
		C->AddNade(Item, +1);
		PS->RoundBuyNades.FindOrAdd(Item)++;
	}
	else if (const FWeaponDef* W = Weapons::Find(Item))
	{
		if (W->Slot == EWeaponSlot::Knife) return false;
		const int32 Idx = (int32)W->Slot;
		if (C->Guns[Idx].IsValid() && C->Guns[Idx].Def == W) return false;    // already holding it
		FName& Rb = W->Slot == EWeaponSlot::Main ? PS->RoundBuyMain : PS->RoundBuySide;
		if (!Rb.IsNone()) PS->Money += Weapons::PriceOf(Rb);
		PS->Money -= Price;
		C->GiveWeapon(Item);
		Rb = Price > 0 ? Item : NAME_None;
	}
	else if (const FArmorDef* A = Weapons::FindArmor(Item))
	{
		if (PS->RoundBuyArmor.IsNone()) PS->PrevArmor = C->Armor;
		else
		{
			PS->Money += Weapons::PriceOf(PS->RoundBuyArmor);
			if (const FArmorDef* Prev = Weapons::FindArmor(PS->RoundBuyArmor)) C->Armor = FMath::Max(0, C->Armor - Prev->Amount);
		}
		if (C->Armor >= A->Amount)
		{
			// already at least that much: undo the refund and refuse
			if (!PS->RoundBuyArmor.IsNone())
			{
				PS->Money -= Weapons::PriceOf(PS->RoundBuyArmor);
				if (const FArmorDef* Prev = Weapons::FindArmor(PS->RoundBuyArmor)) C->Armor = Prev->Amount;
			}
			return false;
		}
		PS->Money -= Price;
		C->Armor = A->Amount;
		PS->RoundBuyArmor = Item;
	}
	else
	{
		return false;
	}
	PS->Money = FMath::Clamp(PS->Money, 0, Weapons::MaxMoney);
	if (!C->bIsBot) GameSounds::UI(this, TEXT("S_Buy"), 0.6f);
	return true;
}

bool ABombGameMode::Sell(AFPSCharacter* C, FName Item)
{
	ABombPlayerState* PS = PSOf(C);
	if (!PS || !CanBuy(C)) return false;
	const int32 Price = Weapons::PriceOf(Item);
	if (Weapons::FindGrenade(Item))
	{
		int32* N = PS->RoundBuyNades.Find(Item);
		if (!N || *N <= 0 || C->NadeCount(Item) <= 0) return false;
		(*N)--;
		C->AddNade(Item, -1);
		if (C->Held == EHeldSlot::Nade && C->NadeCount(C->NadeKind) <= 0) C->SwitchTo(C->HasMain() ? EHeldSlot::Main : EHeldSlot::Side);
		PS->Money += Price;
	}
	else if (const FWeaponDef* W = Weapons::Find(Item))
	{
		FName& Rb = W->Slot == EWeaponSlot::Main ? PS->RoundBuyMain : PS->RoundBuySide;
		if (Rb != Item) return false;
		PS->Money += Price;
		Rb = NAME_None;
		if (W->Slot == EWeaponSlot::Main) { C->Guns[0].Clear(); if (C->Held == EHeldSlot::Main) C->Held = EHeldSlot::Side; }
		else { C->Guns[1].Set(Weapons::Find(TEXT("p9"))); }
		C->ApplyHeldVisuals();
	}
	else if (Weapons::FindArmor(Item))
	{
		if (PS->RoundBuyArmor != Item) return false;
		PS->Money += Price;
		C->Armor = PS->PrevArmor;
		PS->RoundBuyArmor = NAME_None;
	}
	else
	{
		return false;
	}
	PS->Money = FMath::Clamp(PS->Money, 0, Weapons::MaxMoney);
	if (!C->bIsBot) GameSounds::UI(this, TEXT("S_Sell"), 0.6f);
	return true;
}

// ---------------------------------------------------------------- deaths

void ABombGameMode::OnCharacterDied(AFPSCharacter* Victim, AController* Killer)
{
	ABombGameState* S = GS();
	if (!S) return;
	if (ABombPlayerState* VPS = Victim->GetPlayerState<ABombPlayerState>()) VPS->Deaths++;
	FString KillerName = TEXT("the world");
	AFPSCharacter* KC = Killer ? Cast<AFPSCharacter>(Killer->GetPawn()) : nullptr;
	if (Killer)
	{
		if (ABombPlayerState* KPS = Killer->GetPlayerState<ABombPlayerState>())
		{
			KPS->Kills++;
			KillerName = KPS->GetPlayerName();
		}
		if (KC && KC != Victim && KC->GetTeam() != Victim->GetTeam()) Pay(KC, Weapons::KillMoney);
	}
	const FString VictimName = Victim->GetPlayerState() ? Victim->GetPlayerState()->GetPlayerName() : TEXT("?");
	const FString Weapon = Victim->LastHitWeapon.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" [%s]"), *Victim->LastHitWeapon);
	S->AddFeed(FString::Printf(TEXT("%s%s%s  >  %s"), *KillerName, *Weapon, Victim->bLastHitHeadshot ? TEXT(" (HS)") : TEXT(""), *VictimName), TeamColor(Victim->GetTeam()));
	UE_LOG(LogParisBomb, Log, TEXT("Kill: %s%s%s > %s"), *KillerName, *Weapon, Victim->bLastHitHeadshot ? TEXT(" (HS)") : TEXT(""), *VictimName);

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

void ABombGameMode::UpdateBotsMayTake()
{
	// your bot teammates leave the bomb for you; they only go get it if you left spawn without it,
	// 10 s passed after GO, or you died
	ABombGameState* S = GS();
	bool bMay = true;
	if (bBombForHuman && S && S->bBombDropped)
	{
		AFPSCharacter* H = HumanCharacter();
		bMay = false;
		if (S->Phase == ERoundPhase::Live && H)
		{
			FVector Center; float Radius;
			const bool bInSpawn = SpawnFence(EBombTeam::Attackers, Center, Radius) && FVector::Dist2D(H->GetActorLocation(), Center) <= Radius + 300.f;
			bMay = H->bDead || H->bHadBomb || !bInSpawn || GetWorld()->GetTimeSeconds() - LiveStartTime > 10.f;
		}
	}
	bBotsMayTakeBomb = bMay;
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
		FString Tele;
		if (!bAutoShotTaken && T >= AutoShotAt - 0.6f && FParse::Value(FCommandLine::Get(), TEXT("pbm_teleport="), Tele, false))
		{
			// dev: -pbm_teleport=X,Y,Z,Yaw,Pitch (UE cm / degrees) places the human for the screenshot
			TArray<FString> Parts; Tele.ParseIntoArray(Parts, TEXT(","));
			AFPSCharacter* H = HumanCharacter(); APlayerController* PC = HumanController();
			if (H && PC && Parts.Num() >= 4)
			{
				const FVector Loc(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
				const FRotator Rot(Parts.Num() >= 5 ? FCString::Atof(*Parts[4]) : 0.f, FCString::Atof(*Parts[3]), 0.f);
				H->bFenceExempt = true;
				H->TeleportTo(Loc, Rot, false, true);
				PC->SetControlRotation(Rot);
			}
		}
		if (!bAutoShotTaken && T >= AutoShotAt - 1.5f && FParse::Param(FCommandLine::Get(), TEXT("pbm_openshop")))
		{
			if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(HumanController())) PC->OpenShop();
		}
		if (!bAutoShotTaken && T >= AutoShotAt)
		{
			bAutoShotTaken = true;
			FScreenshotRequest::RequestScreenshot(FString(TEXT("autoshot.png")), false, false);
			UE_LOG(LogParisBomb, Log, TEXT("Auto screenshot taken at %.1fs; alive %d v %d"), T, S->AliveAttackers, S->AliveDefenders);
			UE_LOG(LogParisBomb, Log, TEXT("FPS %.1f (%.2f ms/frame, averaged over the last seconds)"), GAverageFPS, GAverageMS);
			for (AFPSCharacter* C : Characters)
			{
				if (!C) continue;
				const APlayerState* PS = C->GetPlayerState();
				const ABombPlayerState* BPS = PSOf(C);
				const FWeaponDef* D = C->HeldDef();
				UE_LOG(LogParisBomb, Log, TEXT("  %-16s team %d at (%.0f, %.0f) speed %.0f dead %d bomb %d $%d holding %s main %s kevlar %d nades %d/%d/%d"),
					PS ? *PS->GetPlayerName() : TEXT("?"), (int32)C->GetTeam(), C->GetActorLocation().X, C->GetActorLocation().Y,
					C->GetVelocity().Size(), C->bDead ? 1 : 0, C->bHasBomb ? 1 : 0, BPS ? BPS->Money : -1, D ? *D->Name : TEXT("-"),
					C->HasMain() ? *C->Guns[0].Def->Name : TEXT("-"), C->Armor, C->Nades[0], C->Nades[1], C->Nades[2]);
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
	UpdateBotsMayTake();

	switch (S->Phase)
	{
	case ERoundPhase::Warmup:
		if (S->PhaseTimeRemaining <= 0.f) StartRound();
		break;
	case ERoundPhase::Freeze:
		if (S->PhaseTimeRemaining <= 0.f)
		{
			for (AFPSCharacter* C : Characters)
			{
				if (!C) continue;
				C->SetFrozen(false);
				// whatever you blew before GO was practice: everyone starts the round full
				for (FGunState& G : C->Guns) if (G.IsValid()) { G.Ammo = G.Def->MagSize; G.Reserve = G.Def->Reserve; G.Reloading = 0.f; }
			}
			if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(HumanController())) PC->CloseShop();
			LiveStartTime = Now;
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
				if (Bomb)
				{
					for (AFPSCharacter* C : Characters)
					{
						if (C && !C->bDead && FVector::Dist(C->GetActorLocation(), Bomb->GetActorLocation()) < BlastRadius)
						{
							C->LastHitWeapon = TEXT("the bomb");
							C->bLastHitHeadshot = false;
							C->Die(nullptr, Bomb);
						}
					}
					GameSounds::At(this, TEXT("S_Explosion"), Bomb->GetActorLocation(), 2.0f);
				}
				EndRound(EBombTeam::Attackers, TEXT("the bomb exploded"));
			}
		}
		else if (S->PhaseTimeRemaining <= 0.f)
		{
			S->PhaseTimeRemaining = 0.f;
			// at 0:00 the round only ends once nobody is mid-plant
			bool bPlanting = false;
			for (AFPSCharacter* C : Characters)
			{
				if (C && !C->bDead && C->bHasBomb && C->bInteracting && C->InteractProgress > 0.f) bPlanting = true;
			}
			if (!bPlanting) EndRound(EBombTeam::Defenders, TEXT("time ran out"));
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
		if (!C->GetCharacterMovement()->IsMovingOnGround()) { C->InteractProgress = 0.f; return; }
		for (ABombSite* Site : Sites)
		{
			if (Site && Site->IsInside(C->GetActorLocation()))
			{
				if (C->InteractProgress <= 0.f && S->PhaseTimeRemaining <= 0.f) return;   // no new plant at 0:00
				const float Before = C->InteractProgress;
				C->InteractProgress += DeltaSeconds / PlantTime;
				if (!C->bIsBot && FMath::FloorToInt(C->InteractProgress * 8.f) != FMath::FloorToInt(Before * 8.f)) GameSounds::UI(this, TEXT("S_Tick"), 0.5f);
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
			const float Before = C->InteractProgress;
			C->InteractProgress += DeltaSeconds / DefuseTime;
			if (!C->bIsBot && FMath::FloorToInt(C->InteractProgress * 8.f) != FMath::FloorToInt(Before * 8.f)) GameSounds::UI(this, TEXT("S_Tick"), 0.5f);
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
	Planter->OnBombPlanted();
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
		if (C && C->GetTeam() == EBombTeam::Attackers) Pay(C, Weapons::PlantMoney);
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
	if (C->bIsBot && !bBotsMayTakeBomb) return;        // the bots leave it for you
	Bomb->Destroy();
	Bomb = nullptr;
	C->OnBombPickedUp();
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
