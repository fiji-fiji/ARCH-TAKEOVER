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

/**
 * Bomb-defusal rules (Counter-Strike style), single machine: one human + bots to fill 5v5.
 *   Freeze -> Live (round timer) -> plant (bomb timer) -> PostRound -> next round. Half-time side swap, first to 13.
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
	UPROPERTY(EditAnywhere, Category = "Rules") float FreezeTime = 8.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float RoundTime = 150.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float BombTime = 45.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float PlantTime = 4.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float DefuseTime = 7.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float PostRoundTime = 7.f;
	UPROPERTY(EditAnywhere, Category = "Rules") float MatchOverTime = 15.f;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 RoundsToWin = 13;
	UPROPERTY(EditAnywhere, Category = "Rules") int32 HalfTimeRound = 12;
	UPROPERTY(EditAnywhere, Category = "Rules") EBombTeam HumanTeam = EBombTeam::Defenders;

	// ---- world
	UPROPERTY() TArray<TObjectPtr<ATeamPlayerStart>> Starts;
	UPROPERTY() TArray<TObjectPtr<ABombSite>> Sites;
	UPROPERTY() TArray<TObjectPtr<ACalloutMarker>> Callouts;
	UPROPERTY() TArray<TObjectPtr<AFPSCharacter>> Characters;
	UPROPERTY() TObjectPtr<ABombActor> Bomb;
	UPROPERTY() TObjectPtr<ABombSite> PlantedSite;
	UPROPERTY() TObjectPtr<ABombSite> AttackSite;      // the site the attacking bots go for this round

	// ---- API used by characters / bots / bomb
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
	ATeamPlayerStart* StartFor(EBombTeam Team, int32 Index) const;
	AFPSCharacter* HumanCharacter() const;
	APlayerController* HumanController() const;

	bool bPendingTeamSwitch = false;
	/** Dev helper: launch with -pbm_autoshot=<seconds> to take a HighResShot after that delay and quit 4 s later. */
	float AutoShotAt = -1.f;
	bool bAutoShotTaken = false;
	int32 BotNameIndex = 0;
	FRandomStream Rng;
};
