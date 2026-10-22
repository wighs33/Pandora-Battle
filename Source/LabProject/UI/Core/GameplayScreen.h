#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameplayScreen.generated.h"

/**
 * 게임 중 화면 층 맨 아래에 깔리는 빈 화면. 위에 다른 화면이 없을 때 CommonUI 입력의 주인이 된다.
 * ESC(뒤로 가기)를 CommonUI 액션으로 받아 HUD에 넘기고, 입력 설정은 라우터의 기본 설정을 따른다.
 */
UCLASS()
class LABPROJECT_API UGameplayScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UGameplayScreen(const FObjectInitializer& ObjectInitializer);

	// 층에 올리는 일은 UUiSubsystem::PushScreen이 한다.
	static UGameplayScreen* Create(APlayerController* OwningPlayer, FSimpleDelegate BackAction);

private:
	FSimpleDelegate OnBack;
};
