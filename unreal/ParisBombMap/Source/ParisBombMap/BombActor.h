#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BombActor.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USphereComponent;
class AFPSCharacter;

/** The C4: either dropped on the ground (pick-up by attackers) or planted (blinking, counting down). */
UCLASS()
class PARISBOMBMAP_API ABombActor : public AActor
{
	GENERATED_BODY()

public:
	ABombActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Pickup;

	UPROPERTY(BlueprintReadOnly) bool bPlanted = false;
	UPROPERTY(BlueprintReadOnly) float DefuseProgress = 0.f;   // 0..1, for the HUD

	void SetPlanted(bool bInPlanted);

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                     int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	float BlinkPhase = 0.f;
	bool bWasOn = false;
};
