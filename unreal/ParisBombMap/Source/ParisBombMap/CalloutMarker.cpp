#include "CalloutMarker.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

ACalloutMarker::ACalloutMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(Root);
	Text->SetWorldSize(70.f);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetTextRenderColor(FColor(255, 235, 180));
	Text->SetCanEverAffectNavigation(false);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACalloutMarker::SetCallout(const FString& Name)
{
	CalloutName = Name;
	Text->SetText(FText::FromString(Name));
}

void ACalloutMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < NextFaceUpdate) return;
	NextFaceUpdate = Now + 0.1f;
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector To = Cam->GetCameraLocation() - GetActorLocation();
		if (To.SizeSquared2D() > 1.f)
		{
			// text render faces its +X axis: point it at the camera
			Text->SetWorldRotation(FRotator(0.f, To.Rotation().Yaw, 0.f));
		}
	}
}
