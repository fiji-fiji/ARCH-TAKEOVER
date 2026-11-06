#include "GameSounds.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	TMap<FString, TWeakObjectPtr<USoundBase>> GCache;
	TSet<FString> GMissing;
}

USoundBase* GameSounds::Get(const TCHAR* Name)
{
	const FString Key(Name);
	if (TWeakObjectPtr<USoundBase>* Found = GCache.Find(Key))
	{
		if (Found->IsValid()) return Found->Get();
	}
	if (GMissing.Contains(Key)) return nullptr;
	const FString Path = FString::Printf(TEXT("/Game/Sounds/%s.%s"), Name, Name);
	USoundBase* S = LoadObject<USoundBase>(nullptr, *Path);
	if (!S)
	{
		GMissing.Add(Key);
		UE_LOG(LogTemp, Warning, TEXT("[GameSounds] missing %s"), *Path);
		return nullptr;
	}
	S->AddToRoot();
	GCache.Add(Key, S);
	return S;
}

void GameSounds::At(const UObject* Ctx, const TCHAR* Name, const FVector& Loc, float Volume, float Pitch)
{
	if (USoundBase* S = Get(Name))
	{
		UGameplayStatics::PlaySoundAtLocation(Ctx, S, Loc, Volume, Pitch);
	}
}

void GameSounds::UI(const UObject* Ctx, const TCHAR* Name, float Volume, float Pitch)
{
	if (USoundBase* S = Get(Name))
	{
		UGameplayStatics::PlaySound2D(Ctx, S, Volume, Pitch);
	}
}
