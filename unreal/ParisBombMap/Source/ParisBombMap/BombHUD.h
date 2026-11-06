#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BombHUD.generated.h"

class AFPSCharacter;
class ABombGameState;
class UFont;

/** Canvas-drawn HUD: crosshair, health, ammo, round/bomb timer, scores, interaction progress, callout, kill feed, scoreboard.
 *  Every text is sized in pixels of a 1080p screen (scaled with the window height) so it stays readable. */
UCLASS()
class PARISBOMBMAP_API ABombHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// text sizes in pixels at 1080p (multiplied by the UI scale)
	static constexpr float PX_SMALL = 22.f;
	static constexpr float PX_BODY = 27.f;
	static constexpr float PX_MED = 32.f;
	static constexpr float PX_BIG = 46.f;
	static constexpr float PX_HUGE = 64.f;

private:
	/** Draw text with its cap height ~ Px pixels. Align: 0 left, 1 centre, 2 right. Returns the text width. */
	float Text(const FString& Str, float X, float Y, float Px, const FLinearColor& Color, int32 Align = 0, bool bShadow = true);
	FVector2D Measure(const FString& Str, float Px) const;
	void DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back);
	void DrawScoreboard(ABombGameState* GS, float W, float H);
	static FString FormatTime(float Seconds);

	UPROPERTY() TObjectPtr<UFont> Font;
	float FontHeight = 1.f;   // height of the font at scale 1 (measured once)
	float S = 1.f;            // UI scale = window height / 1080
};
