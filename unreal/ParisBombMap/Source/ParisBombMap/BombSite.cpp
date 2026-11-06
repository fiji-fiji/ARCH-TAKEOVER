#include "BombSite.h"
#include "Components/BoxComponent.h"

ABombSite::ABombSite()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);
	Box->SetBoxExtent(FVector(1500.f, 1000.f, 400.f));
	Box->SetCollisionProfileName(TEXT("BombSite"));
	Box->SetCanEverAffectNavigation(false);
	Box->SetGenerateOverlapEvents(false);
	Box->ShapeColor = FColor(255, 90, 0);
	Box->bHiddenInGame = true;
}

void ABombSite::SetExtent(FVector Extent)
{
	Box->SetBoxExtent(Extent);
}

bool ABombSite::IsInside(FVector Point) const
{
	const FVector C = GetActorLocation();
	const FVector E = Box->GetScaledBoxExtent();
	return FMath::Abs(Point.X - C.X) <= E.X && FMath::Abs(Point.Y - C.Y) <= E.Y && FMath::Abs(Point.Z - C.Z) <= E.Z + 200.f;
}

FVector ABombSite::RandomPointInside(FRandomStream& Rng) const
{
	const FVector C = GetActorLocation();
	const FVector E = Box->GetScaledBoxExtent();
	return FVector(C.X + Rng.FRandRange(-E.X * 0.7f, E.X * 0.7f), C.Y + Rng.FRandRange(-E.Y * 0.7f, E.Y * 0.7f), C.Z);
}
