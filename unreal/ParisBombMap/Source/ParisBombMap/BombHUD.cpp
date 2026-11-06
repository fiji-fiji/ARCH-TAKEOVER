#include "BombHUD.h"
#include "BombGameState.h"
#include "BombGameMode.h"
#include "BombPlayerState.h"
#include "FPSCharacter.h"
#include "BombSite.h"
#include "BombActor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

FString ABombHUD::FormatTime(float Seconds)
{
	const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
}

void ABombHUD::DrawTextAt(const FString& Text, float X, float Y, UFont* Font, float Scale, const FLinearColor& Color, bool bShadow)
{
	if (bShadow)
	{
		DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.75f), X + 2.f, Y + 2.f, Font, Scale);
	}
	DrawText(Text, Color, X, Y, Font, Scale);
}

void ABombHUD::DrawCenteredText(const FString& Text, float CX, float Y, UFont* Font, float Scale, const FLinearColor& Color, bool bShadow)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawTextAt(Text, CX - W * 0.5f, Y, Font, Scale, Color, bShadow);
}

void ABombHUD::DrawBar(float X, float Y, float W, float H, float Fraction, const FLinearColor& Fill, const FLinearColor& Back)
{
	DrawRect(Back, X, Y, W, H);
	DrawRect(Fill, X + 2.f, Y + 2.f, (W - 4.f) * FMath::Clamp(Fraction, 0.f, 1.f), H - 4.f);
}

void ABombHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 1.6f);   // UI scale
	UFont* Big = GEngine->GetLargeFont();
	UFont* Med = GEngine->GetMediumFont();
	UFont* Small = GEngine->GetSmallFont();

	ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
	APlayerController* PC = GetOwningPlayerController();
	AFPSCharacter* Me = PC ? Cast<AFPSCharacter>(PC->GetPawn()) : nullptr;
	if (!GS) return;

	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.6f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.45f);
	const FLinearColor ColA = TeamColor(EBombTeam::Attackers);
	const FLinearColor ColD = TeamColor(EBombTeam::Defenders);

	// ---------------------------------------------------------------- top centre: scores + timer
	{
		const float PW = 420.f * S, PH = 64.f * S;
		const float PX = W * 0.5f - PW * 0.5f, PY = 12.f * S;
		DrawRect(Panel, PX, PY, PW, PH);
		DrawRect(ColA * FLinearColor(1, 1, 1, 0.85f), PX, PY, 90.f * S, PH);
		DrawRect(ColD * FLinearColor(1, 1, 1, 0.85f), PX + PW - 90.f * S, PY, 90.f * S, PH);
		DrawCenteredText(FString::FromInt(GS->ScoreAttackers), PX + 45.f * S, PY + 8.f * S, Big, 2.0f * S, White);
		DrawCenteredText(FString::FromInt(GS->ScoreDefenders), PX + PW - 45.f * S, PY + 8.f * S, Big, 2.0f * S, White);
		FString Timer;
		FLinearColor TimerCol = White;
		if (GS->Phase == ERoundPhase::Live && GS->bBombPlanted)
		{
			Timer = FString::Printf(TEXT("BOMB %s"), *FormatTime(GS->BombTimeRemaining));
			TimerCol = FLinearColor(1.f, 0.25f, 0.2f);
		}
		else if (GS->Phase == ERoundPhase::Live || GS->Phase == ERoundPhase::Freeze)
		{
			Timer = FormatTime(GS->PhaseTimeRemaining);
		}
		else
		{
			Timer = TEXT("--:--");
		}
		DrawCenteredText(Timer, W * 0.5f, PY + 6.f * S, Big, 1.9f * S, TimerCol);
		DrawCenteredText(FString::Printf(TEXT("ROUND %d   %d v %d alive"), GS->RoundNumber, GS->AliveAttackers, GS->AliveDefenders), W * 0.5f, PY + 42.f * S, Small, 1.0f * S, Dim);
		DrawCenteredText(TEXT("ATK"), PX + 45.f * S, PY + PH - 16.f * S, Small, 0.9f * S, White);
		DrawCenteredText(TEXT("DEF"), PX + PW - 45.f * S, PY + PH - 16.f * S, Small, 0.9f * S, White);
	}

	// ---------------------------------------------------------------- kill feed (top right)
	{
		float Y = 16.f * S;
		for (int32 i = GS->KillFeed.Num() - 1; i >= 0; --i)
		{
			const auto& E = GS->KillFeed[i];
			float TW = 0.f, TH = 0.f;
			GetTextSize(E.Text, TW, TH, Med, 1.0f * S);
			DrawRect(Panel, W - TW - 28.f * S, Y - 3.f * S, TW + 16.f * S, TH + 6.f * S);
			DrawTextAt(E.Text, W - TW - 20.f * S, Y, Med, 1.0f * S, E.Color);
			Y += TH + 8.f * S;
		}
	}

	// ---------------------------------------------------------------- phase banners
	{
		FString Banner;
		FLinearColor BCol = White;
		switch (GS->Phase)
		{
		case ERoundPhase::Warmup: Banner = TEXT("WARM-UP"); break;
		case ERoundPhase::Freeze:
			Banner = FString::Printf(TEXT("ROUND %d  -  you are %s"), GS->RoundNumber, Me ? *TeamName(Me->GetTeam()) : TEXT("spectating"));
			BCol = Me ? TeamColor(Me->GetTeam()) : White;
			break;
		case ERoundPhase::PostRound: Banner = GS->RoundResultText; break;
		case ERoundPhase::MatchOver: Banner = GS->MatchResultText; break;
		default: break;
		}
		if (!Banner.IsEmpty())
		{
			float TW = 0.f, TH = 0.f;
			GetTextSize(Banner, TW, TH, Big, 1.4f * S);
			DrawRect(Panel, W * 0.5f - TW * 0.5f - 24.f * S, H * 0.22f - 8.f * S, TW + 48.f * S, TH + 16.f * S);
			DrawCenteredText(Banner, W * 0.5f, H * 0.22f, Big, 1.4f * S, BCol);
			if (GS->Phase == ERoundPhase::Freeze)
			{
				const FString Obj = (Me && Me->GetTeam() == EBombTeam::Attackers) ? TEXT("Plant the bomb at A or B, or eliminate the defenders")
				                                                                   : TEXT("Defend both sites. Defuse the bomb if it is planted");
				DrawCenteredText(Obj, W * 0.5f, H * 0.22f + TH + 20.f * S, Med, 1.0f * S, Dim);
			}
		}
		if (GS->Phase == ERoundPhase::Live && GS->bBombPlanted)
		{
			DrawCenteredText(FString::Printf(TEXT("BOMB PLANTED AT %s"), *GS->PlantedSiteId), W * 0.5f, 92.f * S, Med, 1.1f * S, FLinearColor(1.f, 0.3f, 0.2f));
		}
	}

	// ---------------------------------------------------------------- player elements
	if (Me && !Me->bDead)
	{
		// crosshair
		const float CX = W * 0.5f, CY = H * 0.5f;
		const float Gap = (4.f + Me->Spread * 4.f + Me->GetVelocity().Size2D() / 120.f) * S;
		const float Len = 11.f * S;
		const float Now = GetWorld()->GetTimeSeconds();
		const FLinearColor CH = (Now - Me->LastHitMarkerTime < 0.12f) ? FLinearColor(1.f, 0.3f, 0.2f) : FLinearColor(0.4f, 1.f, 0.4f);
		const float Th = FMath::Max(2.f, 2.5f * S);
		const FLinearColor Outline(0.f, 0.f, 0.f, 0.6f);
		DrawRect(Outline, CX - Gap - Len - 1.f, CY - Th * 0.5f - 1.f, Len + 2.f, Th + 2.f);
		DrawRect(Outline, CX + Gap - 1.f, CY - Th * 0.5f - 1.f, Len + 2.f, Th + 2.f);
		DrawRect(Outline, CX - Th * 0.5f - 1.f, CY - Gap - Len - 1.f, Th + 2.f, Len + 2.f);
		DrawRect(Outline, CX - Th * 0.5f - 1.f, CY + Gap - 1.f, Th + 2.f, Len + 2.f);
		DrawRect(CH, CX - Gap - Len, CY - Th * 0.5f, Len, Th);
		DrawRect(CH, CX + Gap, CY - Th * 0.5f, Len, Th);
		DrawRect(CH, CX - Th * 0.5f, CY - Gap - Len, Th, Len);
		DrawRect(CH, CX - Th * 0.5f, CY + Gap, Th, Len);

		// damage flash
		if (Now - Me->LastDamageTime < 0.25f)
		{
			DrawRect(FLinearColor(1.f, 0.f, 0.f, 0.22f * (1.f - (Now - Me->LastDamageTime) / 0.25f)), 0, 0, W, H);
		}

		// health (bottom-left)
		const float BX = 24.f * S, BY = H - 70.f * S;
		DrawRect(Panel, BX - 8.f * S, BY - 8.f * S, 300.f * S, 60.f * S);
		DrawTextAt(TEXT("HP"), BX, BY, Small, 1.0f * S, Dim);
		DrawBar(BX + 30.f * S, BY + 2.f * S, 180.f * S, 16.f * S, Me->Health / 100.f, Me->Health > 30.f ? FLinearColor(0.3f, 0.9f, 0.3f) : FLinearColor(1.f, 0.3f, 0.2f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
		DrawTextAt(FString::FromInt(FMath::CeilToInt(Me->Health)), BX + 220.f * S, BY - 4.f * S, Big, 1.2f * S, White);
		if (Me->bHasBomb)
		{
			DrawTextAt(TEXT("[C4]  you carry the bomb"), BX, BY + 26.f * S, Small, 1.0f * S, FLinearColor(1.f, 0.6f, 0.2f));
		}
		else if (GS->Phase == ERoundPhase::Live)
		{
			DrawTextAt(FString::Printf(TEXT("Team %s"), *TeamName(Me->GetTeam())), BX, BY + 26.f * S, Small, 1.0f * S, TeamColor(Me->GetTeam()));
		}

		// ammo (bottom-right)
		const float AX = W - 230.f * S, AY = H - 70.f * S;
		DrawRect(Panel, AX - 8.f * S, AY - 8.f * S, 215.f * S, 60.f * S);
		DrawTextAt(FString::Printf(TEXT("%d"), Me->AmmoInMag), AX, AY - 6.f * S, Big, 1.6f * S, Me->AmmoInMag > 5 ? White : FLinearColor(1.f, 0.4f, 0.3f));
		DrawTextAt(FString::Printf(TEXT("/ %d"), Me->AmmoReserve), AX + 80.f * S, AY + 6.f * S, Med, 1.0f * S, Dim);
		DrawTextAt(Me->bReloading ? TEXT("RELOADING") : TEXT("RIFLE"), AX, AY + 30.f * S, Small, 1.0f * S, Me->bReloading ? FLinearColor::Yellow : Dim);

		// callout + interaction hints (bottom centre)
		FString Callout = GM ? GM->NearestCallout(Me->GetActorLocation()) : FString();
		if (!Callout.IsEmpty())
		{
			DrawCenteredText(Callout.ToUpper(), W * 0.5f, H - 44.f * S, Med, 1.1f * S, Dim);
		}
		if (GS->Phase == ERoundPhase::Live)
		{
			FString Hint;
			if (Me->GetTeam() == EBombTeam::Attackers && Me->bHasBomb && !GS->bBombPlanted && GM)
			{
				for (ABombSite* Site : GM->Sites)
				{
					if (Site && Site->IsInside(Me->GetActorLocation()))
					{
						Hint = FString::Printf(TEXT("Hold E to plant the bomb at %s"), *Site->SiteId);
					}
				}
			}
			if (Me->GetTeam() == EBombTeam::Defenders && GS->bBombPlanted && GM && GM->Bomb)
			{
				if (FVector::Dist(Me->GetActorLocation(), GM->Bomb->GetActorLocation()) < 220.f)
				{
					Hint = TEXT("Hold E to defuse the bomb");
				}
			}
			if (Me->bInteracting && Me->InteractProgress > 0.f)
			{
				DrawBar(W * 0.5f - 120.f * S, H * 0.5f + 60.f * S, 240.f * S, 18.f * S, Me->InteractProgress, FLinearColor(1.f, 0.6f, 0.1f), FLinearColor(0.f, 0.f, 0.f, 0.7f));
				DrawCenteredText(Me->InteractHint, W * 0.5f, H * 0.5f + 82.f * S, Med, 1.0f * S, White);
			}
			else if (!Hint.IsEmpty())
			{
				DrawCenteredText(Hint, W * 0.5f, H * 0.5f + 60.f * S, Med, 1.0f * S, FLinearColor::Yellow);
			}
		}
	}
	else if (Me && Me->bDead)
	{
		DrawCenteredText(TEXT("YOU ARE DEAD"), W * 0.5f, H * 0.36f, Big, 1.6f * S, FLinearColor(1.f, 0.3f, 0.3f));
		DrawCenteredText(TEXT("spectating a teammate until the next round"), W * 0.5f, H * 0.36f + 44.f * S, Med, 1.0f * S, Dim);
	}

	// controls reminder during warm-up / freeze
	if (GS->Phase == ERoundPhase::Warmup || GS->Phase == ERoundPhase::Freeze)
	{
		DrawCenteredText(TEXT("WASD move   Shift sprint   Ctrl crouch   LMB fire   R reload   E plant/defuse   Tab scoreboard   M switch team"), W * 0.5f, H - 20.f * S, Small, 1.0f * S, Dim);
	}

	if (Me && Me->bShowScoreboard)
	{
		DrawScoreboard(GS, W, H);
	}
}

void ABombHUD::DrawScoreboard(ABombGameState* GS, float W, float H)
{
	const float S = FMath::Clamp(H / 1080.f, 0.6f, 1.6f);
	UFont* Med = GEngine->GetMediumFont();
	UFont* Big = GEngine->GetLargeFont();
	const float PW = 720.f * S, PH = 480.f * S;
	const float PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - PH * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), PX, PY, PW, PH);
	DrawCenteredText(FString::Printf(TEXT("ATTACKERS %d   -   %d DEFENDERS"), GS->ScoreAttackers, GS->ScoreDefenders), W * 0.5f, PY + 14.f * S, Big, 1.2f * S, FLinearColor::White);
	for (int32 T = 0; T < 2; ++T)
	{
		const EBombTeam Team = (EBombTeam)T;
		const float ColX = PX + 24.f * S + T * PW * 0.5f;
		float Y = PY + 70.f * S;
		DrawTextAt(TeamName(Team), ColX, Y, Med, 1.1f * S, TeamColor(Team));
		DrawTextAt(TEXT("K    D"), ColX + 230.f * S, Y, Med, 1.1f * S, FLinearColor(1, 1, 1, 0.6f));
		Y += 30.f * S;
		TArray<ABombPlayerState*> List;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (ABombPlayerState* B = Cast<ABombPlayerState>(PS))
			{
				if (B->Team == Team) List.Add(B);
			}
		}
		List.Sort([](const ABombPlayerState& A, const ABombPlayerState& B) { return A.Kills > B.Kills; });
		for (ABombPlayerState* PS : List)
		{
			DrawTextAt(PS->GetPlayerName(), ColX, Y, Med, 1.0f * S, FLinearColor::White);
			DrawTextAt(FString::Printf(TEXT("%d    %d"), PS->Kills, PS->Deaths), ColX + 230.f * S, Y, Med, 1.0f * S, FLinearColor::White);
			Y += 26.f * S;
		}
	}
}
