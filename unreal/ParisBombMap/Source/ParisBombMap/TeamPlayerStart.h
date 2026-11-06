#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "TeamPlayerStart.generated.h"

/** PlayerStart tagged with a team: 0 = Attackers, 1 = Defenders. Placed by Scripts/build_level.py. */
UCLASS()
class PARISBOMBMAP_API ATeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	ATeamPlayerStart(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FString SpawnLabel;
};
