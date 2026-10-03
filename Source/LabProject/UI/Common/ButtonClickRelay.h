#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ButtonClickRelay.generated.h"

class UButton;

/**
 * UButton::OnClicked에는 인자가 없어 처리기 하나로 여러 버튼을 구분할 수 없다.
 * 버튼마다 하나씩 붙여, 눌리면 미리 정해 둔 델리게이트(버튼 번호 같은 값을 담은)를 부른다.
 */
UCLASS()
class LABPROJECT_API UButtonClickRelay : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Bind(UButton* InButton, FSimpleDelegate InOnClicked);
	void Unbind();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UButton> Button;
	FSimpleDelegate OnClicked;
};
