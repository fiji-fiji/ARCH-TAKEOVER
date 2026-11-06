#include "BombHUD.h"
#include "BombGameState.h"
#include "BombGameMode.h"
#include "BombPlayerState.h"
#include "FPSCharacter.h"
#include "FPSPlayerController.h"
#include "BombSite.h"
#include "BombActor.h"
#include "Weapons.h"
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

FString ABombHUD::Money(int32 Amount)
{
	if (Amount >= 1000) return FString::Printf(TEXT("$%d %03d"), Amount / 1000, Amount % 1000);
	return FString::Printf(TEXT("$%d"), Amount);
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
	AFPSPlayerController* FPC = Cast<AFPSPlayerController>(PC);
	AFPSCharacter* Me = PC ? Cast<AFPSCharacter>(PC->GetPawn()) : nullptr;
	ABombPlayerState* MyPS = Me ? Me->GetPlayerState<ABombPlayerState>() : nullptr;
	if (!GS) return;
	const float Now = GetWorld()->GetTimeSeconds();

	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Dim(1.f, 1.f, 1.f, 0.7f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.5f);
	const FLinearColor Green(0.4f, 0.95f, 0.45f);
	const FLinearColor ColA = TeamColor(EBombTeam::Attackers);
	const FLinearColor ColD = TeamColor(EBombTeam::Defenders);
	const float Pad = 14.f * S;

	// ---------------------------------------------------------------- flash white-out (under everything else)
	if (Me && !Me->bDead && Now < Me->BlindUntil)
	{
		const float Left = Me->BlindUntil - Now;
		const float A = FMath::Clamp(Left / FMath::Max(0.5f, Me->BlindDuration * 0.5f), 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 1.f, 1.f, A), 0, 0, W, H);
	}

	// ---------------------------------------------------------------- scope
	if (Me && !Me->bDead && Me->bScoped)
	{
		const FLinearColor Black(0.f, 0.f, 0.f, 0.92f);
		const float R = H * 0.46f;
		const float CX = W * 0.5f, CY = H * 0.5f;
		DrawRect(Black, 0, 0, CX - R, H);
		DrawRect(Black, CX + R, 0, W - CX - R, H);
		DrawRect(Black, CX - R, 0, 2 * R, CY - R);
		DrawRect(Black, CX - R, CY + R, 2 * R, H - CY - R);
		// corners of the square outside the circle
		const int32 N = 32;
		for (int32 i = 0; i < N; ++i)
		{
			const float Y0 = CY - R + (2 * R) * i / N, Y1 = CY - R + (2 * R) * (i + 1) / N;
			const float Ym = (Y0 + Y1) * 0.5f;
			const float Half = FMath::Sqrt(FMath::Max(0.f, R * R - (Ym - CY) * (Ym - CY)));
			DrawRect(Black, CX - R, Y0, R - Half, Y1 - Y0);
			DrawRect(Black, CX + Half, Y0, R - Half, Y1 - Y0);
		}
		DrawRect(FLinearColor(0, 0, 0, 0.9f), CX - R, CY - 1.f, 2 * R, 2.f);
		DrawRect(FLinearColor(0, 0, 0, 0.9f), CX - 1.f, CY - R, 2.f, 2 * R);
	}

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
		else if (GS->Phase == ERoundPhase::Live)
		{
			Timer = FormatTime(GS->PhaseTimeRemaining);
		}
		else if (GS->Phase == ERoundPhase::Freeze)
		{
			Timer = FString::Printf(TEXT("BUY %d"), FMath::Max(0, FMath::CeilToInt(GS->PhaseTimeRemaining)));
			TimerCol = FLinearColor(0.6f, 1.f, 0.6f);
		}
		else
		{
			Timer = TEXT("--:--");
		}
		Text(Timer, W * 0.5f, PY + 2.f * S, PX_BIG, TimerCol, 1);
		const FString Sub = FString::Printf(TEXT("ROUND %d%s    %d v %d alive"), GS->RoundNumber, GS->bOvertime ? TEXT(" OT") : TEXT(""), GS->AliveAttackers, GS->AliveDefenders);
		Text(Sub, W * 0.5f, PY + PH - 30.f * S, PX_SMALL, Dim, 1);
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
			Banner = FString::Printf(TEXT("ROUND %d%s  -  you are %s"), GS->RoundNumber, GS->bOvertime ? TEXT(" (OVERTIME)") : TEXT(""), Me ? *TeamName(Me->GetTeam()) : TEXT("spectating"));
			BCol = Me ? TeamColor(Me->GetTeam()) : White;
			break;
		case ERoundPhase::PostRound: Banner = GS->RoundResultText; break;
		case ERoundPhase::MatchOver: Banner = GS->MatchResultText; break;
		default: break;
		}
		if (!Banner.IsEmpty() && !(FPC && FPC->bShopOpen))
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
				Text(TEXT("Press B to buy weapons"), W * 0.5f, BY + Sz.Y + 60.f * S, PX_BODY, Green, 1);
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
		// crosshair (not while scoped: the scope has its own lines)
		if (!Me->bScoped)
		{
			const float CX = W * 0.5f, CY = H * 0.5f;
			const float Gap = (5.f + Me->Spread * 10.f) * S;
			const float Len = 12.f * S;
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
		}

		// damage flash
		if (Now - Me->LastDamageTime < 0.25f)
		{
			DrawRect(FLinearColor(1.f, 0.f, 0.f, 0.22f * (1.f - (Now - Me->LastDamageTime) / 0.25f)), 0, 0, W, H);
		}

		// health + kevlar + money (bottom-left)
		{
			const float PW = 420.f * S, PH = 150.f * S;
			const float PX = 20.f * S, PY = H - PH - 20.f * S;
			DrawRect(Panel, PX, PY, PW, PH);
			Text(TEXT("HP"), PX + Pad, PY + 10.f * S, PX_SMALL, Dim);
			DrawBar(PX + Pad + 50.f * S, PY + 14.f * S, 210.f * S, 22.f * S, Me->Health / 100.f, Me->Health > 30.f ? FLinearColor(0.3f, 0.9f, 0.3f) : FLinearColor(1.f, 0.3f, 0.2f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
			Text(FString::FromInt(FMath::CeilToInt(Me->Health)), PX + PW - Pad, PY + 2.f * S, PX_BIG, White, 2);
			if (Me->Armor > 0)
			{
				Text(TEXT("KEV"), PX + Pad, PY + 46.f * S, PX_SMALL, Dim);
				DrawBar(PX + Pad + 50.f * S, PY + 50.f * S, 210.f * S, 16.f * S, Me->Armor / 50.f, FLinearColor(0.4f, 0.7f, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
				Text(FString::FromInt(Me->Armor), PX + PW - Pad, PY + 42.f * S, PX_MED, FLinearColor(0.6f, 0.8f, 1.f), 2);
			}
			if (MyPS)
			{
				Text(Money(MyPS->Money), PX + Pad, PY + PH - 50.f * S, PX_MED, Green);
				if (Now - MyPS->LastGainTime < 2.5f)
				{
					const float A = FMath::Clamp((2.5f - (Now - MyPS->LastGainTime)) / 1.f, 0.f, 1.f);
					Text(FString::Printf(TEXT("+%s"), *Money(MyPS->LastGain)), PX + Pad + 170.f * S, PY + PH - 50.f * S - (1.f - A) * 20.f * S, PX_MED, FLinearColor(0.6f, 1.f, 0.6f, A));
				}
			}
			if (Me->bHasBomb)
			{
				Text(TEXT("[C4]  you carry the bomb (4)"), PX + PW - Pad, PY + PH - 44.f * S, PX_SMALL, FLinearColor(1.f, 0.6f, 0.2f), 2);
			}
			else
			{
				Text(FString::Printf(TEXT("Team %s"), *TeamName(Me->GetTeam())), PX + PW - Pad, PY + PH - 44.f * S, PX_SMALL, TeamColor(Me->GetTeam()), 2);
			}
		}

		// weapon + ammo + grenades (bottom-right)
		{
			const float PW = 420.f * S, PH = 150.f * S;
			const float PX = W - PW - 20.f * S, PY = H - PH - 20.f * S;
			DrawRect(Panel, PX, PY, PW, PH);
			const FWeaponDef* D = Me->HeldDef();
			FGunState* G = Me->HeldGun();
			FString Name = TEXT("");
			if (Me->Held == EHeldSlot::Bomb) Name = TEXT("C4  -  hold 4 in a site to plant");
			else if (Me->Held == EHeldSlot::Nade) Name = Weapons::NameOf(Me->NadeKind) + TEXT("  -  LMB throw, RMB lob");
			else if (D) Name = D->Name.ToUpper();
			if (G && D && !D->bMelee)
			{
				const FString Mag = FString::Printf(TEXT("%d"), G->Ammo);
				const float MW = Measure(Mag, PX_HUGE).X;
				Text(Mag, PX + Pad, PY - 2.f * S, PX_HUGE, G->Ammo > FMath::Max(1, D->MagSize / 5) ? White : FLinearColor(1.f, 0.4f, 0.3f));
				Text(FString::Printf(TEXT("/ %d"), G->Reserve), PX + Pad + MW + 12.f * S, PY + 22.f * S, PX_MED, Dim);
				if (G->Busy()) Text(TEXT("RELOADING"), PX + PW - Pad, PY + 10.f * S, PX_BODY, FLinearColor::Yellow, 2);
			}
			Text(Name, PX + PW - Pad, PY + 64.f * S, PX_BODY, Dim, 2);
			// slots: 1 main · 2 side · 3 knife · 4 bomb · nades with pips
			float X = PX + Pad;
			const float YS = PY + PH - 44.f * S;
			auto Slot = [&](const FString& Label, bool bHave, bool bActive)
			{
				const FLinearColor C = bActive ? Green : (bHave ? Dim : FLinearColor(1, 1, 1, 0.25f));
				X += Text(Label, X, YS, PX_SMALL, C) + 18.f * S;
			};
			Slot(FString::Printf(TEXT("1 %s"), Me->HasMain() ? *Me->Guns[0].Def->Name : TEXT("-")), Me->HasMain(), Me->Held == EHeldSlot::Main);
			Slot(FString::Printf(TEXT("2 %s"), Me->Guns[1].IsValid() ? *Me->Guns[1].Def->Name : TEXT("-")), true, Me->Held == EHeldSlot::Side);
			Slot(TEXT("3 Knife"), true, Me->Held == EHeldSlot::Knife);
			if (Me->bHasBomb) Slot(TEXT("4 C4"), true, Me->Held == EHeldSlot::Bomb);
			X = PX + Pad;
			const float YN = PY + PH - 20.f * S - PX_SMALL * S;
			for (const FName& K : Weapons::NadeOrder())
			{
				const int32 N = Me->NadeCount(K);
				if (N <= 0) continue;
				const bool bActive = Me->Held == EHeldSlot::Nade && Me->NadeKind == K;
				FString Pips; for (int32 i = 0; i < N; ++i) Pips += TEXT("o");
				X += Text(FString::Printf(TEXT("%s %s"), *Weapons::NameOf(K), *Pips), X, YN, PX_SMALL, bActive ? Green : Dim) + 18.f * S;
			}
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
						Hint = FString::Printf(TEXT("Hold 4 to plant the bomb at %s"), *Site->SiteId);
					}
				}
			}
			if (Me->GetTeam() == EBombTeam::Defenders && GS->bBombPlanted && GM && GM->Bomb)
			{
				if (FVector::Dist(Me->GetActorLocation(), GM->Bomb->GetActorLocation()) < 220.f)
				{
					Hint = TEXT("Hold 4 to defuse the bomb");
				}
			}
			if (Hint.IsEmpty() && GS->bBombDropped && GM && GM->Bomb && Me->GetTeam() == EBombTeam::Attackers && !Me->bHasBomb
				&& FVector::Dist(Me->GetActorLocation(), GM->Bomb->GetActorLocation()) < 600.f)
			{
				Hint = TEXT("The bomb is on the ground: walk over it to pick it up");
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
		else if (GS->Phase == ERoundPhase::Freeze && GS->bBombDropped && Me->GetTeam() == EBombTeam::Attackers && !Me->bHasBomb)
		{
			Text(TEXT("The bomb is on the ground in front of you: walk over it to take it"), W * 0.5f, H * 0.5f + 70.f * S, PX_BODY, FLinearColor::Yellow, 1);
		}
	}
	else if (Me && Me->bDead)
	{
		Text(TEXT("YOU ARE DEAD"), W * 0.5f, H * 0.34f, PX_HUGE, FLinearColor(1.f, 0.3f, 0.3f), 1);
		Text(TEXT("spectating a teammate until the next round"), W * 0.5f, H * 0.34f + 80.f * S, PX_BODY, Dim, 1);
	}

	// controls reminder during warm-up / freeze
	if ((GS->Phase == ERoundPhase::Warmup || GS->Phase == ERoundPhase::Freeze) && !(FPC && FPC->bShopOpen))
	{
		const FString Keys = TEXT("WASD move   Shift walk   Ctrl crouch   LMB fire   RMB zoom   R reload   1-3 weapons   4 bomb (hold to plant / defuse)   Q E C grenades   F pick up   B buy   Tab scores   M switch team");
		const FVector2D Sz = Measure(Keys, PX_SMALL);
		DrawRect(Panel, W * 0.5f - Sz.X * 0.5f - Pad, H - Sz.Y - 24.f * S, Sz.X + Pad * 2.f, Sz.Y + 10.f * S);
		Text(Keys, W * 0.5f, H - Sz.Y - 19.f * S, PX_SMALL, Dim, 1);
	}

	if (Me && Me->bShowScoreboard)
	{
		DrawScoreboard(GS, W, H);
	}
	if (FPC && FPC->bShopOpen && GM && Me)
	{
		DrawShop(GM, GS, Me, W, H);
	}
}

void ABombHUD::DrawScoreboard(ABombGameState* GS, float W, float H)
{
	const float PW = 1100.f * S, PH = 620.f * S;
	const float PX = W * 0.5f - PW * 0.5f, PY = H * 0.5f - PH * 0.5f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.82f), PX, PY, PW, PH);
	Text(FString::Printf(TEXT("ATTACKERS %d   -   %d DEFENDERS"), GS->ScoreAttackers, GS->ScoreDefenders), W * 0.5f, PY + 18.f * S, PX_BIG, FLinearColor::White, 1);
	const EBombTeam MyTeam = GS->HumanTeam;
	for (int32 T = 0; T < 2; ++T)
	{
		const EBombTeam Team = (EBombTeam)T;
		const float ColX = PX + 30.f * S + T * PW * 0.5f;
		float Y = PY + 100.f * S;
		Text(TeamName(Team), ColX, Y, PX_MED, TeamColor(Team));
		Text(TEXT("K      D"), ColX + 280.f * S, Y, PX_MED, FLinearColor(1, 1, 1, 0.6f));
		if (Team == MyTeam) Text(TEXT("$"), ColX + 440.f * S, Y, PX_MED, FLinearColor(1, 1, 1, 0.6f));
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
			Text(FString::Printf(TEXT("%d      %d"), PS->Kills, PS->Deaths), ColX + 280.f * S, Y, PX_BODY, FLinearColor::White);
			if (Team == MyTeam) Text(Money(PS->Money), ColX + 440.f * S, Y, PX_BODY, FLinearColor(0.6f, 1.f, 0.6f));   // money only for your team
			Y += 38.f * S;
		}
	}
}

// ---------------------------------------------------------------- buy menu (FPS Reborn shop.js, drawn on the canvas)

void ABombHUD::DrawCard(ABombGameMode* GM, AFPSCharacter* Me, FName Key, float X, float Y, float CW, float CH, bool bOwned, const FString& Extra)
{
	const bool bCan = GM->CanAfford(Me, Key) || bOwned;
	const FName Box(*FString::Printf(TEXT("buy:%s"), *Key.ToString()));
	const bool bHover = HoverBox == Box;
	FLinearColor Back = bOwned ? FLinearColor(0.1f, 0.3f, 0.12f, 0.9f) : FLinearColor(0.12f, 0.12f, 0.14f, 0.9f);
	if (bHover) Back = Back * 1.6f;
	if (!bCan) Back = FLinearColor(0.08f, 0.08f, 0.08f, 0.8f);
	DrawRect(bHover ? FLinearColor(0.9f, 0.9f, 0.9f, 0.9f) : (bOwned ? FLinearColor(0.4f, 0.9f, 0.45f, 0.9f) : FLinearColor(0.35f, 0.35f, 0.38f, 0.9f)), X - 2.f, Y - 2.f, CW + 4.f, CH + 4.f);
	DrawRect(Back, X, Y, CW, CH);
	AddHitBox(FVector2D(X, Y), FVector2D(CW, CH), Box, true, 1);
	const FLinearColor Txt = bCan ? FLinearColor::White : FLinearColor(1, 1, 1, 0.35f);
	const float Pad = 12.f * S;
	const int32 Price = Weapons::PriceOf(Key);
	Text(Weapons::NameOf(Key), X + Pad, Y + 8.f * S, PX_BODY, Txt);
	Text(Price > 0 ? Money(Price) : TEXT("FREE"), X + CW - Pad, Y + 8.f * S, PX_BODY, bCan ? FLinearColor(0.6f, 1.f, 0.6f) : Txt, 2);
	FString Desc;
	if (const FWeaponDef* D = Weapons::Find(Key))
	{
		Desc = D->Desc;
		// Power / Speed / Range bars
		const float BX = X + Pad, BW = 120.f * S, BY = Y + CH - 24.f * S;
		const TCHAR* Labels[3] = { TEXT("PWR"), TEXT("SPD"), TEXT("RNG") };
		const float Vals[3] = { D->BarPower, D->BarSpeed, D->BarRange };
		for (int32 i = 0; i < 3; ++i)
		{
			const float BXi = BX + i * (BW + 24.f * S);
			Text(Labels[i], BXi, BY - 4.f * S, PX_SMALL - 4.f, FLinearColor(1, 1, 1, 0.5f));
			DrawBar(BXi + 42.f * S, BY, BW - 42.f * S, 12.f * S, Vals[i], bCan ? FLinearColor(0.9f, 0.75f, 0.3f) : FLinearColor(0.4f, 0.4f, 0.4f), FLinearColor(0, 0, 0, 0.6f));
		}
	}
	else if (const FGrenadeDef* G = Weapons::FindGrenade(Key)) Desc = G->Desc;
	else if (const FArmorDef* A = Weapons::FindArmor(Key)) Desc = A->Desc;
	// wrap the description on two lines
	FString L1 = Desc, L2;
	const int32 MaxChars = 46;
	if (Desc.Len() > MaxChars)
	{
		int32 Cut = MaxChars;
		while (Cut > 20 && Desc[Cut] != TEXT(' ')) --Cut;
		L1 = Desc.Left(Cut); L2 = Desc.Mid(Cut + 1);
	}
	Text(L1, X + Pad, Y + 42.f * S, PX_SMALL - 2.f, bCan ? FLinearColor(1, 1, 1, 0.75f) : Txt);
	if (!L2.IsEmpty()) Text(L2, X + Pad, Y + 64.f * S, PX_SMALL - 2.f, bCan ? FLinearColor(1, 1, 1, 0.75f) : Txt);
	if (!Extra.IsEmpty()) Text(Extra, X + CW - Pad, Y + CH - 30.f * S, PX_BODY, FLinearColor(0.6f, 1.f, 0.6f), 2);
}

void ABombHUD::DrawShop(ABombGameMode* GM, ABombGameState* GS, AFPSCharacter* Me, float W, float H)
{
	ABombPlayerState* PS = Me->GetPlayerState<ABombPlayerState>();
	APlayerController* PC = GetOwningPlayerController();
	// hover
	HoverBox = NAME_None;
	float MX = 0.f, MY = 0.f;
	if (PC && PC->GetMousePosition(MX, MY))
	{
		// the hit boxes of the last frame are still valid for the hover test
		if (const FHUDHitBox* Box = GetHitBoxAtCoordinates(FVector2D(MX, MY), false)) HoverBox = Box->GetName();
	}
	DrawRect(FLinearColor(0.02f, 0.02f, 0.03f, 0.88f), 0, 0, W, H);
	const float Top = 28.f * S;
	Text(TEXT("BUY MENU"), 40.f * S, Top, PX_BIG, FLinearColor::White);
	if (PS)
	{
		Text(Money(PS->Money), 320.f * S, Top + 4.f * S, PX_BIG, FLinearColor(0.6f, 1.f, 0.6f));
		Text(FString::Printf(TEXT("next round, at least %s"), *Money(GM->MinNextMoney(Me))), 320.f * S, Top + 58.f * S, PX_SMALL, FLinearColor(1, 1, 1, 0.6f));
	}
	Text(FString::Printf(TEXT("Round starts in %d s"), FMath::Max(0, FMath::CeilToInt(GS->PhaseTimeRemaining))), W - 40.f * S, Top, PX_MED, FLinearColor(1.f, 0.85f, 0.4f), 2);
	Text(TEXT("left click: buy   right click: sell back   B: close"), W - 40.f * S, Top + 46.f * S, PX_SMALL, FLinearColor(1, 1, 1, 0.6f), 2);

	const float CW = 420.f * S, CH = 112.f * S, Gap = 12.f * S;
	const float ColGap = 30.f * S;
	const float X0 = 40.f * S;
	const float Y0 = 130.f * S;
	auto Column = [&](int32 Col, const FString& Title, const TArray<FName>& Keys, float& Y)
	{
		const float X = X0 + Col * (CW + ColGap);
		Text(Title, X, Y, PX_MED, FLinearColor(1.f, 0.85f, 0.4f));
		Y += 44.f * S;
		for (const FName& K : Keys)
		{
			bool bOwned = false;
			FString Extra;
			if (const FWeaponDef* D = Weapons::Find(K))
			{
				const int32 Idx = (int32)D->Slot;
				bOwned = Me->Guns[Idx].IsValid() && Me->Guns[Idx].Def == D;
			}
			else if (const FGrenadeDef* G = Weapons::FindGrenade(K))
			{
				const int32 N = Me->NadeCount(K);
				bOwned = N >= G->Max;
				FString Pips; for (int32 i = 0; i < G->Max; ++i) Pips += i < N ? TEXT("o") : TEXT("-");
				Extra = Pips;
			}
			else if (const FArmorDef* A = Weapons::FindArmor(K))
			{
				bOwned = Me->Armor >= A->Amount;
			}
			DrawCard(GM, Me, K, X, Y, CW, CH, bOwned, Extra);
			Y += CH + Gap;
		}
	};
	float Y = Y0;
	Column(0, TEXT("SIDEARMS  (2)"), Weapons::SideOrder(), Y);
	Y = Y0;
	Column(1, TEXT("MAIN GUNS  (1)"), Weapons::MainOrder(), Y);
	Y = Y0;
	Column(2, TEXT("SNIPERS  (1)"), Weapons::SniperOrder(), Y);
	Y += 20.f * S;
	TArray<FName> ArmorKeys; for (const FArmorDef& A : Weapons::Armors()) ArmorKeys.Add(A.Key);
	Column(2, TEXT("KEVLAR"), ArmorKeys, Y);
	Y = Y0;
	Column(3, TEXT("GRENADES  (Q E C / 5)"), Weapons::NadeOrder(), Y);
}

void ABombHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);
	const FString Name = BoxName.ToString();
	if (!Name.StartsWith(TEXT("buy:"))) return;
	const FName Key(*Name.Mid(4));
	APlayerController* PC = GetOwningPlayerController();
	ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
	AFPSCharacter* Me = PC ? Cast<AFPSCharacter>(PC->GetPawn()) : nullptr;
	if (!GM || !Me) return;
	if (PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		GM->Sell(Me, Key);
	}
	else if (!GM->Buy(Me, Key))
	{
		// already owned: just bring it up
		if (const FWeaponDef* D = Weapons::Find(Key))
		{
			const int32 Idx = (int32)D->Slot;
			if (Me->Guns[Idx].IsValid() && Me->Guns[Idx].Def == D) Me->SwitchTo((EHeldSlot)Idx);
		}
	}
}
