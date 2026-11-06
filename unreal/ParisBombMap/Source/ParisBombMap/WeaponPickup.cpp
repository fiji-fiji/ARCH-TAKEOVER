#include "WeaponPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/StaticMesh.h"

AWeaponPickup::AWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetSphereRadius(14.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));       // lying on its side
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -10.f));
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.25f;
	Movement->Friction = 0.8f;
	Movement->ProjectileGravityScale = 1.5f;
	Movement->bRotationFollowsVelocity = false;
	Movement->MaxSpeed = 2000.f;
	Movement->Velocity = FVector::ZeroVector;
}

void AWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	Movement->OnProjectileStop.AddDynamic(this, &AWeaponPickup::OnStop);
}

void AWeaponPickup::Init(const FGunState& InGun, const FVector& Forward)
{
	Gun = InGun;
	Gun.Reloading = 0.f;
	Gun.RecoilPitch = Gun.RecoilYaw = 0.f;
	Gun.SprayIndex = 0;
	if (Gun.Def)
	{
		const FString Path = FString::Printf(TEXT("/Game/Meshes/%s.%s"), *Gun.Def->Mesh, *Gun.Def->Mesh);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
		{
			Mesh->SetStaticMesh(M);
		}
	}
	const FVector Dir = Forward.GetSafeNormal2D();
	Movement->Velocity = Dir * 160.f + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 220.f);
	SetActorRotation(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
}

void AWeaponPickup::OnStop(const FHitResult& Hit)
{
	Movement->StopMovementImmediately();
	Movement->SetUpdatedComponent(nullptr);
}
