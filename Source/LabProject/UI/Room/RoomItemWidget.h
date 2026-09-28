#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "RoomItemWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URoomItemWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Room")
	void SetInfo(const FBlueprintSessionResult& InSessionResult);

	UFUNCTION(BlueprintCallable, Category = "!Room")
	void RefreshUI();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override { RefreshUI(); }

	UFUNCTION()
	void HandleJoinClicked();

	void HandleJoinSessionComplete(uint64 RequestId, bool bWasSuccessful);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UTextBlock> Txt_RoomName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UTextBlock> Txt_PlayerCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UTextBlock> Txt_MapName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Room")
	FBlueprintSessionResult Result;

private:
	FDelegateHandle JoinSessionCompleteHandle;
	uint64 ActiveJoinRequestId = 0;
};
