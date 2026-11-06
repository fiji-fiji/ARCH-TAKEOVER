#include "CalloutMarker.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"

ACalloutMarker::ACalloutMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(Root);
	Text->SetWorldSize(55.f);
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
