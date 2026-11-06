#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BombHUD.generated.h"

class AFPSCharacter;
class ABombGameState;
class ABombGameMode;
class UFont;
class UTexture2D;

/** Canvas-drawn HUD: crosshair, health / kevlar, money, weapon + ammo, grenades, round / bomb timer, scores,
 *  interaction progress, callout, kill feed, scoreboard, flash white-out, scope, and the buy menu (B).
 *  Every text is sized in pixels of a 1080p screen (scaled with the window height) so it stays readable. */
UCLASS()
class PARISBOMBMAP_API ABombHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

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
	void DrawMinimap(ABombGameMode* GM, ABombGameState* GS, AFPSCharacter* Me, float W, float H);
	void DrawHitFeedback(AFPSCharacter* Me, float W, float H, float Now);
	void DrawShop(ABombGameMode* GM, ABombGameState* GS, AFPSCharacter* Me, float W, float H);
	void DrawCard(ABombGameMode* GM, AFPSCharacter* Me, FName Key, float X, float Y, float CW, float CH, bool bOwned, const FString& Extra);
	static FString FormatTime(float Seconds);
	static FString Money(int32 Amount);

	UPROPERTY() TObjectPtr<UFont> Font;
	float FontHeight = 1.f;   // height of the font at scale 1 (measured once)
	float S = 1.f;            // UI scale = window height / 1080
	FName HoverBox;
	UPROPERTY() TObjectPtr<UTexture2D> MinimapTex;
	bool bMinimapTried = false;
	FVector2D MapMin = FVector2D::ZeroVector, MapMax = FVector2D::ZeroVector;   // map metres covered by the texture
};
