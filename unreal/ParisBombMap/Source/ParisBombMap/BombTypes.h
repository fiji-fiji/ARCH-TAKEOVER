#pragma once

#include "CoreMinimal.h"
#include "BombTypes.generated.h"

UENUM(BlueprintType)
enum class EBombTeam : uint8
{
	Attackers = 0 UMETA(DisplayName = "Attackers"),
	Defenders = 1 UMETA(DisplayName = "Defenders"),
	None = 2 UMETA(DisplayName = "None"),
};

UENUM(BlueprintType)
enum class ERoundPhase : uint8
{
	Warmup,
	Freeze,
	Live,
	PostRound,
	MatchOver,
};

inline EBombTeam OtherTeam(EBombTeam T)
{
	return T == EBombTeam::Attackers ? EBombTeam::Defenders : (T == EBombTeam::Defenders ? EBombTeam::Attackers : EBombTeam::None);
}

inline FString TeamName(EBombTeam T)
{
	switch (T)
	{
	case EBombTeam::Attackers: return TEXT("ATTACKERS");
	case EBombTeam::Defenders: return TEXT("DEFENDERS");
	default: return TEXT("NONE");
	}
}

inline FLinearColor TeamColor(EBombTeam T)
{
	return T == EBombTeam::Attackers ? FLinearColor(1.0f, 0.55f, 0.1f) : FLinearColor(0.25f, 0.6f, 1.0f);
}
