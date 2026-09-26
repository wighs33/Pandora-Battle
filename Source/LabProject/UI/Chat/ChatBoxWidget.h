#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "Types/SlateEnums.h"
#include "ChatBoxWidget.generated.h"

class UChatControllerComponent;
class UChatEntryWidget;
class UEditableText;
class UScrollBox;

UCLASS()
class LABPROJECT_API UChatBoxWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Public API ------------------------------------------------------------------------------------------------------
	void InitializeChat(
		UChatControllerComponent* InChatControllerComponent,
		TSubclassOf<UChatEntryWidget> InChatEntryWidgetClass,
		float InScrollMultiplier);

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void FocusChat();

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void ExitChat();

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void Scroll(bool bUp);

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void SubmitChatInput();

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void AddChatMessage(const FString& Message);

	bool IsChatFocused() const { return bChatFocused; }

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	UFUNCTION()
	void HandleChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SetChatInputEnabled(bool bEnabled) const;
	FString GetChatInputText() const;
	void SetChatInputText(const FText& Text) const;
	UEditableText* GetChatInputWidget() const;
	UScrollBox* GetChatScrollBox() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!Chat|Bind")
	TObjectPtr<UScrollBox> ScrollBox_ChatMessages;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!Chat|Bind")
	TObjectPtr<UEditableText> TxtBox_ChatInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|UI")
	TSubclassOf<UChatEntryWidget> ChatEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|Input", meta = (ClampMin = "1.0"))
	float ScrollMultiplier = 60.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UChatControllerComponent> ChatControllerComponent;

	bool bChatFocused = false;
};
