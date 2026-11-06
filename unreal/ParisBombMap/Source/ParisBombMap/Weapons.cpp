#include "Weapons.h"

namespace
{
	FWeaponDef Gun(const TCHAR* Key, const TCHAR* Name, EWeaponSlot Slot, int32 Price, const TCHAR* Desc, const TCHAR* Mesh, const TCHAR* Sound)
	{
		FWeaponDef D;
		D.Key = Key; D.Name = Name; D.Slot = Slot; D.Price = Price; D.Desc = Desc; D.Mesh = Mesh; D.ShotSound = Sound;
		return D;
	}

	TArray<FWeaponDef> BuildRoster()
	{
		TArray<FWeaponDef> R;
		{
			// Dandelion -> P9: free sidearm, one click = one shot
			FWeaponDef D = Gun(TEXT("p9"), TEXT("P9"), EWeaponSlot::Side, 0, TEXT("Standard-issue pistol. One click, one shot."), TEXT("gun_p9"), TEXT("S_Shot_Pistol"));
			D.BarPower = 0.3f; D.BarSpeed = 0.5f; D.BarRange = 0.45f;
			D.bAuto = false; D.FireRate = 6.5f; D.MagSize = 12; D.Reserve = 48; D.ReloadTime = 1.5f; D.Range = 150.f;
			D.DmgHead = 78; D.DmgBody = 26; D.DmgLegs = 22;
			D.FallStart = 30; D.FallEnd = 50; D.FallMin = 0.8f;
			D.SpBase = 0.004f; D.SpWalk = 0.006f; D.SpRun = 0.025f; D.SpAir = 0.15f; D.SpCrouchMul = 0.85f; D.SpPerShot = 0.008f; D.SpMaxSpray = 0.04f;
			D.Kicks = { 0.f, 0.012f, 0.014f }; D.Sustained = 0.014f; D.Side = 0.004f; D.MaxUp = 0.07f; D.Recover = 12.f;
			R.Add(D);
		}
		{
			// Pansy Pump -> SO-12 sawn-off: cheap, only up close
			FWeaponDef D = Gun(TEXT("so12"), TEXT("SO-12 Sawn-off"), EWeaponSlot::Side, 250, TEXT("A cheap sawn-off. Deadly right up close, useless any further."), TEXT("gun_so12"), TEXT("S_Shot_Shotgun"));
			D.BarPower = 0.6f; D.BarSpeed = 0.3f; D.BarRange = 0.05f;
			D.bAuto = false; D.FireRate = 1.4f; D.MagSize = 4; D.Reserve = 20; D.ReloadTime = 2.f; D.Range = 40.f;
			D.Pellets = 7; D.PelletSpread = 0.09f;
			D.DmgHead = 18; D.DmgBody = 10; D.DmgLegs = 8;
			D.FallStart = 5; D.FallEnd = 12; D.FallMin = 0.2f;
			D.SpBase = 0.f; D.SpWalk = 0.005f; D.SpRun = 0.01f; D.SpAir = 0.05f; D.SpCrouchMul = 1.f; D.SpPerShot = 0.f; D.SpMaxSpray = 0.f;
			D.Kicks = { 0.025f }; D.Sustained = 0.025f; D.Side = 0.f; D.MaxUp = 0.05f; D.Recover = 7.f;
			R.Add(D);
		}
		{
			// Buzzy-11 -> MP-11 machine pistol
			FWeaponDef D = Gun(TEXT("mp11"), TEXT("MP-11"), EWeaponSlot::Side, 400, TEXT("A buzzing machine pistol. Great up close, falls apart far away."), TEXT("gun_mp11"), TEXT("S_Shot_SMG"));
			D.BarPower = 0.25f; D.BarSpeed = 1.f; D.BarRange = 0.15f;
			D.bAuto = true; D.FireRate = 14.f; D.MagSize = 20; D.Reserve = 60; D.ReloadTime = 1.9f; D.Range = 120.f;
			D.DmgHead = 52; D.DmgBody = 18; D.DmgLegs = 15;
			D.FallStart = 10; D.FallEnd = 25; D.FallMin = 0.6f;
			D.SpBase = 0.012f; D.SpWalk = 0.006f; D.SpRun = 0.02f; D.SpAir = 0.12f; D.SpCrouchMul = 0.9f; D.SpPerShot = 0.003f; D.SpMaxSpray = 0.045f;
			D.Kicks = { 0.f, 0.004f, 0.005f, 0.006f }; D.Sustained = 0.005f; D.Side = 0.008f; D.MaxUp = 0.06f; D.Recover = 10.f;
			R.Add(D);
		}
		{
			// Hush Daisy -> SP-7 suppressed pistol
			FWeaponDef D = Gun(TEXT("sp7"), TEXT("SP-7 Suppressed"), EWeaponSlot::Side, 500, TEXT("A quiet P9 upgrade: harder hits, steadier aim, and enemies barely hear it."), TEXT("gun_sp7"), TEXT("S_Shot_Silenced"));
			D.BarPower = 0.4f; D.BarSpeed = 0.5f; D.BarRange = 0.55f;
			D.bSilenced = true;
			D.bAuto = false; D.FireRate = 6.f; D.MagSize = 13; D.Reserve = 52; D.ReloadTime = 1.6f; D.Range = 150.f;
			D.DmgHead = 104; D.DmgBody = 30; D.DmgLegs = 25;
			D.FallStart = 35; D.FallEnd = 60; D.FallMin = 0.85f;
			D.SpBase = 0.003f; D.SpWalk = 0.005f; D.SpRun = 0.022f; D.SpAir = 0.15f; D.SpCrouchMul = 0.85f; D.SpPerShot = 0.006f; D.SpMaxSpray = 0.035f;
			D.Kicks = { 0.f, 0.01f, 0.012f }; D.Sustained = 0.012f; D.Side = 0.003f; D.MaxUp = 0.06f; D.Recover = 13.f;
			R.Add(D);
		}
		{
			// Cactus Revolver -> R-44
			FWeaponDef D = Gun(TEXT("r44"), TEXT("R-44 Revolver"), EWeaponSlot::Side, 800, TEXT("Six heavy rounds. One head hit kills. Stand still to aim it."), TEXT("gun_r44"), TEXT("S_Shot_Revolver"));
			D.BarPower = 0.9f; D.BarSpeed = 0.2f; D.BarRange = 0.6f;
			D.bAuto = false; D.FireRate = 2.2f; D.MagSize = 6; D.Reserve = 30; D.ReloadTime = 2.3f; D.Range = 160.f;
			D.DmgHead = 180; D.DmgBody = 55; D.DmgLegs = 45;
			D.FallStart = 30; D.FallEnd = 55; D.FallMin = 0.8f;
			D.SpBase = 0.003f; D.SpWalk = 0.03f; D.SpRun = 0.07f; D.SpAir = 0.2f; D.SpCrouchMul = 0.8f; D.SpPerShot = 0.02f; D.SpMaxSpray = 0.05f;
			D.Kicks = { 0.05f }; D.Sustained = 0.05f; D.Side = 0.01f; D.MaxUp = 0.08f; D.Recover = 7.f;
			R.Add(D);
		}
		{
			// Tulip Popper -> SMG-9
			FWeaponDef D = Gun(TEXT("smg9"), TEXT("SMG-9"), EWeaponSlot::Main, 900, TEXT("Sprays super fast and stays accurate while running. Weaker far away."), TEXT("gun_smg9"), TEXT("S_Shot_SMG"));
			D.BarPower = 0.35f; D.BarSpeed = 0.95f; D.BarRange = 0.3f;
			D.bAuto = true; D.FireRate = 13.f; D.MagSize = 30; D.Reserve = 90; D.ReloadTime = 1.6f; D.Range = 150.f;
			D.DmgHead = 66; D.DmgBody = 26; D.DmgLegs = 22;
			D.FallStart = 15; D.FallEnd = 30; D.FallMin = 0.75f;
			D.SpBase = 0.008f; D.SpWalk = 0.004f; D.SpRun = 0.012f; D.SpAir = 0.12f; D.SpCrouchMul = 0.9f; D.SpPerShot = 0.002f; D.SpMaxSpray = 0.03f;
			D.Kicks = { 0.f, 0.004f, 0.006f, 0.007f }; D.Sustained = 0.005f; D.Side = 0.006f; D.MaxUp = 0.07f; D.Recover = 10.f;
			R.Add(D);
		}
		{
			// Sunflower Splash -> PS-12 pump shotgun
			FWeaponDef D = Gun(TEXT("ps12"), TEXT("PS-12 Pump"), EWeaponSlot::Main, 1800, TEXT("A huge spread of buckshot up close. Useless far away."), TEXT("gun_ps12"), TEXT("S_Shot_Shotgun"));
			D.BarPower = 1.f; D.BarSpeed = 0.2f; D.BarRange = 0.1f;
			D.bAuto = false; D.FireRate = 1.1f; D.MagSize = 5; D.Reserve = 15; D.ReloadTime = 2.2f; D.Range = 60.f;
			D.Pellets = 12; D.PelletSpread = 0.075f;
			D.DmgHead = 24; D.DmgBody = 12; D.DmgLegs = 10;
			D.FallStart = 7; D.FallEnd = 18; D.FallMin = 0.25f;
			D.SpBase = 0.f; D.SpWalk = 0.005f; D.SpRun = 0.01f; D.SpAir = 0.05f; D.SpCrouchMul = 1.f; D.SpPerShot = 0.f; D.SpMaxSpray = 0.f;
			D.Kicks = { 0.03f }; D.Sustained = 0.03f; D.Side = 0.f; D.MaxUp = 0.06f; D.Recover = 6.f;
			R.Add(D);
		}
		{
			// Bluebell Scout -> SR-30 scout
			FWeaponDef D = Gun(TEXT("sr30"), TEXT("SR-30 Scout"), EWeaponSlot::Main, 950, TEXT("A light sniper rifle. One head hit kills, a body hit doesn't. Right-click to zoom."), TEXT("gun_sr30"), TEXT("S_Shot_Sniper"));
			D.BarPower = 0.7f; D.BarSpeed = 0.3f; D.BarRange = 1.f;
			D.bAuto = false; D.FireRate = 1.1f; D.MagSize = 8; D.Reserve = 24; D.ReloadTime = 2.2f; D.Range = 250.f;
			D.ScopeFov = 32.f; D.ScopeUnscoped = 0.04f;
			D.DmgHead = 220; D.DmgBody = 80; D.DmgLegs = 65;
			D.SpBase = 0.0008f; D.SpWalk = 0.02f; D.SpRun = 0.05f; D.SpAir = 0.15f; D.SpCrouchMul = 0.8f; D.SpPerShot = 0.f; D.SpMaxSpray = 0.f;
			D.Kicks = { 0.035f }; D.Sustained = 0.035f; D.Side = 0.f; D.MaxUp = 0.05f; D.Recover = 7.f;
			R.Add(D);
		}
		{
			// Bloom Blaster -> AR-7 assault rifle
			FWeaponDef D = Gun(TEXT("ar7"), TEXT("AR-7"), EWeaponSlot::Main, 2900, TEXT("Strong at any distance. The all-rounder. One head hit kills."), TEXT("gun_ar7"), TEXT("S_RifleShot"));
			D.BarPower = 0.65f; D.BarSpeed = 0.7f; D.BarRange = 0.85f;
			D.bAuto = true; D.FireRate = 9.75f; D.MagSize = 25; D.Reserve = 75; D.ReloadTime = 1.8f; D.Range = 200.f;
			D.DmgHead = 156; D.DmgBody = 39; D.DmgLegs = 33;
			D.SpBase = 0.002f; D.SpWalk = 0.012f; D.SpRun = 0.06f; D.SpAir = 0.22f; D.SpCrouchMul = 0.8f; D.SpPerShot = 0.004f; D.SpMaxSpray = 0.035f;
			D.Kicks = { 0.f, 0.006f, 0.010f, 0.013f, 0.015f, 0.016f, 0.016f, 0.014f }; D.Sustained = 0.008f; D.Side = 0.006f; D.MaxUp = 0.13f; D.Recover = 9.f;
			R.Add(D);
		}
		{
			// Hush Bloom -> AR-7S suppressed rifle
			FWeaponDef D = Gun(TEXT("ar7s"), TEXT("AR-7S Suppressed"), EWeaponSlot::Main, 2900, TEXT("Quieter and faster than the AR-7, less kick. But one head hit doesn't kill."), TEXT("gun_ar7s"), TEXT("S_Shot_Silenced"));
			D.BarPower = 0.55f; D.BarSpeed = 0.8f; D.BarRange = 0.8f;
			D.bSilenced = true;
			D.bAuto = true; D.FireRate = 11.5f; D.MagSize = 20; D.Reserve = 60; D.ReloadTime = 1.7f; D.Range = 200.f;
			D.DmgHead = 92; D.DmgBody = 33; D.DmgLegs = 28;
			D.SpBase = 0.002f; D.SpWalk = 0.011f; D.SpRun = 0.055f; D.SpAir = 0.22f; D.SpCrouchMul = 0.8f; D.SpPerShot = 0.003f; D.SpMaxSpray = 0.028f;
			D.Kicks = { 0.f, 0.005f, 0.008f, 0.01f, 0.011f, 0.012f, 0.012f, 0.011f }; D.Sustained = 0.006f; D.Side = 0.005f; D.MaxUp = 0.1f; D.Recover = 10.f;
			R.Add(D);
		}
		{
			// Lily Longshot -> LR-50 long rifle
			FWeaponDef D = Gun(TEXT("lr50"), TEXT("LR-50"), EWeaponSlot::Main, 4500, TEXT("One body hit kills. Right-click to zoom in. Slow to shoot."), TEXT("gun_lr50"), TEXT("S_Shot_Sniper"));
			D.BarPower = 1.f; D.BarSpeed = 0.1f; D.BarRange = 1.f;
			D.bAuto = false; D.FireRate = 0.75f; D.MagSize = 5; D.Reserve = 15; D.ReloadTime = 2.6f; D.Range = 250.f;
			D.ScopeFov = 28.f; D.ScopeUnscoped = 0.07f;
			D.DmgHead = 255; D.DmgBody = 150; D.DmgLegs = 120;
			D.SpBase = 0.0008f; D.SpWalk = 0.03f; D.SpRun = 0.09f; D.SpAir = 0.2f; D.SpCrouchMul = 0.8f; D.SpPerShot = 0.f; D.SpMaxSpray = 0.f;
			D.Kicks = { 0.05f }; D.Sustained = 0.05f; D.Side = 0.f; D.MaxUp = 0.06f; D.Recover = 5.f;
			R.Add(D);
		}
		{
			// Frosting Knife -> combat knife: everyone carries one (key 3); one hit, only up close
			FWeaponDef D = Gun(TEXT("knife"), TEXT("Knife"), EWeaponSlot::Knife, 0, TEXT("One stab kills, but only right up close. You run faster holding it."), TEXT("gun_knife"), TEXT("S_Knife"));
			D.bMelee = true; D.Reach = 2.2f; D.bAuto = false; D.FireRate = 2.f; D.MagSize = 1; D.Reserve = 0; D.ReloadTime = 0.f; D.Range = 3.f;
			D.DmgHead = 999; D.DmgBody = 999; D.DmgLegs = 999;
			D.SpBase = D.SpWalk = D.SpRun = D.SpAir = D.SpPerShot = D.SpMaxSpray = 0.f; D.SpCrouchMul = 1.f;
			D.Kicks = { 0.f }; D.Sustained = 0.f; D.Side = 0.f; D.MaxUp = 0.f; D.Recover = 10.f;
			R.Add(D);
		}
		return R;
	}
}

void FGunState::Reset()
{
	if (!Def) return;
	Ammo = Def->MagSize;
	Reserve = Def->Reserve;
	Cooldown = 0.f;
	Reloading = 0.f;
	SprayIndex = 0;
	SinceShot = 10.f;
	RecoilPitch = RecoilYaw = 0.f;
}

bool FGunState::StartReload()
{
	if (!Def || Def->bMelee || Reloading > 0.f || Ammo == Def->MagSize || Reserve == 0) return false;
	Reloading = Def->ReloadTime;
	return true;
}

void FGunState::Tick(float Dt)
{
	if (!Def) return;
	Cooldown -= Dt;
	SinceShot += Dt;
	if (Reloading > 0.f)
	{
		Reloading -= Dt;
		if (Reloading <= 0.f)
		{
			Reloading = 0.f;
			const int32 Take = FMath::Min(Def->MagSize - Ammo, Reserve);
			Ammo += Take;
			Reserve -= Take;
		}
	}
	// recoil settles once you stop shooting
	if (SinceShot > 1.f / Def->FireRate + 0.05f)
	{
		const float K = FMath::Exp(-Def->Recover * Dt);
		RecoilPitch *= K;
		RecoilYaw *= K;
		if (SinceShot > 0.35f) SprayIndex = 0;
	}
}

float FGunState::ComputeSpread(float V, bool bOnGround, bool bCrouched, bool bScoped) const
{
	if (!Def) return 0.f;
	const FWeaponDef& D = *Def;
	float Move = 0.f;
	if (!bOnGround) Move = D.SpAir;
	else if (V > 0.5f)
	{
		Move = V <= Weapons::WalkSpeed + 0.2f
			? D.SpWalk * (V / Weapons::WalkSpeed)
			: D.SpWalk + (D.SpRun - D.SpWalk) * FMath::Min(1.f, (V - Weapons::WalkSpeed) / (Weapons::RunSpeed - Weapons::WalkSpeed));
	}
	const float Spray = FMath::Min(D.SpMaxSpray, FMath::Max(0, SprayIndex - 1) * D.SpPerShot);
	float Total = D.SpBase + Move + Spray;
	if (bCrouched && bOnGround) Total *= D.SpCrouchMul;
	if (D.HasScope() && !bScoped) Total += D.ScopeUnscoped;
	return Total;
}

float FGunState::Falloff(float DistM) const
{
	if (!Def || Def->FallEnd <= 0.f || DistM <= Def->FallStart) return 1.f;
	if (DistM >= Def->FallEnd) return Def->FallMin;
	return 1.f - (1.f - Def->FallMin) * (DistM - Def->FallStart) / (Def->FallEnd - Def->FallStart);
}

float FGunState::Fire(bool bScoped, float V, bool bOnGround, bool bCrouched, FRandomStream& Rng)
{
	const FWeaponDef& D = *Def;
	if (!D.bMelee) Ammo--;
	// keep leftover frame time while spraying so the fire rate stays exact
	Cooldown = FMath::Max(Cooldown, -0.05f) + 1.f / D.FireRate;
	SinceShot = 0.f;
	const float Spread = ComputeSpread(V, bOnGround, bCrouched, bScoped);
	// recoil climbs for the first shots, then wobbles side to side
	const int32 I = SprayIndex;
	const float UpKick = I < D.Kicks.Num() ? D.Kicks[I] : D.Sustained;
	RecoilPitch = FMath::Min(D.MaxUp, RecoilPitch + UpKick);
	if (I >= 4) RecoilYaw += (Rng.FRand() - 0.5f) * 2.f * D.Side;
	SprayIndex++;
	return Spread;
}

namespace Weapons
{
	const TArray<FWeaponDef>& All()
	{
		static TArray<FWeaponDef> Roster = BuildRoster();
		return Roster;
	}

	const FWeaponDef* Find(FName Key)
	{
		for (const FWeaponDef& D : All()) if (D.Key == Key) return &D;
		return nullptr;
	}

	const TArray<FGrenadeDef>& Grenades()
	{
		static TArray<FGrenadeDef> G = []
		{
			TArray<FGrenadeDef> L;
			L.Add({ TEXT("smoke"), TEXT("Smoke"), 200, 2, 1.3f, TEXT("Puffs into a thick cloud for 15 s. Nobody can see through it."), TEXT("nade_smoke") });
			L.Add({ TEXT("flash"), TEXT("Flashbang"), 250, 2, 0.9f, TEXT("Blinds enemies looking at it for a moment."), TEXT("nade_flash") });
			L.Add({ TEXT("frag"), TEXT("Frag"), 400, 1, 1.8f, TEXT("Bounces, then a blast kills anyone close."), TEXT("nade_frag") });
			return L;
		}();
		return G;
	}

	const FGrenadeDef* FindGrenade(FName Key)
	{
		for (const FGrenadeDef& D : Grenades()) if (D.Key == Key) return &D;
		return nullptr;
	}

	const TArray<FArmorDef>& Armors()
	{
		static TArray<FArmorDef> A = []
		{
			TArray<FArmorDef> L;
			L.Add({ TEXT("light"), TEXT("Kevlar"), 400, 25, TEXT("+25 armour on top of your health.") });
			L.Add({ TEXT("heavy"), TEXT("Heavy Kevlar"), 1000, 50, TEXT("+50 armour on top of your health.") });
			return L;
		}();
		return A;
	}

	const FArmorDef* FindArmor(FName Key)
	{
		for (const FArmorDef& D : Armors()) if (D.Key == Key) return &D;
		return nullptr;
	}

	const TArray<FName>& SideOrder() { static TArray<FName> L = { TEXT("p9"), TEXT("so12"), TEXT("mp11"), TEXT("sp7"), TEXT("r44") }; return L; }
	const TArray<FName>& MainOrder() { static TArray<FName> L = { TEXT("smg9"), TEXT("ps12"), TEXT("ar7"), TEXT("ar7s") }; return L; }
	const TArray<FName>& SniperOrder() { static TArray<FName> L = { TEXT("sr30"), TEXT("lr50") }; return L; }
	const TArray<FName>& NadeOrder() { static TArray<FName> L = { TEXT("smoke"), TEXT("flash"), TEXT("frag") }; return L; }

	int32 PriceOf(FName Key)
	{
		if (const FWeaponDef* W = Find(Key)) return W->Price;
		if (const FGrenadeDef* G = FindGrenade(Key)) return G->Price;
		if (const FArmorDef* A = FindArmor(Key)) return A->Price;
		return 0;
	}

	FString NameOf(FName Key)
	{
		if (const FWeaponDef* W = Find(Key)) return W->Name;
		if (const FGrenadeDef* G = FindGrenade(Key)) return G->Name;
		if (const FArmorDef* A = FindArmor(Key)) return A->Name;
		return Key.ToString();
	}
}
