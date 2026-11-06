#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BombHUD.generated.h"

class AFPSCharacter;
class ABombGameState;
class UFont;

/** Canvas-drawn HUD: crosshair, health, ammo, round/bomb timer, scores, interaction progress, callout, kill feed, scoreboard. */
UCLASS()
class PARISBOMBMAP_API ABombHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCenteredText(const FString& Text, float CX, float Y, UFont* Font, float Scale, const FLinearColor& Color, bool bShadow = true);
	void DrawTextAt(const FString& Text, float X, float Y, UFont* Font, float Scale, const FLinearColor& Color, bool bShadow = true);
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back);
	void DrawScoreboard(ABombGameState* GS, float W, float H);
	static FString FormatTime(float Seconds);
};
