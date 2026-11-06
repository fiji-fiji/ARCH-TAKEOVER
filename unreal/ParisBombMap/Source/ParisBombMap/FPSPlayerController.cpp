#include "FPSPlayerController.h"

AFPSPlayerController::AFPSPlayerController()
{
	bShowMouseCursor = false;
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
}
