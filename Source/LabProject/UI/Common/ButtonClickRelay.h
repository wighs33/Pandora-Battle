#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ButtonClickRelay.generated.h"

class UButton;

/**
 * UButton::OnClicked·OnHovered에는 인자가 없어 처리기 하나로 여러 버튼을 구분할 수 없다.
 * 버튼마다 하나씩 붙여, 눌리면(그리고 마우스가 오르내리면) 미리 정해 둔 델리게이트(버튼 번호 같은 값을 담은)를 부른다.
 */
UCLASS()
class LABPROJECT_API UButtonClickRelay : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	DECLARE_DELEGATE_OneParam(FOnHoverChanged, bool /*bHovered*/);

	void Bind(UButton* InButton, FSimpleDelegate InOnClicked, FOnHoverChanged InOnHoverChanged = FOnHoverChanged());
	void Unbind();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

private:
	TWeakObjectPtr<UButton> Button;
	FSimpleDelegate OnClicked;
	FOnHoverChanged OnHoverChanged;
};
