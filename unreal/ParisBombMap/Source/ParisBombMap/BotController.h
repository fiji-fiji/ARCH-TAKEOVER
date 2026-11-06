#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BombTypes.h"
#include "BotController.generated.h"

class AFPSCharacter;
class ABombGameMode;
class ABombSite;
class AWeaponPickup;

UENUM()
enum class EBotTask : uint8
{
	Idle,
	Travel,      // walking to ObjectiveLoc
	Hold,        // standing guard, looking around
	Engage,      // fighting a visible enemy
	Interact,    // planting / defusing
	Fetch,       // going to pick up a dropped gun
};

/** Simple state-machine bot: buys per the FPS Reborn logic, patrols to objectives on the nav mesh, fights what it
 *  sees (frags, smokes and flashes included), plants / defuses, grabs dropped guns. */
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
	/** Cunning bots flash before they peek and always want a flash (FPS Reborn level.cunning). */
	bool bCunning = false;

	void OnRoundStart();
	/** Spend the money (FPS Reborn _botsBuy): called by the game mode at the start of the buy phase. */
	void BuyPhase();

private:
	AFPSCharacter* Me() const;
	ABombGameMode* GM() const;
	void Think();
	AFPSCharacter* FindVisibleEnemy() const;
	bool CanSee(const AFPSCharacter* Other) const;
	void ChooseObjective();
	void GoTo(const FVector& Loc, float Accept = 90.f);
	FVector RandomNavPointNear(const FVector& Origin, float Radius) const;
	AWeaponPickup* NearestDrop(float MaxDist) const;
	void ChooseWeapon();

	EBotTask Task = EBotTask::Idle;
	FVector ObjectiveLoc = FVector::ZeroVector;
	TWeakObjectPtr<ABombSite> TargetSite;
	TWeakObjectPtr<AFPSCharacter> Enemy;
	TWeakObjectPtr<AWeaponPickup> TargetDrop;
	float NextThink = 0.f;
	float EnemySeenSince = -1.f;
	float LastEnemySeen = -100.f;
	float NextBurstAt = 0.f;
	float BurstEndAt = 0.f;
	float HoldUntil = 0.f;
	float NextLookAt = 0.f;
	float NextStrafeAt = 0.f;
	float StuckSince = 0.f;
	float LastNadeTime = -100.f;
	bool bSiteNadesThrown = false;
	FVector LastPos = FVector::ZeroVector;
	bool bWentViaWaypoint = false;
	FRandomStream Rng;
};
