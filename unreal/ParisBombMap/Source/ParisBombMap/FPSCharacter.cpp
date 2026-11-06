#include "FPSCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BombPlayerState.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "FPSPlayerController.h"
#include "GrenadeActor.h"
#include "WeaponPickup.h"
#include "ParisBombMap.h"
#include "GameSounds.h"

namespace
{
	UStaticMesh* HeldMesh(const FString& Name)
	{
		static TMap<FString, UStaticMesh*> Cache;
		if (UStaticMesh** Found = Cache.Find(Name)) return *Found;
		const FString Path = FString::Printf(TEXT("/Game/Meshes/%s.%s"), *Name, *Name);
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path);
		if (M) M->AddToRoot();
		Cache.Add(Name, M);
		return M;
	}
}

AFPSCharacter::AFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	Rng.Initialize(FMath::Rand());

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// the engine's Pawn profile ignores Visibility traces: bullets, melee and grenade line-of-sight would pass through
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BaseEyeHeight = 70.f;
	bUseControllerRotationYaw = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(BaseFov);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// generated models (tools/build_map.py -> build_level.py); missing => the primitive stand-ins below
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Soldier(TEXT("/Game/Meshes/soldier.soldier"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	if (Soldier.Succeeded())
	{
		bSoldierMesh = true;
		BodyMesh->SetStaticMesh(Soldier.Object);
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -88.f));      // feet on the ground
	}
	else
	{
		if (Cylinder.Succeeded()) BodyMesh->SetStaticMesh(Cylinder.Object);
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -18.f));
		BodyMesh->SetRelativeScale3D(FVector(0.58f, 0.58f, 1.35f));
	}
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->SetCanEverAffectNavigation(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetCapsuleComponent());
	if (Sphere.Succeeded()) HeadMesh->SetStaticMesh(Sphere.Object);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 66.f));
	HeadMesh->SetRelativeScale3D(FVector(0.34f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetOwnerNoSee(true);
	HeadMesh->SetVisibility(!bSoldierMesh);                            // the soldier model has its own head
	HeadMesh->SetCanEverAffectNavigation(false);

	GunMesh3P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh3P"));
	GunMesh3P->SetupAttachment(GetCapsuleComponent());
	if (Cube.Succeeded()) GunMesh3P->SetStaticMesh(Cube.Object);
	GunMesh3P->SetRelativeLocation(FVector(40.f, 14.f, 40.f));
	GunMesh3P->SetRelativeScale3D(FVector(0.7f, 0.08f, 0.14f));
	GunMesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh3P->SetOwnerNoSee(true);
	GunMesh3P->SetVisibility(!bSoldierMesh);                           // ... and its own rifle
	GunMesh3P->SetCanEverAffectNavigation(false);

	GunMesh1P = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh1P"));
	GunMesh1P->SetupAttachment(Camera);
	if (Cube.Succeeded()) GunMesh1P->SetStaticMesh(Cube.Object);
	GunMesh1P->SetRelativeLocation(FVector(42.f, 16.f, -15.f));
	GunMesh1P->SetRelativeScale3D(FVector(0.20f, 0.028f, 0.045f));
	GunMesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh1P->SetOnlyOwnerSee(true);
	GunMesh1P->bCastDynamicShadow = false;
	GunMesh1P->CastShadow = false;
	GunMesh1P->SetCanEverAffectNavigation(false);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Camera);
	MuzzleLight->SetRelativeLocation(FVector(80.f, 13.f, -10.f));
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.8f, 0.5f));
	MuzzleLight->SetAttenuationRadius(500.f);
	MuzzleLight->SetCastShadows(false);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = RunSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->JumpZVelocity = 460.f;
	Move->AirControl = 0.25f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(66.f);
	Move->bUseControllerDesiredRotation = false;
	Move->bOrientRotationToMovement = false;

	Guns[2].Set(Weapons::Find(TEXT("knife")));
	Guns[1].Set(Weapons::Find(TEXT("p9")));
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	ResetLoadout();
	ApplyTeamVisuals();
	ApplyHeldVisuals();
}

EBombTeam AFPSCharacter::GetTeam() const
{
	const ABombPlayerState* PS = GetPlayerState<ABombPlayerState>();
	return PS ? PS->Team : EBombTeam::None;
}

FVector AFPSCharacter::GetEyeLocation() const
{
	return Camera ? Camera->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
}

FRotator AFPSCharacter::GetAimRotation() const
{
	if (Camera && !bIsBot)
	{
		return Camera->GetComponentRotation();
	}
	return GetBaseAimRotation();
}

void AFPSCharacter::ApplyTeamVisuals()
{
	const EBombTeam Team = GetTeam();
	const TCHAR* Path = Team == EBombTeam::Attackers ? TEXT("/Game/Materials/M_TeamAttackers.M_TeamAttackers") : TEXT("/Game/Materials/M_TeamDefenders.M_TeamDefenders");
	UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
	if (!M)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), TeamColor(Team));
			M = MID;
		}
	}
	if (M)
	{
		BodyMesh->SetMaterial(0, M);
		HeadMesh->SetMaterial(0, M);
	}
	if (!bSoldierMesh)
	{
		if (UMaterialInterface* Gun = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Metal.M_Metal")))
		{
			GunMesh3P->SetMaterial(0, Gun);
		}
	}
}

// ---------------------------------------------------------------- loadout

void AFPSCharacter::ResetLoadout()
{
	Guns[0].Clear();
	Guns[1].Set(Weapons::Find(TEXT("p9")));
	Guns[2].Set(Weapons::Find(TEXT("knife")));
	Held = EHeldSlot::Side;
	Armor = 0;
	Nades[0] = Nades[1] = Nades[2] = 0;
	NadeKind = NAME_None;
	if (ABombPlayerState* PS = GetPlayerState<ABombPlayerState>()) PS->ClearRoundBuys();
	ApplyHeldVisuals();
}

void AFPSCharacter::RefillLoadout()
{
	for (FGunState& G : Guns) if (G.IsValid()) G.Reset();
	Held = HasMain() ? EHeldSlot::Main : EHeldSlot::Side;
	BlindUntil = -100.f;
	if (ABombPlayerState* PS = GetPlayerState<ABombPlayerState>()) PS->ClearRoundBuys();
	ApplyHeldVisuals();
}

void AFPSCharacter::GiveWeapon(FName Key)
{
	const FWeaponDef* D = Weapons::Find(Key);
	if (!D) return;
	const int32 Idx = (int32)D->Slot;
	Guns[Idx].Set(D);
	Held = (EHeldSlot)Idx;
	ApplyHeldVisuals();
}

FGunState* AFPSCharacter::HeldGun()
{
	switch (Held)
	{
	case EHeldSlot::Main: return Guns[0].IsValid() ? &Guns[0] : nullptr;
	case EHeldSlot::Side: return &Guns[1];
	case EHeldSlot::Knife: return &Guns[2];
	default: return nullptr;
	}
}

const FWeaponDef* AFPSCharacter::HeldDef()
{
	FGunState* G = HeldGun();
	return G ? G->Def : nullptr;
}

int32 AFPSCharacter::NadeIndex(FName Kind) const
{
	return Weapons::NadeOrder().IndexOfByKey(Kind);
}

int32 AFPSCharacter::NadeCount(FName Kind) const
{
	const int32 I = NadeIndex(Kind);
	return I >= 0 ? Nades[I] : 0;
}

void AFPSCharacter::AddNade(FName Kind, int32 Delta)
{
	const int32 I = NadeIndex(Kind);
	if (I >= 0) Nades[I] = FMath::Max(0, Nades[I] + Delta);
}

void AFPSCharacter::SwitchTo(EHeldSlot Slot)
{
	if (bDead) return;
	if (Slot == EHeldSlot::Main && !HasMain()) return;
	if (Slot == EHeldSlot::Bomb && !bHasBomb) return;
	if (Slot == EHeldSlot::Nade)
	{
		if (NadeKind.IsNone() || NadeCount(NadeKind) <= 0)
		{
			for (const FName& K : Weapons::NadeOrder()) if (NadeCount(K) > 0) { NadeKind = K; break; }
		}
		if (NadeKind.IsNone() || NadeCount(NadeKind) <= 0) return;
	}
	if (Held == Slot) return;
	Held = Slot;
	bScoped = false;
	if (FGunState* G = HeldGun()) G->Cooldown = FMath::Max(G->Cooldown, 0.35f);   // takes a moment to bring it up
	ApplyHeldVisuals();
	UpdateMoveSpeed();
}

void AFPSCharacter::SelectNade(FName Kind)
{
	if (bDead || NadeCount(Kind) <= 0) return;
	if (Held == EHeldSlot::Nade && NadeKind == Kind) return;
	NadeKind = Kind;
	Held = EHeldSlot::Nade;
	bScoped = false;
	ApplyHeldVisuals();
	UpdateMoveSpeed();
}

void AFPSCharacter::CycleNade()
{
	if (bDead) return;
	TArray<FName> Owned;
	for (const FName& K : Weapons::NadeOrder()) if (NadeCount(K) > 0) Owned.Add(K);
	if (Owned.Num() == 0) return;
	if (Held != EHeldSlot::Nade)
	{
		NadeKind = Owned.Contains(NadeKind) ? NadeKind : Owned[0];
		SwitchTo(EHeldSlot::Nade);
		return;
	}
	const int32 I = Owned.IndexOfByKey(NadeKind);
	if (I + 1 < Owned.Num()) { NadeKind = Owned[I + 1]; ApplyHeldVisuals(); }
	else SwitchTo(HasMain() ? EHeldSlot::Main : EHeldSlot::Side);
}

void AFPSCharacter::ApplyHeldVisuals()
{
	if (!GunMesh1P) return;
	UStaticMesh* M = nullptr;
	FVector Loc(38.f, 17.f, -24.f);
	FRotator Rot(0.f, -4.f, 0.f);
	float Scale = 0.6f;
	switch (Held)
	{
	case EHeldSlot::Main:
	case EHeldSlot::Side:
	case EHeldSlot::Knife:
		if (const FWeaponDef* D = HeldDef())
		{
			M = HeldMesh(D->Mesh);
			if (D->bMelee) { Loc = FVector(30.f, 15.f, -22.f); Rot = FRotator(0.f, -10.f, 0.f); Scale = 0.7f; }
			else if (D->Slot == EWeaponSlot::Side) { Loc = FVector(34.f, 15.f, -22.f); }
		}
		break;
	case EHeldSlot::Nade:
		if (const FGrenadeDef* G = Weapons::FindGrenade(NadeKind))
		{
			M = HeldMesh(G->Mesh);
			Loc = FVector(30.f, 16.f, -26.f); Rot = FRotator(0.f, 0.f, 0.f); Scale = 1.1f;
		}
		break;
	case EHeldSlot::Bomb:
		M = HeldMesh(TEXT("c4"));
		Loc = FVector(32.f, 10.f, -27.f); Rot = FRotator(0.f, 20.f, 0.f); Scale = 0.9f;
		break;
	}
	if (M)
	{
		GunMesh1P->SetStaticMesh(M);
		GunMesh1P->SetRelativeLocation(Loc);
		GunMesh1P->SetRelativeRotation(Rot);
		GunMesh1P->SetRelativeScale3D(FVector(Scale));
		GunMesh1P->SetVisibility(true);
	}
	else
	{
		GunMesh1P->SetVisibility(false);
	}
}

void AFPSCharacter::UpdateMoveSpeed()
{
	const FWeaponDef* D = HeldDef();
	const float K = (D && D->bMelee) ? Weapons::KnifeSpeed : 1.f;
	GetCharacterMovement()->MaxWalkSpeed = (bWalking ? WalkSpeed : RunSpeed) * K;
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed * K;
}

void AFPSCharacter::DropMainWeapon()
{
	if (!HasMain()) return;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Loc = GetActorLocation() + FVector(0.f, 0.f, 20.f);
	if (AWeaponPickup* Pick = GetWorld()->SpawnActor<AWeaponPickup>(AWeaponPickup::StaticClass(), Loc, FRotator::ZeroRotator, P))
	{
		Pick->Init(Guns[0], GetActorForwardVector());
	}
	Guns[0].Clear();
	if (Held == EHeldSlot::Main) Held = EHeldSlot::Side;
	ApplyHeldVisuals();
}

bool AFPSCharacter::TryPickup()
{
	if (bDead || bFrozen) return false;
	AWeaponPickup* Best = nullptr;
	float BestD = Weapons::DropSwapReach * 100.f;
	for (TActorIterator<AWeaponPickup> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < BestD) { BestD = D; Best = *It; }
	}
	if (!Best) return false;
	TakeDroppedGun(Best);
	return true;
}

void AFPSCharacter::TakeDroppedGun(AWeaponPickup* Pickup)
{
	if (!Pickup || !Pickup->Gun.IsValid()) return;
	const FGunState Taken = Pickup->Gun;
	const FVector At = Pickup->GetActorLocation();
	Pickup->Destroy();
	if (HasMain())
	{
		// swap: the one held falls where the other one was
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AWeaponPickup* Pick = GetWorld()->SpawnActor<AWeaponPickup>(AWeaponPickup::StaticClass(), At + FVector(0, 0, 10.f), FRotator::ZeroRotator, P))
		{
			Pick->Init(Guns[0], GetActorRightVector());
		}
	}
	Guns[0] = Taken;
	Held = EHeldSlot::Main;
	Guns[0].Cooldown = 0.35f;
	bScoped = false;
	ApplyHeldVisuals();
	UpdateMoveSpeed();
	GameSounds::At(this, TEXT("S_Pickup"), GetActorLocation(), 0.7f);
}

void AFPSCharacter::OnBombPickedUp()
{
	bHasBomb = true;
	bHadBomb = true;
	if (!bIsBot) GameSounds::UI(this, TEXT("S_Pickup"), 0.7f);
}

void AFPSCharacter::OnBombPlanted()
{
	bHasBomb = false;
	if (Held == EHeldSlot::Bomb) SwitchTo(HasMain() ? EHeldSlot::Main : EHeldSlot::Side);
}

// ---------------------------------------------------------------- round

void AFPSCharacter::ResetForRound(const FTransform& SpawnTransform)
{
	bDead = false;
	Health = 100.f;
	bHasBomb = false;
	bHadBomb = false;
	bFiring = false;
	bTriggerPulled = false;
	bAltHeld = false;
	bScoped = false;
	bInteracting = false;
	InteractProgress = 0.f;
	InteractHint.Empty();
	Spread = 0.f;
	BlindUntil = -100.f;
	AppliedRecoilPitch = AppliedRecoilYaw = 0.f;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	UnCrouch();
	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.GetRotation().Rotator(), false, true);
	if (AController* C = GetController())
	{
		C->SetControlRotation(SpawnTransform.GetRotation().Rotator());
	}
	ApplyTeamVisuals();
	UpdateMoveSpeed();
}

void AFPSCharacter::SetFrozen(bool bInFrozen)
{
	bFrozen = bInFrozen;
	if (bDead)
	{
		return;
	}
	GetCharacterMovement()->SetMovementMode(bInFrozen ? MOVE_None : MOVE_Walking);
	if (bInFrozen)
	{
		bFiring = false;
	}
}

float AFPSCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}
	const ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	if (GS && GS->Phase != ERoundPhase::Live)
	{
		return 0.f;   // no damage during freeze / post round
	}
	// friendly fire off
	if (const APawn* Attacker = EventInstigator ? EventInstigator->GetPawn() : nullptr)
	{
		if (const AFPSCharacter* AC = Cast<AFPSCharacter>(Attacker))
		{
			if (AC != this && AC->GetTeam() == GetTeam())
			{
				return 0.f;
			}
		}
	}
	// kevlar soaks first
	const float Soak = FMath::Min((float)Armor, DamageAmount);
	Armor -= FMath::RoundToInt(Soak);
	Health -= DamageAmount - Soak;
	LastDamageTime = GetWorld()->GetTimeSeconds();
	if (Health <= 0.f)
	{
		Health = 0.f;
		Die(EventInstigator, DamageCauser);
	}
	return DamageAmount;
}

void AFPSCharacter::Die(AController* Killer, AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bFiring = false;
	bInteracting = false;
	bScoped = false;
	InteractProgress = 0.f;
	StopFire();
	DropMainWeapon();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	if (Camera) Camera->SetFieldOfView(BaseFov);
	GameSounds::At(this, TEXT("S_Death"), GetActorLocation(), 0.9f);
	if (ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OnCharacterDied(this, Killer);
	}
}

// ---------------------------------------------------------------- input

void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &AFPSCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AFPSCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &AFPSCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AFPSCharacter::LookUp);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFPSCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AFPSCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AFPSCharacter::StopFire);
	PlayerInputComponent->BindAction("AltFire", IE_Pressed, this, &AFPSCharacter::StartAlt);
	PlayerInputComponent->BindAction("AltFire", IE_Released, this, &AFPSCharacter::StopAlt);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AFPSCharacter::Reload);
	PlayerInputComponent->BindAction("Walk", IE_Pressed, this, &AFPSCharacter::StartWalk);
	PlayerInputComponent->BindAction("Walk", IE_Released, this, &AFPSCharacter::StopWalk);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AFPSCharacter::StartCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &AFPSCharacter::StopCrouch);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AFPSCharacter::StartInteract);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &AFPSCharacter::StopInteract);
	PlayerInputComponent->BindAction("Scoreboard", IE_Pressed, this, &AFPSCharacter::ToggleScoreboard);
	PlayerInputComponent->BindAction("Scoreboard", IE_Released, this, &AFPSCharacter::ToggleScoreboard);
	PlayerInputComponent->BindAction("SwitchTeam", IE_Pressed, this, &AFPSCharacter::SwitchTeamRequest);
	PlayerInputComponent->BindAction("Slot1", IE_Pressed, this, &AFPSCharacter::Slot1);
	PlayerInputComponent->BindAction("Slot2", IE_Pressed, this, &AFPSCharacter::Slot2);
	PlayerInputComponent->BindAction("Slot3", IE_Pressed, this, &AFPSCharacter::Slot3);
	PlayerInputComponent->BindAction("SlotNade", IE_Pressed, this, &AFPSCharacter::SlotNade);
	PlayerInputComponent->BindAction("QuickFlash", IE_Pressed, this, &AFPSCharacter::QuickFlash);
	PlayerInputComponent->BindAction("QuickSmoke", IE_Pressed, this, &AFPSCharacter::QuickSmoke);
	PlayerInputComponent->BindAction("QuickFrag", IE_Pressed, this, &AFPSCharacter::QuickFrag);
	PlayerInputComponent->BindAction("Shop", IE_Pressed, this, &AFPSCharacter::ToggleShop);
	PlayerInputComponent->BindAction("Pickup", IE_Pressed, this, &AFPSCharacter::PickupKey);
}

void AFPSCharacter::MoveForward(float Value)
{
	if (bDead || bFrozen || FMath::IsNearlyZero(Value)) return;
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Value);
}

void AFPSCharacter::MoveRight(float Value)
{
	if (bDead || bFrozen || FMath::IsNearlyZero(Value)) return;
	const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Value);
}

void AFPSCharacter::Turn(float Value)
{
	const float K = bScoped && HeldDef() ? HeldDef()->ScopeFov / BaseFov : 1.f;
	AddControllerYawInput(Value * K);
}

void AFPSCharacter::LookUp(float Value)
{
	const float K = bScoped && HeldDef() ? HeldDef()->ScopeFov / BaseFov : 1.f;
	AddControllerPitchInput(Value * K);
}

void AFPSCharacter::StartJump()
{
	if (bDead || bFrozen) return;
	Jump();
}

void AFPSCharacter::StartFire()
{
	if (bDead || bFrozen) return;
	bFiring = true;
	bTriggerPulled = true;
}

void AFPSCharacter::StopFire()
{
	bFiring = false;
}

void AFPSCharacter::StartAlt()
{
	if (bDead) return;
	bAltHeld = true;
	if (Held == EHeldSlot::Nade && !bFrozen)
	{
		ThrowNade(true);
	}
}

void AFPSCharacter::StopAlt()
{
	bAltHeld = false;
}

void AFPSCharacter::Reload()
{
	if (bDead) return;
	FGunState* G = HeldGun();
	if (G && G->StartReload())
	{
		GameSounds::At(this, TEXT("S_Reload"), GetActorLocation(), 0.8f);
	}
}

void AFPSCharacter::StartWalk()
{
	bWalking = true;
	UpdateMoveSpeed();
}

void AFPSCharacter::StopWalk()
{
	bWalking = false;
	UpdateMoveSpeed();
}

void AFPSCharacter::StartCrouch()
{
	if (bDead) return;
	Crouch();
}

void AFPSCharacter::StopCrouch()
{
	UnCrouch();
}

void AFPSCharacter::StartInteract()
{
	if (bDead) return;
	bInteracting = true;
	if (bHasBomb) SwitchTo(EHeldSlot::Bomb);     // 4: take the bomb out (and keep holding to plant)
}

void AFPSCharacter::StopInteract()
{
	bInteracting = false;
	InteractProgress = 0.f;
}

void AFPSCharacter::ToggleScoreboard()
{
	bShowScoreboard = !bShowScoreboard;
}

void AFPSCharacter::SwitchTeamRequest()
{
	if (ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->RequestHumanTeamSwitch();
	}
}

void AFPSCharacter::ToggleShop()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
	{
		PC->ToggleShop();
	}
}

// ---------------------------------------------------------------- weapons

void AFPSCharacter::FireOnce(float ExtraSpreadDeg)
{
	FGunState* G = HeldGun();
	if (!G || !G->IsValid() || !G->Ready()) return;
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const FWeaponDef& D = *G->Def;
	if (!D.bMelee && G->Ammo <= 0)
	{
		if (!bIsBot) GameSounds::UI(this, TEXT("S_DryFire"), 0.6f);
		G->Cooldown = 0.25f;
		Reload();
		return;
	}
	const bool bScopedNow = D.HasScope() && (bIsBot ? GetVelocity().Size2D() < 50.f : bScoped);
	const float V = GetVelocity().Size2D() / 100.f;
	const bool bGround = GetCharacterMovement()->IsMovingOnGround();
	float SpreadRad = G->Fire(bScopedNow, V, bGround, bIsCrouched, Rng) + FMath::DegreesToRadians(ExtraSpreadDeg);
	Spread = FMath::RadiansToDegrees(SpreadRad);

	const FVector Start = GetEyeLocation();
	FRotator Aim = GetAimRotation();
	if (bIsBot)
	{
		// bots do not steer through the recoil: it only pushes their shots
		Aim.Pitch += FMath::RadiansToDegrees(G->RecoilPitch);
		Aim.Yaw += FMath::RadiansToDegrees(G->RecoilYaw);
	}
	if (D.bMelee)
	{
		FireMelee(D, Start, Aim);
	}
	else
	{
		FireHitscan(D, SpreadRad, Start, Aim);
	}
	const FVector Muzzle = Start + Aim.RotateVector(FVector(60.f, 14.f, -12.f));
	GameSounds::At(this, *D.ShotSound, Muzzle, D.bSilenced ? 0.35f : (bIsBot ? 0.8f : 1.0f), FMath::FRandRange(0.94f, 1.06f));
	if (!D.bMelee)
	{
		MuzzleLight->SetIntensity(D.bSilenced ? 800.f : 4000.f);
		MuzzleOffTime = Now + 0.04f;
	}
}

void AFPSCharacter::FireHitscan(const FWeaponDef& D, float SpreadRad, const FVector& Start, const FRotator& Aim)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	const FVector Fwd = Aim.Vector();
	const FVector Right = FRotationMatrix(Aim).GetUnitAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(Aim).GetUnitAxis(EAxis::Z);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Shot), false, this);
	for (TActorIterator<ASmokeCloud> It(World); It; ++It) Params.AddIgnoredActor(*It);   // bullets fly through smoke
	bool bAnyHit = false;
	for (int32 P = 0; P < FMath::Max(1, D.Pellets); ++P)
	{
		const float Total = SpreadRad + D.PelletSpread;
		const float Ang = Rng.FRand() * 2.f * PI;
		const float Rad = FMath::Sqrt(Rng.FRand()) * Total;
		const FVector Dir = (Fwd + Right * (FMath::Cos(Ang) * FMath::Tan(Rad)) + Up * (FMath::Sin(Ang) * FMath::Tan(Rad))).GetSafeNormal();
		const FVector End = Start + Dir * D.Range * 100.f;
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		const FVector ImpactPoint = bHit ? Hit.ImpactPoint : End;
		const FVector Muzzle = Start + Aim.RotateVector(FVector(60.f, 14.f, -12.f));
		DrawDebugLine(World, Muzzle, ImpactPoint, FColor(255, 215, 120), false, 0.05f, 0, 1.0f);
		if (!bHit) continue;
		DrawDebugPoint(World, ImpactPoint, 5.f, FColor(255, 240, 200), false, 0.3f);
		if (AFPSCharacter* Victim = Cast<AFPSCharacter>(Hit.GetActor()))
		{
			if (Victim->bDead) continue;
			const float Half = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const float HeadZ = Victim->GetActorLocation().Z + Half - 28.f;
			const float LegsZ = Victim->GetActorLocation().Z - Half * 0.15f;
			const bool bHeadshot = Hit.ImpactPoint.Z > HeadZ;
			const bool bLegs = Hit.ImpactPoint.Z < LegsZ;
			float Dmg = bHeadshot ? D.DmgHead : (bLegs ? D.DmgLegs : D.DmgBody);
			Dmg *= HeldGun()->Falloff(FVector::Dist(Start, Hit.ImpactPoint) / 100.f);
			Victim->LastHitWeapon = D.Name;
			Victim->bLastHitHeadshot = bHeadshot;
			UGameplayStatics::ApplyPointDamage(Victim, Dmg, Dir, Hit, GetController(), this, UDamageType::StaticClass());
			bAnyHit = true;
			if (!bIsBot && !Victim->bDead) {}
			if (!bIsBot) { LastHitMarkerTime = Now; GameSounds::UI(this, bHeadshot ? TEXT("S_Headshot") : TEXT("S_Hit"), 0.7f); }
		}
	}
}

void AFPSCharacter::FireMelee(const FWeaponDef& D, const FVector& Start, const FRotator& Aim)
{
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Stab), false, this);
	FHitResult Hit;
	const FVector End = Start + Aim.Vector() * D.Reach * 100.f;
	if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(25.f), Params))
	{
		if (AFPSCharacter* Victim = Cast<AFPSCharacter>(Hit.GetActor()))
		{
			Victim->LastHitWeapon = D.Name;
			Victim->bLastHitHeadshot = false;
			UGameplayStatics::ApplyPointDamage(Victim, D.DmgBody, Aim.Vector(), Hit, GetController(), this, UDamageType::StaticClass());
			if (!bIsBot) { LastHitMarkerTime = World->GetTimeSeconds(); GameSounds::UI(this, TEXT("S_Hit"), 0.7f); }
		}
	}
}

void AFPSCharacter::ThrowNade(bool bUnderhand)
{
	if (bDead || Held != EHeldSlot::Nade || NadeCount(NadeKind) <= 0) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NadeCooldownUntil) return;
	NadeCooldownUntil = Now + 0.6f;
	const FRotator Aim = GetAimRotation();
	const FVector Dir = Aim.Vector();
	const FVector Vel = bUnderhand ? (Dir * 650.f + FVector(0, 0, 150.f)) : (Dir * 1700.f + FVector(0, 0, 250.f));
	const FVector From = GetEyeLocation() + Dir * 40.f + FRotationMatrix(Aim).GetUnitAxis(EAxis::Y) * 15.f;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AGrenadeActor* N = GetWorld()->SpawnActor<AGrenadeActor>(AGrenadeActor::StaticClass(), From, FRotator::ZeroRotator, P))
	{
		N->Init(NadeKind, this, Vel);
	}
	AddNade(NadeKind, -1);
	if (NadeCount(NadeKind) <= 0)
	{
		// next grenade of another kind, or back to the gun
		FName Next = NAME_None;
		for (const FName& K : Weapons::NadeOrder()) if (NadeCount(K) > 0) { Next = K; break; }
		if (!Next.IsNone()) { NadeKind = Next; ApplyHeldVisuals(); }
		else SwitchTo(HasMain() ? EHeldSlot::Main : EHeldSlot::Side);
	}
}

bool AFPSCharacter::ThrowNadeTo(FName Kind, const FVector& Target)
{
	if (bDead || NadeCount(Kind) <= 0) return false;
	const FVector From = GetEyeLocation() + GetActorForwardVector() * 40.f;
	// lob (FPS Reborn solveLob): flight time from the distance, gravity from the grenade def
	const FVector D = Target - From;
	const float Dist = D.Size2D() / 100.f;
	const float T = FMath::Clamp(Dist / 12.f, 0.5f, 1.6f);
	const float G = Weapons::NadeGravity * 100.f;
	const FVector Vel(D.X / T, D.Y / T, (D.Z + 0.5f * G * T * T) / T);
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AGrenadeActor* N = GetWorld()->SpawnActor<AGrenadeActor>(AGrenadeActor::StaticClass(), From, FRotator::ZeroRotator, P))
	{
		N->Init(Kind, this, Vel);
	}
	AddNade(Kind, -1);
	return true;
}

void AFPSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (MuzzleLight->Intensity > 0.f && Now >= MuzzleOffTime)
	{
		MuzzleLight->SetIntensity(0.f);
	}
	for (FGunState& G : Guns) if (G.IsValid()) G.Tick(DeltaSeconds);
	FGunState* Gun = HeldGun();
	const FWeaponDef* Def = Gun ? Gun->Def : nullptr;

	// crosshair spread readout
	if (Gun && Def && !Def->bMelee)
	{
		Spread = FMath::RadiansToDegrees(Gun->ComputeSpread(GetVelocity().Size2D() / 100.f, GetCharacterMovement()->IsMovingOnGround(), bIsCrouched, bScoped));
	}
	else
	{
		Spread = 0.f;
	}

	// recoil: the human's view is pushed by the gun's recoil and recovers with it
	if (!bIsBot)
	{
		const float WantP = Gun ? FMath::RadiansToDegrees(Gun->RecoilPitch) : 0.f;
		const float WantY = Gun ? FMath::RadiansToDegrees(Gun->RecoilYaw) : 0.f;
		const float DP = WantP - AppliedRecoilPitch, DY = WantY - AppliedRecoilYaw;
		if (!FMath::IsNearlyZero(DP, 1e-4f) || !FMath::IsNearlyZero(DY, 1e-4f))
		{
			if (AController* C = GetController())
			{
				FRotator R = C->GetControlRotation();
				R.Pitch = FRotator::NormalizeAxis(R.Pitch + DP);
				R.Yaw += DY;
				C->SetControlRotation(R);
			}
			AppliedRecoilPitch = WantP;
			AppliedRecoilYaw = WantY;
		}
		// scope zoom
		bScoped = bAltHeld && Def && Def->HasScope() && Gun && !Gun->Busy() && Held != EHeldSlot::Nade;
		const float TargetBlend = bScoped ? 1.f : 0.f;
		ScopeBlend = FMath::FInterpTo(ScopeBlend, TargetBlend, DeltaSeconds, 14.f);
		const float Fov = FMath::Lerp(BaseFov, Def && Def->HasScope() ? Def->ScopeFov : BaseFov, ScopeBlend);
		if (Camera && !FMath::IsNearlyEqual(Camera->FieldOfView, Fov, 0.01f)) Camera->SetFieldOfView(Fov);
		if (GunMesh1P) GunMesh1P->SetVisibility(ScopeBlend < 0.5f && Held != EHeldSlot::Bomb ? true : (Held == EHeldSlot::Bomb));
	}

	// footsteps: one every 1.7 m on the ground (quieter when crouched or walking)
	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D > 120.f && GetCharacterMovement()->IsMovingOnGround())
	{
		StepDistance += Speed2D * DeltaSeconds;
		if (StepDistance > 170.f)
		{
			StepDistance = 0.f;
			static const TCHAR* Steps[3] = { TEXT("S_Step0"), TEXT("S_Step1"), TEXT("S_Step2") };
			GameSounds::At(this, Steps[FMath::RandRange(0, 2)], GetActorLocation() - FVector(0, 0, 80.f), (bIsCrouched || bWalking) ? 0.25f : 0.7f, FMath::FRandRange(0.9f, 1.1f));
		}
	}
	else
	{
		StepDistance = 120.f;                         // the first step after a stop comes quickly
	}
	if (bSoldierMesh)
	{
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -Half));
		BodyMesh->SetRelativeScale3D(FVector(1.f, 1.f, Half / 88.f));
	}

	const ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	const bool bLive = GS && GS->Phase == ERoundPhase::Live;
	const bool bCanShoot = GS && (GS->Phase == ERoundPhase::Live || GS->Phase == ERoundPhase::Freeze) && !bFrozen;   // practice shots before GO
	if (bCanShoot && Gun && Def)
	{
		if (Held == EHeldSlot::Nade)
		{
			if (bTriggerPulled) ThrowNade(false);
		}
		else if (Def->bAuto || bIsBot)
		{
			if (bFiring) FireOnce(0.f);
		}
		else if (bTriggerPulled)
		{
			FireOnce(0.f);
		}
	}
	bTriggerPulled = false;

	if (bInteracting && bLive)
	{
		if (ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			GM->TickInteract(this, DeltaSeconds);
		}
	}
	else
	{
		InteractProgress = 0.f;
	}

	// before GO: an invisible fence keeps the human inside the spawn area
	if (GS && GS->Phase == ERoundPhase::Freeze && !bFrozen && !bIsBot && !bFenceExempt)
	{
		if (ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)))
		{
			FVector Center; float Radius;
			if (GM->SpawnFence(GetTeam(), Center, Radius))
			{
				FVector P = GetActorLocation();
				const FVector D(P.X - Center.X, P.Y - Center.Y, 0.f);
				if (D.Size() > Radius)
				{
					const FVector N = D.GetSafeNormal();
					SetActorLocation(FVector(Center.X + N.X * Radius, Center.Y + N.Y * Radius, P.Z), false);
				}
			}
		}
	}
}
