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
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "BombPlayerState.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "ParisBombMap.h"
#include "GameSounds.h"

AFPSCharacter::AFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	BaseEyeHeight = 70.f;
	bUseControllerRotationYaw = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(95.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// generated models (tools/build_map.py -> build_level.py); missing => the primitive stand-ins below
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Soldier(TEXT("/Game/Meshes/soldier.soldier"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Meshes/rifle.rifle"));

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
	if (Rifle.Succeeded())
	{
		GunMesh1P->SetStaticMesh(Rifle.Object);
		GunMesh1P->SetRelativeLocation(FVector(22.f, 13.f, -19.f));     // grip bottom-right, sights just under the crosshair
		GunMesh1P->SetRelativeScale3D(FVector(0.85f));
	}
	else
	{
		if (Cube.Succeeded()) GunMesh1P->SetStaticMesh(Cube.Object);
		GunMesh1P->SetRelativeLocation(FVector(42.f, 16.f, -15.f));
		GunMesh1P->SetRelativeScale3D(FVector(0.20f, 0.028f, 0.045f));
	}
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
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = 260.f;
	Move->JumpZVelocity = 430.f;
	Move->AirControl = 0.25f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(55.f);
	Move->bUseControllerDesiredRotation = false;
	Move->bOrientRotationToMovement = false;
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	AmmoInMag = MagSize;
	AmmoReserve = ReserveMax;
	ApplyTeamVisuals();
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
		// fallback: engine basic shape material tinted through a dynamic instance
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
			GunMesh1P->SetMaterial(0, Gun);
		}
	}
}

void AFPSCharacter::ResetForRound(const FTransform& SpawnTransform)
{
	bDead = false;
	Health = 100.f;
	bHasBomb = false;
	AmmoInMag = MagSize;
	AmmoReserve = ReserveMax;
	bReloading = false;
	bFiring = false;
	bInteracting = false;
	InteractProgress = 0.f;
	InteractHint.Empty();
	Spread = 0.f;
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
	Health -= DamageAmount;
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
	InteractProgress = 0.f;
	StopFire();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
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
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AFPSCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AFPSCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &AFPSCharacter::StopFire);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &AFPSCharacter::Reload);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AFPSCharacter::StartSprint);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AFPSCharacter::StopSprint);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &AFPSCharacter::StartCrouch);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &AFPSCharacter::StopCrouch);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AFPSCharacter::StartInteract);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &AFPSCharacter::StopInteract);
	PlayerInputComponent->BindAction("Scoreboard", IE_Pressed, this, &AFPSCharacter::ToggleScoreboard);
	PlayerInputComponent->BindAction("Scoreboard", IE_Released, this, &AFPSCharacter::ToggleScoreboard);
	PlayerInputComponent->BindAction("SwitchTeam", IE_Pressed, this, &AFPSCharacter::SwitchTeamRequest);
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

void AFPSCharacter::TurnRate(float Value) { AddControllerYawInput(Value); }
void AFPSCharacter::LookUpRate(float Value) { AddControllerPitchInput(Value); }

void AFPSCharacter::StartJump()
{
	if (bDead || bFrozen) return;
	Jump();
}

void AFPSCharacter::StartFire()
{
	if (bDead || bFrozen) return;
	bFiring = true;
}

void AFPSCharacter::StopFire()
{
	bFiring = false;
}

void AFPSCharacter::Reload()
{
	if (bDead || bReloading || AmmoInMag >= MagSize || AmmoReserve <= 0) return;
	bReloading = true;
	ReloadEndTime = GetWorld()->GetTimeSeconds() + ReloadTime;
	GameSounds::At(this, TEXT("S_Reload"), GetActorLocation(), 0.8f);
}

void AFPSCharacter::StartSprint()
{
	bSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void AFPSCharacter::StopSprint()
{
	bSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
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

// ---------------------------------------------------------------- weapon

void AFPSCharacter::FireOnce(float ExtraSpreadDeg)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	if (AmmoInMag <= 0)
	{
		if (AmmoReserve <= 0 && !bIsBot) GameSounds::UI(this, TEXT("S_DryFire"), 0.6f);
		Reload();
		NextFireTime = Now + 0.25f;
		return;
	}
	AmmoInMag--;
	NextFireTime = Now + FireInterval;

	const FVector Start = GetEyeLocation();
	FRotator Aim = GetAimRotation();
	const float Moving = GetVelocity().Size2D() / WalkSpeed;
	const float TotalSpread = Spread + Moving * 2.0f + (bIsCrouched ? -0.3f : 0.f) + ExtraSpreadDeg;
	const float Cone = FMath::DegreesToRadians(FMath::Max(0.15f, TotalSpread));
	const FVector Dir = FMath::VRandCone(Aim.Vector(), Cone);
	const FVector End = Start + Dir * Range;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(Shot), false, this);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : End;

	// tracer + impact (debug draw: visible in editor / development builds)
	const FVector Muzzle = Start + Aim.RotateVector(FVector(60.f, 14.f, -12.f));
	DrawDebugLine(World, Muzzle, ImpactPoint, FColor(255, 215, 120), false, 0.05f, 0, 1.2f);
	GameSounds::At(this, TEXT("S_RifleShot"), Muzzle, bIsBot ? 0.8f : 1.0f, FMath::FRandRange(0.94f, 1.06f));
	if (bHit)
	{
		DrawDebugPoint(World, ImpactPoint, 6.f, FColor(255, 240, 200), false, 0.35f);
		if (AFPSCharacter* Victim = Cast<AFPSCharacter>(Hit.GetActor()))
		{
			const float HeadZ = Victim->GetActorLocation().Z + Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 28.f;
			const bool bHeadshot = Hit.ImpactPoint.Z > HeadZ;
			const float Dmg = bHeadshot ? HeadDamage : BodyDamage;
			UGameplayStatics::ApplyPointDamage(Victim, Dmg, Dir, Hit, GetController(), this, UDamageType::StaticClass());
			LastHitMarkerTime = Now;
			if (!bIsBot) GameSounds::UI(this, bHeadshot ? TEXT("S_Headshot") : TEXT("S_Hit"), 0.7f);
		}
	}

	// recoil & bloom
	Spread = FMath::Min(Spread + 0.55f, 5.5f);
	if (!bIsBot)
	{
		AddControllerPitchInput(-0.35f - FMath::FRandRange(0.f, 0.25f));
		AddControllerYawInput(FMath::FRandRange(-0.25f, 0.25f));
	}
	MuzzleLight->SetIntensity(4000.f);
	MuzzleOffTime = Now + 0.04f;
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
	if (bReloading && Now >= ReloadEndTime)
	{
		const int32 Need = MagSize - AmmoInMag;
		const int32 Take = FMath::Min(Need, AmmoReserve);
		AmmoInMag += Take;
		AmmoReserve -= Take;
		bReloading = false;
	}
	Spread = FMath::Max(0.f, Spread - DeltaSeconds * 6.f);

	// footsteps: one every 1.7 m on the ground (quieter when crouched)
	const float Speed2D = GetVelocity().Size2D();
	if (Speed2D > 120.f && GetCharacterMovement()->IsMovingOnGround())
	{
		StepDistance += Speed2D * DeltaSeconds;
		if (StepDistance > 170.f)
		{
			StepDistance = 0.f;
			static const TCHAR* Steps[3] = { TEXT("S_Step0"), TEXT("S_Step1"), TEXT("S_Step2") };
			GameSounds::At(this, Steps[FMath::RandRange(0, 2)], GetActorLocation() - FVector(0, 0, 80.f), bIsCrouched ? 0.25f : (bSprinting ? 0.8f : 0.55f), FMath::FRandRange(0.9f, 1.1f));
		}
	}
	else
	{
		StepDistance = 120.f;                         // the first step after a stop comes quickly
	}
	if (bSoldierMesh)
	{
		// keep the feet on the ground and squash the model when the capsule is crouched
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -Half));
		BodyMesh->SetRelativeScale3D(FVector(1.f, 1.f, Half / 88.f));
	}

	const ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	const bool bLive = GS && GS->Phase == ERoundPhase::Live;
	if (bFiring && bLive && !bReloading && !bFrozen && Now >= NextFireTime)
	{
		FireOnce(0.f);
	}

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
}
