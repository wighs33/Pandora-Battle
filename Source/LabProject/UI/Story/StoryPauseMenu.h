#pragma once

#include "CoreMinimal.h"
#include "UI/Core/ScreenWidget.h"
#include "StoryPauseMenu.generated.h"

class UButton;
class UButtonClickRelay;
class UTextBlock;
class UVerticalBox;

/** 이야기 도중 ESC로 여는 메뉴: 계속하기, 건너뛰기, 설정, 타이틀로. 판 없이 글자 목록만 둔다. */
UCLASS()
class LABPROJECT_API UStoryPauseMenu : public UScreenWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UStoryPauseMenu(const FObjectInitializer& ObjectInitializer);

	// 층에 올리는 일은 UUiSubsystem::PushScreen이 한다.
	static UStoryPauseMenu* Create(APlayerController* OwningPlayer, FSimpleDelegate InOnClosed);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleResume();
	void HandleSkip();
	void HandleSettings();
	void HandleTitle();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UButton* AddRow(UVerticalBox& List, FName TextKey, FSimpleDelegate OnClicked);
	void CloseMenu();

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButtonClickRelay>> Relays;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeButton;

	FSimpleDelegate OnClosed;
};
