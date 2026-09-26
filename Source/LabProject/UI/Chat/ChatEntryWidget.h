#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ChatEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class LABPROJECT_API UChatEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void SetMessage(const FString& InMessage);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshUI();
	UTextBlock* GetMessageTextBlock() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Chat|Bind")
	TObjectPtr<UTextBlock> Txt_Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!Chat")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Chat|Fallback")
	TArray<FName> MessageTextCandidateNames =
	{
		TEXT("Txt_Message"),
		TEXT("Text_Message"),
		TEXT("TextBlock_Message"),
		TEXT("MessageText"),
		TEXT("Txt_ChatMessage"),
		TEXT("TextBlock")
	};
};
