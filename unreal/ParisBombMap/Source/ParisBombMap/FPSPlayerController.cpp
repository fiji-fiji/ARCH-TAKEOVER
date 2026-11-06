#include "FPSPlayerController.h"
#include "FPSCharacter.h"
#include "BombGameMode.h"
#include "BombGameState.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AFPSPlayerController::AFPSPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	ClickEventKeys.AddUnique(EKeys::RightMouseButton);   // right click on a shop card sells it back
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
}

void AFPSPlayerController::ToggleShop()
{
	if (bShopOpen) CloseShop(); else OpenShop();
}

void AFPSPlayerController::OpenShop()
{
	if (bShopOpen) return;
	ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
	AFPSCharacter* C = Cast<AFPSCharacter>(GetPawn());
	if (!GM || !C || !GM->CanBuy(C)) return;
	bShopOpen = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
	C->StopFire();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	// park the cursor in the middle of the window
	int32 W = 0, H = 0;
	GetViewportSize(W, H);
	SetMouseLocation(W / 2, H / 2);
}

void AFPSPlayerController::CloseShop()
{
	if (!bShopOpen) return;
	bShopOpen = false;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	ResetIgnoreLookInput();
	ResetIgnoreMoveInput();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
}

void AFPSPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bShopOpen)
	{
		ABombGameMode* GM = Cast<ABombGameMode>(UGameplayStatics::GetGameMode(this));
		AFPSCharacter* C = Cast<AFPSCharacter>(GetPawn());
		if (!GM || !C || !GM->CanBuy(C)) CloseShop();     // round went live or you died
	}
}
