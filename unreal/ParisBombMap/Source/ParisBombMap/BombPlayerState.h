#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BombTypes.h"
#include "BombPlayerState.generated.h"

UCLASS()
class PARISBOMBMAP_API ABombPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Team")
	EBombTeam Team = EBombTeam::None;

	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Deaths = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Plants = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Score") int32 Defuses = 0;

	// ---- economy (FPS Reborn rules)
	UPROPERTY(BlueprintReadOnly, Category = "Economy") int32 Money = 800;
	/** Alive at the end of the last round: keeps the loadout. */
	bool bSurvived = false;
	/** Things bought this buy phase (refunded in full when replaced / sold back). */
	FName RoundBuyMain, RoundBuySide, RoundBuyArmor;
	int32 PrevArmor = 0;
	TMap<FName, int32> RoundBuyNades;
	/** HUD: the last money gain and when it happened. */
	int32 LastGain = 0;
	float LastGainTime = -100.f;

	void ClearRoundBuys()
	{
		RoundBuyMain = RoundBuySide = RoundBuyArmor = NAME_None;
		PrevArmor = 0;
		RoundBuyNades.Empty();
	}
};
