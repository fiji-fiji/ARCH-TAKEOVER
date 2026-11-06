#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons.h"
#include "GrenadeActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;
class AFPSCharacter;

/** A thrown grenade (FPS Reborn grenades.js): bounces, then goes off after its fuse.
 *  smoke -> ASmokeCloud for 15 s · flash -> blinds everyone looking at it · frag -> blast damage. */
UCLASS()
class PARISBOMBMAP_API AGrenadeActor : public AActor
{
	GENERATED_BODY()

public:
	AGrenadeActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Sphere;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	FName Kind;
	TWeakObjectPtr<AFPSCharacter> Thrower;
	float Fuse = 1.f;

	void Init(FName InKind, AFPSCharacter* InThrower, const FVector& Velocity);

	UFUNCTION()
	void OnBounce(const FHitResult& Hit, const FVector& Velocity);

private:
	void Detonate();
	float LastBounceSound = -1.f;
};

/** The smoke: an opaque sphere that blocks sight (bot line traces) but not bullets. */
UCLASS()
class PARISBOMBMAP_API ASmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	ASmokeCloud();
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	float Life = Weapons::SmokeLife;

private:
	float Age = 0.f;
};
