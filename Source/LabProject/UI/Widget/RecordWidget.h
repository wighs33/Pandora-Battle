#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SavedGameData/PdSaveGame.h"
#include "RecordWidget.generated.h"

class UButton;
struct FStreamableHandle;
class UImage;
class UPanelWidget;
class UProgressBar;
class URecordEntryWidget;
class URecordDefinition;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URecordWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URecordWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Record")
	void RefreshRecords();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UPanelWidget> RecordScrollBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UImage> Img_MyTier;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UTextBlock> Txt_WinCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Record|Bind")
	TObjectPtr<UProgressBar> TierProgressBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record|Setup")
	TSubclassOf<URecordEntryWidget> RecordEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record|Setup", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<URecordDefinition> RecordData;

private:
	UFUNCTION()
	void HandleCloseClicked();

	void ResolveWidgets();
	void BindWidgets();
	void UnbindWidgets();
	void BeginContentPreload();
	void BeginTierImagePreload(int32 PreloadGeneration);
	void ReleaseContentPreloads();
	void ApplyWidgetDefinitionSettings();
	FString ResolveRecordPlayerId() const;
	TSubclassOf<URecordEntryWidget> ResolveRecordEntryWidgetClass() const;
	const URecordDefinition* ResolveRecordDefinition();
	void ApplyTierImage(const FString& PlayerId);
	void ApplyWinCountUI(const FString& PlayerId);

	bool bWidgetsBound = false;
	int32 MaxVisibleRecordEntries = 5;
	float MaxTierProgressWinCount = 160.0f;
	int32 ContentPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> RecordDefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> TierImagePreloadHandle;
};
