#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPSPlayerController.generated.h"

/** The human's controller: mouse look in game, mouse cursor while the buy menu is open. */
UCLASS()
class PARISBOMBMAP_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFPSPlayerController();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	bool bShopOpen = false;
	void ToggleShop();
	void OpenShop();
	void CloseShop();
};
