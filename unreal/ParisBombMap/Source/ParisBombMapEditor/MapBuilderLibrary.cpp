#include "MapBuilderLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Editor.h"

AActor* UMapBuilderLibrary::SpawnBoxVolume(UObject* WorldContextObject, TSubclassOf<AVolume> VolumeClass, FVector Location, FVector Extent)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !*VolumeClass)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVolume* Volume = World->SpawnActor<AVolume>(*VolumeClass, Location, FRotator::ZeroRotator, Params);
	if (!Volume)
	{
		return nullptr;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>(Volume, NAME_None, RF_Transactional);
	Builder->X = Extent.X * 2.0;
	Builder->Y = Extent.Y * 2.0;
	Builder->Z = Extent.Z * 2.0;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->SetActorLocation(Location);
	Volume->PostEditChange();
	Volume->MarkPackageDirty();
	return Volume;
}

bool UMapBuilderLibrary::BuildNavigation(UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return false;
	}
	// FEditorBuildUtils::EditorBuild needs the level-editor UI (crashes under the commandlet), so drive
	// the navigation system directly.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
		NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	}
	if (!NavSys)
	{
		return false;
	}
	// the commandlet world keeps the async-load lock set, which silently skips Build()
	NavSys->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock | ENavigationBuildLock::InitialLock | ENavigationBuildLock::NoUpdateInEditor, UNavigationSystemV1::ELockRemovalRebuildAction::RebuildIfNotInEditor);
	// let the system register the bounds volume(s) and spawn/register the nav data before building
	for (int32 i = 0; i < 20; ++i)
	{
		NavSys->Tick(0.1f);
	}
	NavSys->Build();
	for (int32 i = 0; i < 20; ++i)
	{
		NavSys->Tick(0.1f);
	}
	// pump the async tile generation so the saved level carries the nav mesh
	const double Start = FPlatformTime::Seconds();
	int32 Ticks = 0;
	while (NavSys->IsNavigationBuildInProgress() && FPlatformTime::Seconds() - Start < 900.0)
	{
		NavSys->Tick(0.05f);
		FPlatformProcess::Sleep(0.02f);
		if (++Ticks % 200 == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("[MapBuilder] navigation build in progress... %.0fs"), FPlatformTime::Seconds() - Start);
		}
	}
	for (int32 i = 0; i < 10; ++i)
	{
		NavSys->Tick(0.1f);
	}
	int32 Tiles = -1;
	if (ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()))
	{
		Tiles = Recast->GetNavMeshTilesCount();
	}
	UE_LOG(LogTemp, Display, TEXT("[MapBuilder] navigation build finished in %.1fs, nav data %s, tiles %d"),
		FPlatformTime::Seconds() - Start, NavSys->GetDefaultNavDataInstance() ? TEXT("present") : TEXT("MISSING"), Tiles);
	return Tiles > 0;
}

void UMapBuilderLibrary::SetComplexCollision(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	UBodySetup* Body = Mesh->GetBodySetup();
	if (!Body)
	{
		Mesh->CreateBodySetup();
		Body = Mesh->GetBodySetup();
	}
	if (Body)
	{
		Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
		Body->AggGeom.EmptyElements();
		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();
	}
	Mesh->MarkPackageDirty();
}

int32 UMapBuilderLibrary::GetTriangleCount(UStaticMesh* Mesh)
{
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return 0;
	}
	return Mesh->GetRenderData()->LODResources[0].GetNumTriangles();
}
