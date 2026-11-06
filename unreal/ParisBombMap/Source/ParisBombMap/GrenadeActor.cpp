#include "GrenadeActor.h"
#include "FPSCharacter.h"
#include "BombGameMode.h"
#include "GameSounds.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"

AGrenadeActor::AGrenadeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->SetSphereRadius(6.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -6.f));
	Mesh->SetRelativeScale3D(FVector(1.5f));
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.45f;
	Movement->Friction = 0.5f;
	Movement->ProjectileGravityScale = Weapons::NadeGravity * 100.f / 980.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->MaxSpeed = 4000.f;
}

void AGrenadeActor::BeginPlay()
{
	Super::BeginPlay();
	Movement->OnProjectileBounce.AddDynamic(this, &AGrenadeActor::OnBounce);
}

void AGrenadeActor::Init(FName InKind, AFPSCharacter* InThrower, const FVector& Velocity)
{
	Kind = InKind;
	Thrower = InThrower;
	if (const FGrenadeDef* D = Weapons::FindGrenade(Kind))
	{
		Fuse = D->Fuse;
		const FString Path = FString::Printf(TEXT("/Game/Meshes/%s.%s"), *D->Mesh, *D->Mesh);
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path)) Mesh->SetStaticMesh(M);
	}
	Movement->Velocity = Velocity;
	GameSounds::At(this, TEXT("S_Toss"), GetActorLocation(), 0.6f);
}

void AGrenadeActor::OnBounce(const FHitResult& Hit, const FVector& Velocity)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Velocity.Size() > 150.f && Now - LastBounceSound > 0.12f)
	{
		LastBounceSound = Now;
		GameSounds::At(this, TEXT("S_Bounce"), GetActorLocation(), FMath::Clamp(Velocity.Size() / 1200.f, 0.2f, 0.8f));
	}
}

void AGrenadeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Fuse -= DeltaSeconds;
	if (Fuse <= 0.f)
	{
		Detonate();
	}
}

void AGrenadeActor::Detonate()
{
	UWorld* World = GetWorld();
	const FVector Pos = GetActorLocation();
	ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
	AFPSCharacter* T = Thrower.Get();
	const EBombTeam Team = T ? T->GetTeam() : EBombTeam::None;

	if (Kind == TEXT("smoke"))
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ASmokeCloud>(ASmokeCloud::StaticClass(), Pos + FVector(0, 0, Weapons::SmokeRadius * 40.f), FRotator::ZeroRotator, P);
		GameSounds::At(this, TEXT("S_SmokePop"), Pos, 0.8f);
	}
	else if (Kind == TEXT("flash"))
	{
		GameSounds::At(this, TEXT("S_FlashBang"), Pos, 1.2f);
		if (GM)
		{
			for (AFPSCharacter* C : GM->Characters)
			{
				if (!C || C->bDead || (Team != EBombTeam::None && C->GetTeam() == Team)) continue;     // never your own team
				const FVector Eye = C->GetEyeLocation();
				const float Dist = FVector::Dist(Eye, Pos) / 100.f;
				if (Dist > Weapons::FlashRange) continue;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(Flash), false, this);
				Q.AddIgnoredActor(C);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, Pos, Eye, ECC_Visibility, Q)) continue;       // behind a wall
				const float Dot = FVector::DotProduct(C->GetAimRotation().Vector(), (Pos - Eye).GetSafeNormal());
				if (Dot < 0.2f && Dist > 4.f) continue;                                                // not looking at it
				const float Strength = (1.f - Dist / Weapons::FlashRange) * FMath::Clamp(0.5f + Dot, 0.4f, 1.5f);
				const float Dur = 0.8f + 3.2f * FMath::Clamp(Strength, 0.f, 1.f);
				const float Now = World->GetTimeSeconds();
				if (Now + Dur > C->BlindUntil)
				{
					C->BlindUntil = Now + Dur;
					C->BlindDuration = Dur;
				}
			}
		}
	}
	else   // frag
	{
		GameSounds::At(this, TEXT("S_FragBoom"), Pos, 1.5f);
		if (GM)
		{
			for (AFPSCharacter* C : GM->Characters)
			{
				if (!C || C->bDead || (Team != EBombTeam::None && C->GetTeam() == Team)) continue;
				const float Dist = FVector::Dist(C->GetActorLocation(), Pos) / 100.f;
				if (Dist > Weapons::FragRadius) continue;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(Frag), false, this);
				Q.AddIgnoredActor(C);
				FHitResult Hit;
				if (World->LineTraceSingleByChannel(Hit, Pos + FVector(0, 0, 20.f), C->GetActorLocation(), ECC_Visibility, Q)) continue;
				const float K = 1.f - FMath::Clamp((Dist - Weapons::FragFull) / (Weapons::FragRadius - Weapons::FragFull), 0.f, 1.f);
				const float Dmg = Weapons::FragMax * K;
				C->LastHitWeapon = TEXT("Frag");
				C->bLastHitHeadshot = false;
				UGameplayStatics::ApplyDamage(C, Dmg, T ? T->GetController() : nullptr, this, UDamageType::StaticClass());
			}
		}
	}
	Destroy();
}

// ---------------------------------------------------------------- smoke cloud

ASmokeCloud::ASmokeCloud()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded()) Mesh->SetStaticMesh(SphereMesh.Object);
	// engine sphere is 100 cm across: scale to the smoke radius (sight-blocking, bullets pass: see FireHitscan)
	Mesh->SetRelativeScale3D(FVector(Weapons::SmokeRadius * 2.f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SmokeMat(TEXT("/Game/Materials/M_Smoke.M_Smoke"));
	if (SmokeMat.Succeeded()) Mesh->SetMaterial(0, SmokeMat.Object);
}

void ASmokeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// grows in over 0.6 s, shrinks away over the last 2 s
	const float K = FMath::Min(1.f, Age / 0.6f) * FMath::Clamp((Life - Age) / 2.f, 0.f, 1.f);
	Mesh->SetRelativeScale3D(FVector(FMath::Max(0.05f, Weapons::SmokeRadius * 2.f * K)));
	if (Age >= Life) Destroy();
}
