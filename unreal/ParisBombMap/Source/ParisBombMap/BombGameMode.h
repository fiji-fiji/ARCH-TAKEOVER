#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BombTypes.h"
#include "BombGameMode.generated.h"

class AFPSCharacter;
class ATeamPlayerStart;
class ABombSite;
class ACalloutMarker;
class ABombActor;
class ABombGameState;
class ABotController;
class ABombPlayerState;

/**
 * Bomb-defusal rules ported from FPS Reborn (modes.js StandardMode), single machine: one human + bots to fill 5v5.
 *   Freeze (buy phase, 20 s; 5 s in round 1) -> Live (100 s) -> plant (45 s) -> PostRound (5 s) -> next round.
 *   Money: start 800, win 3 000, loss 1 900 / 2 400 / 2 900 by streak, kill 200, plant 300, cap 9 000.
 *   First to 13, win by two, sides swap after round 12 and every round in overtime.
 */
UCLASS()
class PARISBOMBMAP_API ABombGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABombGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// ---- rules
	UPROPERTY(EditAnywhere, Category = "Rules") int32 TeamSize = 5;
	UPROPERTY(EditAnywhere, Category = "Rules") float WarmupTime = 4.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float BuyTime = 20.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float FirstRoundBuyTime = 5.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float RoundTime = 100.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float BombTime = 45.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float PlantTime = 4.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float DefuseTime = 7.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float PostRoundTime = 5.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float MatchOverTime = 15.f;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 RoundsToWin = 13;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 HalfTimeRound = 12;
	UPROPERTY(EditAnywhere, Category = "Rules") bool bWinByTwo = true;
	UPROPERTY(EditAnywhere, Category = "Rules") float BlastRadius = 1000.f;      // cm (FPS Reborn bloomRadius 10 m)
	UPROPERTY(EditAnywhere, Category = "Rules") float SpawnFenceRadius = 1400.f; // cm: how far you can walk before GO
	UPROPERTY(EditAnywhere, Category = "Rules") EBombTeam HumanTeam = EBombTeam::Defenders;

	// ---- world
	UPROPERTY() TArray<TObjectPtr<ATeamPlayerStart>> Starts;
	UPROPERTY() TArray<TObjectPtr<ABombSite>> Sites;
	UPROPERTY() TArray<TObjectPtr<ACalloutMarker>> Callouts;
	UPROPERTY() TArray<TObjectPtr<AFPSCharacter>> Characters;
	UPROPERTY() TObjectPtr<ABombActor> Bomb;
	UPROPERTY() TObjectPtr<ABombSite> PlantedSite;
	UPROPERTY() TObjectPtr<ABombSite> AttackSite;      // the site the attacking bots go for this round

	// ---- bomb hand-off (FPS Reborn: your bot teammates leave the bomb for you)
	bool bBombForHuman = false;
	bool bBotsMayTakeBomb = true;
	float LiveStartTime = 0.f;

	// ---- API used by characters / bots / bomb / HUD
	void OnCharacterDied(AFPSCharacter* Victim, AController* Killer);
	void TickInteract(AFPSCharacter* C, float DeltaSeconds);
	void PlantBomb(AFPSCharacter* Planter, ABombSite* Site);
	void DefuseBomb(AFPSCharacter* Defuser);
	void DropBomb(AFPSCharacter* Carrier);
	void PickupBomb(AFPSCharacter* C);
	void RequestHumanTeamSwitch();
	int32 CountAlive(EBombTeam Team) const;
	FString NearestCallout(const FVector& Loc) const;
	ABombGameState* GS() const;
	bool SpawnFence(EBombTeam Team, FVector& OutCenter, float& OutRadius) const;
	AFPSCharacter* HumanCharacter() const;

	// ---- economy / shop
	bool CanBuy(const AFPSCharacter* C) const;
	bool CanAfford(const AFPSCharacter* C, FName Item) const;
	bool Buy(AFPSCharacter* C, FName Item);
	bool Sell(AFPSCharacter* C, FName Item);
	int32 MinNextMoney(const AFPSCharacter* C) const;
	void Pay(AFPSCharacter* C, int32 Amount);
	bool IsPistolRound() const;

private:
	void CollectWorldActors();
	void SpawnBots();
	void StartRound();
	void SetPhase(ERoundPhase Phase, float Duration);
	void EndRound(EBombTeam Winner, const FString& Reason);
	void SwapSides();
	void ResetMatch();
	void UpdateAliveCounts();
	void UpdateSpectating();
	void UpdateBotsMayTake();
	void ClearRoundActors();
	ATeamPlayerStart* StartFor(EBombTeam Team, int32 Index) const;
	APlayerController* HumanController() const;
	static ABombPlayerState* PSOf(const AFPSCharacter* C);

	bool bPendingTeamSwitch = false;
	int32 LossStreak[2] = { 0, 0 };        // by EBombTeam (swapped with the sides)
	/** Dev helper: launch with -pbm_autoshot=<seconds> to take a HighResShot after that delay and quit 4 s later. */
	float AutoShotAt = -1.f;
	bool bAutoShotTaken = false;
	int32 BotNameIndex = 0;
	FRandomStream Rng;
};
