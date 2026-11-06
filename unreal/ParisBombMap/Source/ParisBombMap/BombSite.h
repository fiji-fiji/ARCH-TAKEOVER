#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BombSite.generated.h"

class UBoxComponent;

/** Axis-aligned bomb site volume (A or B). Placed by Scripts/build_level.py. */
UCLASS()
class PARISBOMBMAP_API ABombSite : public AActor
{
	GENERATED_BODY()

public:
	ABombSite();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb Site")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb Site")
	FString SiteId = TEXT("A");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bomb Site")
	FString Label;

	UFUNCTION(BlueprintCallable, Category = "Bomb Site")
	void SetExtent(FVector Extent);

	UFUNCTION(BlueprintCallable, Category = "Bomb Site")
	bool IsInside(FVector Point) const;

	/** A random walkable-ish point inside the site footprint (XY only). */
	FVector RandomPointInside(FRandomStream& Rng) const;
};
