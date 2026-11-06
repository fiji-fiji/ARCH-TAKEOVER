#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BombTypes.h"
#include "BombGameState.generated.h"

/** Everything the HUD needs to know about the match. Written by ABombGameMode. */
UCLASS()
class PARISBOMBMAP_API ABombGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Match") ERoundPhase Phase = ERoundPhase::Warmup;
	UPROPERTY(BlueprintReadOnly, Category = "Match") float PhaseTimeRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Match") int32 RoundNumber = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Match") int32 ScoreAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Match") int32 ScoreDefenders = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Match") int32 AliveAttackers = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Match") int32 AliveDefenders = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Match") FString RoundResultText;
	UPROPERTY(BlueprintReadOnly, Category = "Match") FString MatchResultText;
	UPROPERTY(BlueprintReadOnly, Category = "Match") EBombTeam HumanTeam = EBombTeam::Defenders;

	UPROPERTY(BlueprintReadOnly, Category = "Bomb") bool bBombPlanted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bomb") bool bBombDropped = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bomb") float BombTimeRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bomb") FString PlantedSiteId;
	UPROPERTY(BlueprintReadOnly, Category = "Bomb") FString BombCarrierName;

	/** Kill feed: newest last, each entry expires after a few seconds. */
	struct FFeedEntry { FString Text; float Expires; FLinearColor Color; };
	TArray<FFeedEntry> KillFeed;
	void AddFeed(const FString& Text, const FLinearColor& Color);
};
