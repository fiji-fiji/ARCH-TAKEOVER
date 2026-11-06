#pragma once

#include "CoreMinimal.h"

class USoundBase;

/** Sound effects by name: /Game/Sounds/<Name> (assets/sounds/<Name>.wav imported by build_level.py).
 *  A missing asset is silent, never fatal. */
namespace GameSounds
{
	USoundBase* Get(const TCHAR* Name);
	/** 3-D one-shot at a world location. */
	void At(const UObject* Ctx, const TCHAR* Name, const FVector& Loc, float Volume = 1.f, float Pitch = 1.f);
	/** 2-D one-shot (UI / the local player's own cues). */
	void UI(const UObject* Ctx, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f);
}
