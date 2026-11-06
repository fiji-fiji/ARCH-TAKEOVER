#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BombTypes.h"
#include "FPSCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/** First-person soldier used by both the human player and the bots. Hitscan rifle, plant / defuse interaction. */
UCLASS()
class PARISBOMBMAP_API AFPSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFPSCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// ---- components
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyMesh;     // seen by others
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeadMesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GunMesh3P;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GunMesh1P;    // seen by owner only
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> MuzzleLight;

	// ---- tuning
	UPROPERTY(EditAnywhere, Category = "Weapon") float FireInterval = 0.095f;
	UPROPERTY(EditAnywhere, Category = "Weapon") float BodyDamage = 30.f;
	UPROPERTY(EditAnywhere, Category = "Weapon") float HeadDamage = 100.f;
	UPROPERTY(EditAnywhere, Category = "Weapon") int32 MagSize = 30;
	UPROPERTY(EditAnywhere, Category = "Weapon") int32 ReserveMax = 90;
	UPROPERTY(EditAnywhere, Category = "Weapon") float ReloadTime = 2.2f;
	UPROPERTY(EditAnywhere, Category = "Weapon") float Range = 12000.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 600.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float SprintSpeed = 820.f;

	// ---- state
	UPROPERTY(BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(BlueprintReadOnly) bool bDead = false;
	UPROPERTY(BlueprintReadOnly) bool bHasBomb = false;
	UPROPERTY(BlueprintReadOnly) bool bIsBot = false;
	UPROPERTY(BlueprintReadOnly) int32 AmmoInMag = 30;
	UPROPERTY(BlueprintReadOnly) int32 AmmoReserve = 90;
	UPROPERTY(BlueprintReadOnly) bool bReloading = false;
	UPROPERTY(BlueprintReadOnly) bool bInteracting = false;
	UPROPERTY(BlueprintReadOnly) float InteractProgress = 0.f;   // 0..1 (plant / defuse)
	UPROPERTY(BlueprintReadOnly) FString InteractHint;           // "Hold E to plant" etc. (set by game mode)
	bool bFrozen = false;
	bool bFiring = false;
	bool bSprinting = false;
	float Spread = 0.f;                   // degrees, grows while firing / moving
	float LastDamageTime = -100.f;
	float LastHitMarkerTime = -100.f;     // for the HUD hit marker
	float StepDistance = 0.f;             // cm walked since the last footstep sound
	bool bSoldierMesh = false;            // generated soldier model loaded (else primitive stand-ins)

	EBombTeam GetTeam() const;
	FVector GetEyeLocation() const;
	FRotator GetAimRotation() const;

	void ApplyTeamVisuals();
	void ResetForRound(const FTransform& SpawnTransform);
	void SetFrozen(bool bInFrozen);
	void Die(AController* Killer, AActor* DamageCauser);

	// ---- input handlers (also used by bots)
	void MoveForward(float Value);
	void MoveRight(float Value);
	void TurnRate(float Value);
	void LookUpRate(float Value);
	void StartJump();
	void StartFire();
	void StopFire();
	void Reload();
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void StartInteract();
	void StopInteract();
	void ToggleScoreboard();
	void SwitchTeamRequest();

	/** Fire one hitscan bullet. ExtraSpreadDeg lets bots be worse shots. */
	void FireOnce(float ExtraSpreadDeg = 0.f);

	bool bShowScoreboard = false;

private:
	float NextFireTime = 0.f;
	float ReloadEndTime = 0.f;
	float MuzzleOffTime = 0.f;
	float RecoilPitch = 0.f;
};
