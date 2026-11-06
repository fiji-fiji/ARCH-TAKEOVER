#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons.h"
#include "WeaponPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;

/** A dropped main gun lying on the ground (FPS Reborn drops.js): anyone can take it. Cleared at round start. */
UCLASS()
class PARISBOMBMAP_API AWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AWeaponPickup();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	/** The gun with whatever ammo was left in it. */
	FGunState Gun;

	/** Give it the gun's mesh and toss it forward from the dead owner. */
	void Init(const FGunState& InGun, const FVector& Forward);

	UFUNCTION()
	void OnStop(const FHitResult& Hit);
};
