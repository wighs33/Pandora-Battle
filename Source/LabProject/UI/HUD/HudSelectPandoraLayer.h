#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HudSelectPandoraLayer.generated.h"

class APdHUD;
class APlayerController;
class USelectPandoraWidget;
class UUiScreen;
class UWidgetClassDefinition;

/**
 * 판도라 선택 원형 메뉴를 만들고 연다·닫는다.
 *
 * 열려 있는 동안 화면 가운데에서 마우스가 놓인 방향을 고르고, 확정하며 닫으면 그 방향을 위젯에 넘긴다.
 * HUD 틱 켜기·끄기와 게임플레이 HUD 숨김은 HUD가 맡는다.
 */
UCLASS()
class LABPROJECT_API UHudSelectPandoraLayer : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(APdHUD* InOwnerHud);
	void EnsureWidget(APlayerController& Controller, const UWidgetClassDefinition& Definition);
	void BindPresenter();
	void ReleaseWidget();

	/** 위젯이 있고 아직 닫혀 있으면 막는 화면으로 올리고 마우스를 화면 가운데로 옮긴다. 열었으면 true. */
	bool Open();

	/** bCommitSelection이면 고른 방향을 위젯에 넘긴다. 그 선택이 장착 판도라를 바꾸면 true. */
	bool Close(bool bCommitSelection);

	bool IsOpen() const;
	USelectPandoraWidget* GetWidget() const { return Widget; }

	// Event Handlers --------------------------------------------------------------------------------------------------
	void UpdateDirectionFromMouse();

private:
	TWeakObjectPtr<APdHUD> OwnerHud;

	UPROPERTY(Transient)
	TObjectPtr<USelectPandoraWidget> Widget;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> Screen;

	/** 마우스가 가리키는 방향 칸. 가운데 무시 범위 안이면 -1이다. */
	int32 DirectionIndex = -1;
};
