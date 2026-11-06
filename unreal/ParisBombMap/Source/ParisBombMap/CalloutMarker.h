#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CalloutMarker.generated.h"

class UTextRenderComponent;

/** Named map location ("Mid", "A Site", ...). Used by the HUD (nearest callout) and by bots as waypoints. */
UCLASS()
class PARISBOMBMAP_API ACalloutMarker : public AActor
{
	GENERATED_BODY()

public:
	ACalloutMarker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callout")
	FString CalloutName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callout")
	TObjectPtr<UTextRenderComponent> Text;

	UFUNCTION(BlueprintCallable, Category = "Callout")
	void SetCallout(const FString& Name);
};
