#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CinematicScreen.generated.h"

/**
 * 연출 시퀀스가 재생되는 동안 겹침 층에 올리는 화면. 위아래 검은 띠를 그리고 게임 입력과 커서를 막는다.
 * 여럿이 함께 보는 연출이라 뒤로 가기(ESC)로 건너뛰지 않는다.
 */
UCLASS()
class LABPROJECT_API UCinematicScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UCinematicScreen(const FObjectInitializer& ObjectInitializer);

	// 층에 올리는 일은 UUiSubsystem::PushScreen이 한다.
	static UCinematicScreen* Create(APlayerController* OwningPlayer);
};
