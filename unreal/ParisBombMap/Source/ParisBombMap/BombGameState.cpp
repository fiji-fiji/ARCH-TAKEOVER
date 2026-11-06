#include "BombGameState.h"
#include "Engine/World.h"

void ABombGameState::AddFeed(const FString& Text, const FLinearColor& Color)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	KillFeed.Add({Text, Now + 6.f, Color});
	while (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}
}
