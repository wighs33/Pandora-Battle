#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "ChatBoxWidget.generated.h"

class UChatControllerComponent;
class UChatEntryWidget;
class UEditableText;
class UScrollBox;

UCLASS()
class LABPROJECT_API UChatBoxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UChatBoxWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Chat|Bind")
	TObjectPtr<UScrollBox> ScrollBox_ChatMessages;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Chat|Bind")
	TObjectPtr<UEditableText> TxtBox_ChatInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|UI")
	TSubclassOf<UChatEntryWidget> ChatEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|Input", meta = (ClampMin = "1.0"))
	float ScrollMultiplier = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|Fallback")
	TArray<FName> ChatScrollBoxCandidateNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|Fallback")
	TArray<FName> ChatInputCandidateNames;

private:
	UFUNCTION()
	void HandleChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void ApplyWidgetDefinitionSettings();
	void CacheWidgets();
	bool ApplyRoutedChatInput(UEditableText* ChatInputText);
	bool ReleaseRoutedChatInput();
	void RestoreGameInputFallback() const;
	void SetChatInputEnabled(bool bEnabled) const;
	FString GetChatInputText() const;
	void SetChatInputText(const FText& Text) const;
	UEditableText* GetChatInputWidget() const;
	UScrollBox* GetChatScrollBox() const;

	UPROPERTY(Transient)
	TObjectPtr<UChatControllerComponent> ChatControllerComponent;

	UPROPERTY(Transient)
	TObjectPtr<UEditableText> CachedChatInputText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> CachedChatScrollBox;

	FGuid ChatModalInputToken;
	bool bChatFocused = false;
};
