#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BombTypes.h"
#include "Weapons.h"
#include "FPSCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class AWeaponPickup;

UENUM()
enum class EHeldSlot : uint8 { Main, Side, Knife, Nade, Bomb };

/** First-person soldier used by both the human player and the bots.
 *  Loadout per FPS Reborn: a main gun, a sidearm, the knife, grenades, kevlar, maybe the bomb. */
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
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GunMesh1P;    // seen by owner only: the held item
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> MuzzleLight;

	// ---- movement (cm/s; FPS Reborn: run by default, Shift walks quietly)
	UPROPERTY(EditAnywhere, Category = "Movement") float RunSpeed = Weapons::RunSpeed * 100.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = Weapons::WalkSpeed * 100.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float CrouchSpeed = Weapons::CrouchSpeed * 100.f;
	UPROPERTY(EditAnywhere, Category = "Camera") float BaseFov = 95.f;

	// ---- state
	UPROPERTY(BlueprintReadOnly) float Health = 100.f;
	UPROPERTY(BlueprintReadOnly) int32 Armor = 0;
	UPROPERTY(BlueprintReadOnly) bool bDead = false;
	UPROPERTY(BlueprintReadOnly) bool bHasBomb = false;
	UPROPERTY(BlueprintReadOnly) bool bIsBot = false;
	UPROPERTY(BlueprintReadOnly) bool bInteracting = false;
	UPROPERTY(BlueprintReadOnly) float InteractProgress = 0.f;   // 0..1 (plant / defuse)
	UPROPERTY(BlueprintReadOnly) FString InteractHint;           // "PLANTING AT A" etc. (set by game mode)
	bool bFrozen = false;
	bool bFiring = false;          // trigger held
	bool bTriggerPulled = false;   // pressed this frame (semi-auto)
	bool bAltHeld = false;         // right mouse (scope / under-hand throw)
	bool bWalking = false;         // Shift: slow, quiet
	bool bScoped = false;
	float Spread = 0.f;            // current spread in degrees (HUD crosshair)
	float LastDamageTime = -100.f;
	float LastHitMarkerTime = -100.f;     // for the HUD hit marker
	float StepDistance = 0.f;             // cm walked since the last footstep sound
	bool bSoldierMesh = false;            // generated soldier model loaded (else primitive stand-ins)
	float BlindUntil = -100.f;            // flashed until (seconds); HUD whites out, bots see nothing
	float BlindDuration = 1.f;
	bool bHadBomb = false;                // picked the bomb up this round (bomb hand-off rules)
	FString LastHitWeapon;                // for the kill feed
	bool bLastHitHeadshot = false;

	// ---- inventory
	FGunState Guns[3];                    // EWeaponSlot: Main, Side, Knife
	EHeldSlot Held = EHeldSlot::Side;
	FName NadeKind;                       // the grenade selected in the nade slot
	int32 Nades[3] = { 0, 0, 0 };         // by Weapons::NadeOrder index

	EBombTeam GetTeam() const;
	FVector GetEyeLocation() const;
	FRotator GetAimRotation() const;

	void ApplyTeamVisuals();
	void ResetForRound(const FTransform& SpawnTransform);
	void SetFrozen(bool bInFrozen);
	void Die(AController* Killer, AActor* DamageCauser);

	// loadout
	void ResetLoadout();                  // back to the P9 + knife, no kevlar, no grenades
	void RefillLoadout();                 // full ammo, main gun out
	void GiveWeapon(FName Key);
	bool HasMain() const { return Guns[0].IsValid(); }
	FGunState* HeldGun();
	const FWeaponDef* HeldDef();
	int32 NadeIndex(FName Kind) const;
	int32 NadeCount(FName Kind) const;
	void AddNade(FName Kind, int32 Delta);
	void SwitchTo(EHeldSlot Slot);
	void SelectNade(FName Kind);          // Q / E / C quick select (if owned)
	void CycleNade();                     // 5
	void ApplyHeldVisuals();
	void DropMainWeapon();                // on death: the main gun falls
	bool TryPickup();                     // F: nearest dropped gun within reach
	void TakeDroppedGun(AWeaponPickup* Pickup);
	void OnBombPickedUp();
	void OnBombPlanted();

	// ---- input handlers (also used by bots)
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartJump();
	void StartFire();
	void StopFire();
	void StartAlt();
	void StopAlt();
	void Reload();
	void StartWalk();
	void StopWalk();
	void StartCrouch();
	void StopCrouch();
	void StartInteract();
	void StopInteract();
	void ToggleScoreboard();
	void SwitchTeamRequest();
	void Slot1() { SwitchTo(EHeldSlot::Main); }
	void Slot2() { SwitchTo(EHeldSlot::Side); }
	void Slot3() { SwitchTo(EHeldSlot::Knife); }
	void SlotNade() { CycleNade(); }
	void QuickFlash() { SelectNade(TEXT("flash")); }
	void QuickSmoke() { SelectNade(TEXT("smoke")); }
	void QuickFrag() { SelectNade(TEXT("frag")); }
	void ToggleShop();
	void PickupKey() { TryPickup(); }

	/** Fire one shot with the held gun (hitscan / pellets / melee). ExtraSpreadDeg lets bots be worse shots. */
	void FireOnce(float ExtraSpreadDeg = 0.f);
	/** Throw the selected grenade along the aim (bUnderhand = short lob). */
	void ThrowNade(bool bUnderhand);
	/** Bots: throw a grenade of Kind so it lands near Target. Returns false if none is carried. */
	bool ThrowNadeTo(FName Kind, const FVector& Target);
	void UpdateMoveSpeed();

	bool bShowScoreboard = false;

private:
	void FireHitscan(const FWeaponDef& Def, float SpreadRad, const FVector& Start, const FRotator& Aim);
	void FireMelee(const FWeaponDef& Def, const FVector& Start, const FRotator& Aim);
	float MuzzleOffTime = 0.f;
	float AppliedRecoilPitch = 0.f;       // degrees of recoil currently added to the control rotation
	float AppliedRecoilYaw = 0.f;
	float NadeCooldownUntil = 0.f;
	float ScopeBlend = 0.f;
	FRandomStream Rng;
};
