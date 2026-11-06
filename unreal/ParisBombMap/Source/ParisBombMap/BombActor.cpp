#include "BombActor.h"
#include "GameSounds.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "FPSCharacter.h"
#include "Kismet/GameplayStatics.h"

ABombActor::ABombActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.38f, 0.26f, 0.12f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCanEverAffectNavigation(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Mesh);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Light->SetIntensity(0.f);
	Light->SetLightColor(FLinearColor(1.f, 0.05f, 0.05f));
	Light->SetAttenuationRadius(600.f);
	Light->SetCastShadows(false);

	Pickup = CreateDefaultSubobject<USphereComponent>(TEXT("Pickup"));
	Pickup->SetupAttachment(Mesh);
	Pickup->SetSphereRadius(120.f);
	Pickup->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Pickup->SetCollisionResponseToAllChannels(ECR_Ignore);
	Pickup->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Pickup->SetCanEverAffectNavigation(false);
}

void ABombActor::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Bomb.M_Bomb")))
	{
		Mesh->SetMaterial(0, M);
	}
	Pickup->OnComponentBeginOverlap.AddDynamic(this, &ABombActor::OnPickupOverlap);
	// the scale on the root affects children: undo it on the sphere so the pickup radius is in world units
	Pickup->SetWorldScale3D(FVector(1.f));
	Light->SetWorldScale3D(FVector(1.f));
}

void ABombActor::SetPlanted(bool bInPlanted)
{
	bPlanted = bInPlanted;
	DefuseProgress = 0.f;
	Pickup->SetCollisionEnabled(bPlanted ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	Light->SetIntensity(bPlanted ? 3000.f : 0.f);
}

void ABombActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlanted)
	{
		// dropped bomb: slow pulse so it can be found
		BlinkPhase += DeltaSeconds * 2.f;
		Light->SetIntensity(400.f + 400.f * FMath::Sin(BlinkPhase));
		return;
	}
	const ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	const float Remaining = GS ? GS->BombTimeRemaining : 30.f;
	const float Rate = FMath::Clamp(1.f + (45.f - Remaining) * 0.25f, 1.f, 12.f);   // faster beeps near the end
	BlinkPhase += DeltaSeconds * Rate * 6.f;
	const bool bOn = FMath::Fmod(BlinkPhase, 6.28f) < 1.2f;
	Light->SetIntensity(bOn ? 5000.f : 200.f);
	if (bOn && !bWasOn)
	{
		GameSounds::At(this, TEXT("S_BombBeep"), GetActorLocation(), 1.0f, FMath::Clamp(0.9f + (45.f - Remaining) * 0.01f, 0.9f, 1.4f));
	}
	bWasOn = bOn;
}

void ABombActor::OnPickupOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                 int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPlanted)
	{
		return;
	}
	AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor);
	if (!C || C->bDead || C->GetTeam() != EBombTeam::Attackers)
	{
		return;
	}
	if (ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->PickupBomb(C);
	}
}
