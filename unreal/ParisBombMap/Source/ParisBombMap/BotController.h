#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BombTypes.h"
#include "BotController.generated.h"

class AFPSCharacter;
class ABombGameMode;
class ABombSite;

UENUM()
enum class EBotTask : uint8
{
	Idle,
	Travel,      // walking to ObjectiveLoc
	Hold,        // standing guard, looking around
	Engage,      // fighting a visible enemy
	Interact,    // planting / defusing
};

/** Simple state-machine bot: patrols to objectives on the nav mesh, fights what it sees, plants / defuses. */
UCLASS()
class PARISBOMBMAP_API ABotController : public AAIController
{
	GENERATED_BODY()

public:
	ABotController();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** 0 = terrible, 1 = perfect. Randomised per bot. */
	float Skill = 0.5f;

	void OnRoundStart();

private:
	AFPSCharacter* Me() const;
	ABombGameMode* GM() const;
	void Think();
	AFPSCharacter* FindVisibleEnemy() const;
	bool CanSee(const AFPSCharacter* Other) const;
	void ChooseObjective();
	void GoTo(const FVector& Loc, float Accept = 90.f);
	FVector RandomNavPointNear(const FVector& Origin, float Radius) const;

	EBotTask Task = EBotTask::Idle;
	FVector ObjectiveLoc = FVector::ZeroVector;
	TWeakObjectPtr<ABombSite> TargetSite;
	TWeakObjectPtr<AFPSCharacter> Enemy;
	float NextThink = 0.f;
	float EnemySeenSince = -1.f;
	float LastEnemySeen = -100.f;
	float NextBurstAt = 0.f;
	float BurstEndAt = 0.f;
	float HoldUntil = 0.f;
	float NextLookAt = 0.f;
	float NextStrafeAt = 0.f;
	float StuckSince = 0.f;
	FVector LastPos = FVector::ZeroVector;
	bool bWentViaWaypoint = false;
	FRandomStream Rng;
};
