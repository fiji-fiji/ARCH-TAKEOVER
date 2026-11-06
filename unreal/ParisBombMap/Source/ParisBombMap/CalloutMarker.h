#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CalloutMarker.generated.h"

class UTextRenderComponent;

/** Named map location ("Mid", "A Site", ...). Used by the HUD (nearest callout) and by bots as waypoints.
 *  The label turns to face the player's camera so it is never seen mirrored. */
UCLASS()
class PARISBOMBMAP_API ACalloutMarker : public AActor
{
	GENERATED_BODY()

public:
	ACalloutMarker();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Callout")
	FString CalloutName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Callout")
	TObjectPtr<UTextRenderComponent> Text;

	UFUNCTION(BlueprintCallable, Category = "Callout")
	void SetCallout(const FString& Name);

private:
	float NextFaceUpdate = 0.f;
};
