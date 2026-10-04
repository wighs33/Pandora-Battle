#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "EscapeMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEscapeMenuClosedSignature, UEscapeMenuWidget*, EscapeMenuWidget);

/** 게임 중 ESC로 여는 메뉴. 게임으로 돌아가기와 타이틀로 나가기만 맡고, 설정은 게임 설정 화면이 맡는다. */
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEscapeMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UEscapeMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "!Menu")
	void ExitToTitleMap();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleExitClicked();

	void HandleDestroySessionForExit(bool bWasSuccessful);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SetRequestedPause(bool bPaused);
	void TravelToTitleMap();
	void ClearDestroySessionDelegate();

public:
	UPROPERTY(BlueprintAssignable, Category = "!Menu")
	FEscapeMenuClosedSignature OnMenuClosed;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Resume;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!Menu|Bind")
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Exit")
	bool bDestroySessionOnExit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Pause")
	bool bPauseGameWhenOpened = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Menu|Pause", meta = (EditCondition = "bPauseGameWhenOpened"))
	bool bAllowNetworkPause = false;

private:
	FDelegateHandle DestroySessionCompleteHandle;
	bool bAppliedPause = false;
};
