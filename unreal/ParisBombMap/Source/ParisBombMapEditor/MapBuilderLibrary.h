#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Volume.h"
#include "MapBuilderLibrary.generated.h"

class UStaticMesh;

/** Editor-only helpers called from Scripts/build_level.py (things the Python API cannot do directly). */
UCLASS()
class PARISBOMBMAPEDITOR_API UMapBuilderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Spawn a volume (NavMeshBoundsVolume, BlockingVolume, ...) with a proper cube brush of the given half-extent. */
	UFUNCTION(BlueprintCallable, Category = "MapBuilder", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBoxVolume(UObject* WorldContextObject, TSubclassOf<AVolume> VolumeClass, FVector Location, FVector Extent);

	/** Build the navigation mesh for the current editor world (synchronous as far as the editor allows). */
	UFUNCTION(BlueprintCallable, Category = "MapBuilder", meta = (WorldContext = "WorldContextObject"))
	static bool BuildNavigation(UObject* WorldContextObject);

	/** Use the render triangles as collision ("Use Complex Collision As Simple"). */
	UFUNCTION(BlueprintCallable, Category = "MapBuilder")
	static void SetComplexCollision(UStaticMesh* Mesh);

	/** Number of triangles in LOD0 (sanity logging). */
	UFUNCTION(BlueprintCallable, Category = "MapBuilder")
	static int32 GetTriangleCount(UStaticMesh* Mesh);
};
