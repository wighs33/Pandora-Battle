#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HudMenuLayer.generated.h"

class APdHUD;
class UEscapeMenuWidget;
class UHudUiRouter;

/** ESC 메뉴 위젯의 수명을 관리하고 닫힘 이벤트를 HUD에 알린다. */
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
	void Shutdown();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleMenuClosed(UEscapeMenuWidget* ClosedWidget);

private:
	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UEscapeMenuWidget> ActiveWidget;
};
