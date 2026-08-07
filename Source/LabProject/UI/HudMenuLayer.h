#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HudMenuLayer.generated.h"

class APdHUD;
class UMenuPopupWidget;
class UHudUiRouter;

/** Owns settings-menu widget lifetime and reports close events back to the HUD facade. */
UCLASS()
class LABPROJECT_API UHudMenuLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter);
	bool Open();
	bool Toggle();
	bool Close();
	bool IsOpen() const;
	UMenuPopupWidget* GetWidget() const { return ActiveWidget; }
	void Shutdown();

private:
	UFUNCTION()
	void HandleMenuClosed(UMenuPopupWidget* ClosedWidget);

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UMenuPopupWidget> ActiveWidget;
};
