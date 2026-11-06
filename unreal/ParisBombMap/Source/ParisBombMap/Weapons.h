#pragma once

#include "CoreMinimal.h"

/**
 * The weapon roster, the rules of which come 1:1 from FPS Reborn (js/weapons.js, grenades.js): slots, prices,
 * fire behaviour, damage per body part, distance fall-off, spread, recoil, silencer, scope. Only the names and
 * the looks are new (realistic firearms). Distances are in metres, angles in radians, times in seconds.
 */
enum class EWeaponSlot : uint8 { Main, Side, Knife };

struct FWeaponDef
{
	FName Key;
	FString Name;
	EWeaponSlot Slot = EWeaponSlot::Main;
	int32 Price = 0;
	FString Desc;
	float BarPower = 0.5f, BarSpeed = 0.5f, BarRange = 0.5f;   // shop bars 0..1
	bool bAuto = false;
	float FireRate = 10.f;        // shots per second
	int32 MagSize = 30;
	int32 Reserve = 90;
	float ReloadTime = 2.f;
	float Range = 150.f;          // metres
	float DmgHead = 100.f, DmgBody = 30.f, DmgLegs = 25.f;
	float FallStart = 0.f, FallEnd = 0.f, FallMin = 1.f;      // FallEnd <= 0: no fall-off
	// spread (radians): base + movement + spray
	float SpBase = 0.002f, SpWalk = 0.01f, SpRun = 0.05f, SpAir = 0.2f, SpCrouchMul = 0.8f, SpPerShot = 0.004f, SpMaxSpray = 0.035f;
	// recoil: kicks per shot (radians), then sustained; side wobble after the 4th shot; max climb; recovery rate
	TArray<float> Kicks;
	float Sustained = 0.008f, Side = 0.006f, MaxUp = 0.13f, Recover = 9.f;
	bool bSilenced = false;
	float ScopeFov = 0.f;         // 0 = no scope
	float ScopeUnscoped = 0.f;    // extra spread when firing a scoped weapon unscoped
	int32 Pellets = 1;
	float PelletSpread = 0.f;
	bool bMelee = false;
	float Reach = 0.f;            // melee reach, metres
	FString Mesh;                 // /Game/Meshes/<Mesh>
	FString ShotSound;            // /Game/Sounds/<ShotSound>

	bool HasScope() const { return ScopeFov > 0.f; }
};

struct FGrenadeDef
{
	FName Key;
	FString Name;
	int32 Price = 0;
	int32 Max = 1;
	float Fuse = 1.f;
	FString Desc;
	FString Mesh;
};

struct FArmorDef
{
	FName Key;
	FString Name;
	int32 Price = 0;
	int32 Amount = 0;
	FString Desc;
};

/** Ammo, cooldown, reload, spray and recoil state of one gun (one per slot on a character). */
struct FGunState
{
	const FWeaponDef* Def = nullptr;
	int32 Ammo = 0;
	int32 Reserve = 0;
	float Cooldown = 0.f;
	float Reloading = 0.f;        // seconds left, 0 = not reloading
	int32 SprayIndex = 0;
	float SinceShot = 10.f;
	float RecoilPitch = 0.f;      // radians, decays back to 0
	float RecoilYaw = 0.f;

	bool IsValid() const { return Def != nullptr; }
	void Set(const FWeaponDef* InDef) { Def = InDef; Reset(); }
	void Clear() { Def = nullptr; Ammo = Reserve = 0; }
	void Reset();
	bool Busy() const { return Reloading > 0.f; }
	bool Ready() const { return Cooldown <= 0.f && !Busy(); }
	bool StartReload();
	/** Per-frame: cooldown, reload completion, recoil recovery. */
	void Tick(float Dt);
	/** Inaccuracy in radians for the shooter's current motion. Speed in m/s. */
	float ComputeSpread(float HorizontalSpeedMps, bool bOnGround, bool bCrouched, bool bScoped) const;
	/** Damage multiplier at a distance (metres). */
	float Falloff(float DistM) const;
	/** Consume one round, apply recoil, return the spread to use (and pellet count via Def). */
	float Fire(bool bScoped, float HorizontalSpeedMps, bool bOnGround, bool bCrouched, FRandomStream& Rng);
};

namespace Weapons
{
	const TArray<FWeaponDef>& All();
	const FWeaponDef* Find(FName Key);
	const TArray<FGrenadeDef>& Grenades();
	const FGrenadeDef* FindGrenade(FName Key);
	const TArray<FArmorDef>& Armors();
	const FArmorDef* FindArmor(FName Key);
	/** Shop columns, in display order. */
	const TArray<FName>& SideOrder();
	const TArray<FName>& MainOrder();
	const TArray<FName>& SniperOrder();
	const TArray<FName>& NadeOrder();
	int32 PriceOf(FName Key);
	FString NameOf(FName Key);

	// movement (FPS Reborn PLAYER): m/s
	constexpr float RunSpeed = 6.75f;
	constexpr float WalkSpeed = 3.8f;
	constexpr float CrouchSpeed = 2.9f;
	constexpr float KnifeSpeed = 1.5f;       // multiplier while the knife is out

	// economy (FPS Reborn ECON)
	constexpr int32 StartMoney = 800;
	constexpr int32 WinMoney = 3000;
	constexpr int32 LossMoney[3] = { 1900, 2400, 2900 };
	constexpr int32 KillMoney = 200;
	constexpr int32 PlantMoney = 300;
	constexpr int32 MaxMoney = 9000;

	// grenades
	constexpr float NadeGravity = 18.f;     // m/s^2
	constexpr float SmokeRadius = 3.6f, SmokeLife = 15.f;
	constexpr float FragRadius = 6.f, FragFull = 1.5f, FragMax = 130.f;
	constexpr float FlashRange = 35.f;

	// drops
	constexpr float DropSwapReach = 2.2f;
	constexpr float DropBotRange = 14.f;
}
