#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HudMenuLayer.generated.h"

class APdHUD;
class UMenuPopupWidget;
class UHudUiRouter;

/** 설정 메뉴 위젯의 수명을 관리하고 닫힘 이벤트를 HUD에 알린다. */
UCLASS()
class LABPROJECT_API UHudMenuLayer : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter);
	bool Open();
	bool Toggle();
	bool Close();
	bool IsOpen() const;
	UMenuPopupWidget* GetWidget() const { return ActiveWidget; }
	void Shutdown();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleMenuClosed(UMenuPopupWidget* ClosedWidget);

private:
	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UMenuPopupWidget> ActiveWidget;
};
