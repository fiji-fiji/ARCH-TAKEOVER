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
	const int32 Sec = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), Sec / 60, Sec % 60);
}

FVector2D ABombHUD::Measure(const FString& Str, float Px) const
{
	float W = 0.f, H = 0.f;
	const float Scale = Px * S / FontHeight;
	const_cast<ABombHUD*>(this)->GetTextSize(Str, W, H, Font, Scale);
	return FVector2D(W, H);
}

float ABombHUD::Text(const FString& Str, float X, float Y, float Px, const FLinearColor& Color, int32 Align, bool bShadow)
{
	const float Scale = Px * S / FontHeight;
	const FVector2D Size = Measure(Str, Px);
	if (Align == 1) X -= Size.X * 0.5f;
	else if (Align == 2) X -= Size.X;
	if (bShadow)
	{
		const float O = FMath::Max(1.5f, Px * S * 0.06f);
		DrawText(Str, FLinearColor(0.f, 0.f, 0.f, 0.8f), X + O, Y + O, Font, Scale);
	}
	DrawText(Str, Color, X, Y, Font, Scale);
	return Size.X;
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
	S = FMath::Clamp(H / 1080.f, 0.5f, 2.5f);
	if (!Font)
	{
		Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		if (!Font) Font = GEngine->GetLargeFont();
		float TW = 0.f, TH = 0.f;
		GetTextSize(TEXT("Hg"), TW, TH, Font, 1.f);
		FontHeight = FMath::Max(1.f, TH);
	}

	ABombGameState* GS = GetWorld()->GetGameState<ABombGameState>();
	ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
	APlayerController* PC = GetOwningPlayerController();
	AFPSCharacter* Me = PC ? Cast<AFPSCharacter>(PC->GetPawn()) : nullptr;
	if (!GS) return;

	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.7f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor ColA = TeamColor(EBombTeam::Attackers);
	const FLinearColor ColD = TeamColor(EBombTeam::Defenders);
	const float Pad = 14.f * S;

	// ---------------------------------------------------------------- top centre: scores + timer
	{
		const float PW = 560.f * S, PH = 96.f * S;
		const float PX = W * 0.5f - PW * 0.5f, PY = 12.f * S;
		const float Side = 120.f * S;
		DrawRect(Panel, PX, PY, PW, PH);
		DrawRect(ColA * FLinearColor(1, 1, 1, 0.85f), PX, PY, Side, PH);
		DrawRect(ColD * FLinearColor(1, 1, 1, 0.85f), PX + PW - Side, PY, Side, PH);
		Text(FString::FromInt(GS->ScoreAttackers), PX + Side * 0.5f, PY + 4.f * S, PX_BIG + 6.f, White, 1);
		Text(FString::FromInt(GS->ScoreDefenders), PX + PW - Side * 0.5f, PY + 4.f * S, PX_BIG + 6.f, White, 1);
		Text(TEXT("ATK"), PX + Side * 0.5f, PY + PH - 28.f * S, PX_SMALL, White, 1);
		Text(TEXT("DEF"), PX + PW - Side * 0.5f, PY + PH - 28.f * S, PX_SMALL, White, 1);
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
		Text(Timer, W * 0.5f, PY + 2.f * S, PX_BIG, TimerCol, 1);
		Text(FString::Printf(TEXT("ROUND %d    %d v %d alive"), GS->RoundNumber, GS->AliveAttackers, GS->AliveDefenders), W * 0.5f, PY + PH - 30.f * S, PX_SMALL, Dim, 1);
	}

	// ---------------------------------------------------------------- kill feed (top right)
	{
		float Y = 16.f * S;
		for (int32 i = GS->KillFeed.Num() - 1; i >= 0; --i)
		{
			const auto& E = GS->KillFeed[i];
			const FVector2D Sz = Measure(E.Text, PX_BODY);
			DrawRect(Panel, W - Sz.X - Pad * 2.f - 10.f * S, Y - 4.f * S, Sz.X + Pad * 2.f, Sz.Y + 8.f * S);
			Text(E.Text, W - Pad - 10.f * S, Y, PX_BODY, E.Color, 2);
			Y += Sz.Y + 10.f * S;
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
			const FVector2D Sz = Measure(Banner, PX_BIG);
			const float BY = H * 0.2f;
			DrawRect(Panel, W * 0.5f - Sz.X * 0.5f - Pad * 2.f, BY - 8.f * S, Sz.X + Pad * 4.f, Sz.Y + 16.f * S);
			Text(Banner, W * 0.5f, BY, PX_BIG, BCol, 1);
			if (GS->Phase == ERoundPhase::Freeze)
			{
				const FString Obj = (Me && Me->GetTeam() == EBombTeam::Attackers) ? TEXT("Plant the bomb at A or B, or eliminate the defenders")
				                                                                   : TEXT("Defend both sites. Defuse the bomb if it is planted");
				Text(Obj, W * 0.5f, BY + Sz.Y + 22.f * S, PX_BODY, Dim, 1);
			}
		}
		if (GS->Phase == ERoundPhase::Live && GS->bBombPlanted)
		{
			Text(FString::Printf(TEXT("BOMB PLANTED AT %s"), *GS->PlantedSiteId), W * 0.5f, 124.f * S, PX_MED, FLinearColor(1.f, 0.3f, 0.2f), 1);
		}
	}

	// ---------------------------------------------------------------- player elements
	if (Me && !Me->bDead)
	{
		// crosshair
		const float CX = W * 0.5f, CY = H * 0.5f;
		const float Gap = (5.f + Me->Spread * 4.f + Me->GetVelocity().Size2D() / 120.f) * S;
		const float Len = 12.f * S;
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
		{
			const float PW = 400.f * S, PH = 100.f * S;
			const float PX = 20.f * S, PY = H - PH - 20.f * S;
			DrawRect(Panel, PX, PY, PW, PH);
			Text(TEXT("HP"), PX + Pad, PY + 10.f * S, PX_SMALL, Dim);
			DrawBar(PX + Pad + 50.f * S, PY + 14.f * S, 210.f * S, 22.f * S, Me->Health / 100.f, Me->Health > 30.f ? FLinearColor(0.3f, 0.9f, 0.3f) : FLinearColor(1.f, 0.3f, 0.2f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
			Text(FString::FromInt(FMath::CeilToInt(Me->Health)), PX + PW - Pad, PY + 2.f * S, PX_BIG, White, 2);
			if (Me->bHasBomb)
			{
				Text(TEXT("[C4]  you carry the bomb"), PX + Pad, PY + PH - 38.f * S, PX_SMALL, FLinearColor(1.f, 0.6f, 0.2f));
			}
			else if (GS->Phase == ERoundPhase::Live)
			{
				Text(FString::Printf(TEXT("Team %s"), *TeamName(Me->GetTeam())), PX + Pad, PY + PH - 38.f * S, PX_SMALL, TeamColor(Me->GetTeam()));
			}
		}

		// ammo (bottom-right)
		{
			const float PW = 300.f * S, PH = 100.f * S;
			const float PX = W - PW - 20.f * S, PY = H - PH - 20.f * S;
			DrawRect(Panel, PX, PY, PW, PH);
			const FString Mag = FString::Printf(TEXT("%d"), Me->AmmoInMag);
			const float MW = Measure(Mag, PX_HUGE).X;
			Text(Mag, PX + Pad, PY - 2.f * S, PX_HUGE, Me->AmmoInMag > 5 ? White : FLinearColor(1.f, 0.4f, 0.3f));
			Text(FString::Printf(TEXT("/ %d"), Me->AmmoReserve), PX + Pad + MW + 12.f * S, PY + 22.f * S, PX_MED, Dim);
			Text(Me->bReloading ? TEXT("RELOADING") : TEXT("RIFLE"), PX + PW - Pad, PY + PH - 36.f * S, PX_SMALL, Me->bReloading ? FLinearColor::Yellow : Dim, 2);
		}

		// callout + interaction hints (bottom centre)
		FString Callout = GM ? GM->NearestCallout(Me->GetActorLocation()) : FString();
		if (!Callout.IsEmpty())
		{
			const bool bKeys = GS->Phase == ERoundPhase::Warmup || GS->Phase == ERoundPhase::Freeze;
			Text(Callout.ToUpper(), W * 0.5f, H - (bKeys ? 110.f : 60.f) * S, PX_MED, Dim, 1);
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
				DrawBar(W * 0.5f - 160.f * S, H * 0.5f + 70.f * S, 320.f * S, 24.f * S, Me->InteractProgress, FLinearColor(1.f, 0.6f, 0.1f), FLinearColor(0.f, 0.f, 0.f, 0.7f));
				Text(Me->InteractHint, W * 0.5f, H * 0.5f + 100.f * S, PX_BODY, White, 1);
			}
			else if (!Hint.IsEmpty())
			{
				Text(Hint, W * 0.5f, H * 0.5f + 70.f * S, PX_BODY, FLinearColor::Yellow, 1);
			}
		}
	}
	else if (Me && Me->bDead)
	{
		Text(TEXT("YOU ARE DEAD"), W * 0.5f, H * 0.34f, PX_HUGE, FLinearColor(1.f, 0.3f, 0.3f), 1);
		Text(TEXT("spectating a teammate until the next round"), W * 0.5f, H * 0.34f + 80.f * S, PX_BODY, Dim, 1);
	}

	// controls reminder during warm-up / freeze
	if (GS->Phase == ERoundPhase::Warmup || GS->Phase == ERoundPhase::Freeze)
	{
		const FString Keys = TEXT("WASD move    Shift sprint    Ctrl crouch    LMB fire    R reload    E plant / defuse    Tab scoreboard    M switch team");
		const FVector2D Sz = Measure(Keys, PX_SMALL);
		DrawRect(Panel, W * 0.5f - Sz.X * 0.5f - Pad, H - Sz.Y - 24.f * S, Sz.X + Pad * 2.f, Sz.Y + 10.f * S);
		Text(Keys, W * 0.5f, H - Sz.Y - 19.f * S, PX_SMALL, Dim, 1);
	}

	if (Me && Me->bShowScoreboard)
	{
		DrawScoreboard(GS, W, H);
	}
}

void ABombHUD::DrawScoreboard(ABombGameState* GS, float W, float H)
{
	const float PW = 980.f * S, PH = 620.f * S;
	const float PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - PH * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.82f), PX, PY, PW, PH);
	Text(FString::Printf(TEXT("ATTACKERS %d   -   %d DEFENDERS"), GS->ScoreAttackers, GS->ScoreDefenders), W * 0.5f, PY + 18.f * S, PX_BIG, FLinearColor::White, 1);
	for (int32 T = 0; T < 2; ++T)
	{
		const EBombTeam Team = (EBombTeam)T;
		const float ColX = PX + 30.f * S + T * PW * 0.5f;
		float Y = PY + 100.f * S;
		Text(TeamName(Team), ColX, Y, PX_MED, TeamColor(Team));
		Text(TEXT("K      D"), ColX + 300.f * S, Y, PX_MED, FLinearColor(1, 1, 1, 0.6f));
		Y += 48.f * S;
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
			Text(PS->GetPlayerName(), ColX, Y, PX_BODY, FLinearColor::White);
			Text(FString::Printf(TEXT("%d      %d"), PS->Kills, PS->Deaths), ColX + 300.f * S, Y, PX_BODY, FLinearColor::White);
			Y += 38.f * S;
		}
	}
}
