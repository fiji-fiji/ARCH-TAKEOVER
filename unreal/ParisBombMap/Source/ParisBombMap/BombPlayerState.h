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

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Plants = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Defuses = 0;
};
