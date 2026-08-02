#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "TrainingRoomMenuPopupWidget.generated.h"

class UButton;
class UCheckBox;
class UImage;
class UItemDefinition;
class UTrainingRoomMenuPopupWidget;
struct FStreamableHandle;

USTRUCT(BlueprintType)
struct FTrainingBotWeaponOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UItemDefinition> WeaponDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot")
	bool bUseUnarmed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot")
	FName ButtonWidgetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (ToolTip = "Image widget used like WBP_ItemSlot's SelectionBorderImage. It stays visible and changes color when selected."))
	FName SelectionBorderImageName;
};

UCLASS(Transient)
class LABPROJECT_API UTrainingBotWeaponOptionClickProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UTrainingRoomMenuPopupWidget* InOwnerWidget, int32 InOptionIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UTrainingRoomMenuPopupWidget> OwnerWidget;

	int32 OptionIndex = INDEX_NONE;
};

USTRUCT()
struct FTrainingBotWeaponOptionBinding
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UTrainingBotWeaponOptionClickProxy> ClickProxy;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTrainingBotWeaponSelectedSignature, UItemDefinition*, WeaponDefinition);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UTrainingRoomMenuPopupWidget : public UMenuPopupWidget
{
	GENERATED_BODY()

public:
	UTrainingRoomMenuPopupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	bool SelectTrainingBotWeaponOptionByIndex(int32 OptionIndex);

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	bool SelectTrainingBotDagger();

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	bool SelectTrainingBotUnarmed();

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	bool SelectTrainingBotWeaponDefinition(UItemDefinition* WeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	bool SelectTrainingBotWeaponDefinitionSoft(TSoftObjectPtr<UItemDefinition> WeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	void RefreshWeaponOptionSelectionVisuals();

	UFUNCTION(BlueprintPure, Category = "!Training|Menu")
	UItemDefinition* GetSelectedTrainingBotWeaponDefinition() const { return SelectedWeaponDefinition.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!Training|Menu")
	void SetTrainingBotAttackEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "!Training|Menu")
	bool IsTrainingBotAttackEnabled() const { return bTrainingBotAttackEnabled; }

	UFUNCTION(BlueprintImplementableEvent, Category = "!Training|Menu")
	void BP_OnTrainingBotWeaponSelected(UItemDefinition* WeaponDefinition);

	UPROPERTY(BlueprintAssignable, Category = "!Training|Menu")
	FTrainingBotWeaponSelectedSignature OnTrainingBotWeaponSelected;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (IncludeAssetBundles))
	TArray<FTrainingBotWeaponOption> WeaponOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UItemDefinition> InitialWeaponDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UItemDefinition> DaggerWeaponDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Built-In Buttons")
	FName DaggerButtonWidgetName = TEXT("Btn_Dagger");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Built-In Buttons")
	FName DaggerSelectionBorderImageName = TEXT("SelectionBorderImage_5");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Built-In Buttons")
	FName UnarmedButtonWidgetName = TEXT("Btn_Unarmed");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Built-In Buttons")
	FName UnarmedSelectionBorderImageName = TEXT("SelectionBorderImage_6");

	UPROPERTY(BlueprintReadWrite, Category = "!Training|Bot", meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> Chk_BotCanAttack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot")
	bool bInitialTrainingBotAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Training|Bot")
	TObjectPtr<UItemDefinition> SelectedWeaponDefinition;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Training|Bot")
	bool bTrainingBotAttackEnabled = true;

private:
	UFUNCTION()
	void HandleBotCanAttackCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleDaggerButtonClicked();

	UFUNCTION()
	void HandleUnarmedButtonClicked();

	bool SelectTrainingBotWeaponDefinitionInternal(
		UItemDefinition* WeaponDefinition,
		FName BuiltInButtonWidgetName);
	bool RequestTrainingBotWeaponSelection(
		TSoftObjectPtr<UItemDefinition> WeaponDefinition,
		FName BuiltInButtonWidgetName);
	void CompletePendingWeaponSelection(
		int32 SelectionGeneration,
		TSoftObjectPtr<UItemDefinition> WeaponDefinition,
		FName BuiltInButtonWidgetName);
	void BeginConfiguredWeaponPreload();
	void ReleaseConfiguredWeaponPreload();
	void CancelPendingWeaponSelection();
	bool SelectTrainingBotUnarmedInternal(int32 OptionIndex);
	bool ApplyWeaponToTrainingBot(UItemDefinition* WeaponDefinition) const;
	bool ApplyUnarmedToTrainingBot() const;
	bool ApplyAttackEnabledToTrainingBots(bool bEnabled) const;
	bool ResolveTrainingBotAttackEnabled() const;
	bool SyncSelectedWeaponFromTrainingBot();
	void SyncBotCanAttackCheckBox();
	void BindWeaponOptionButtons();
	void UnbindWeaponOptionButtons();
	int32 FindWeaponOptionIndex(const UItemDefinition* WeaponDefinition) const;
	int32 FindUnarmedWeaponOptionIndex() const;
	bool DoesSoftWeaponDefinitionMatch(TSoftObjectPtr<UItemDefinition> SoftWeaponDefinition, const UItemDefinition* WeaponDefinition) const;
	bool IsButtonNameConfiguredInWeaponOptions(FName ButtonWidgetName) const;
	UImage* GetOptionSelectionBorderImage(const FTrainingBotWeaponOption& Option) const;
	void SetBuiltInSelectionBorderState(FName SelectionBorderImageName, bool bSelected) const;

	UPROPERTY(Transient)
	TArray<FTrainingBotWeaponOptionBinding> WeaponOptionBindings;

	UPROPERTY(Transient)
	int32 SelectedWeaponOptionIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FName SelectedBuiltInButtonWidgetName;

	int32 PendingWeaponSelectionGeneration = 0;
	bool bPendingWeaponSelectionRequestActive = false;
	TSharedPtr<FStreamableHandle> ConfiguredWeaponPreloadHandle;
	TSharedPtr<FStreamableHandle> PendingWeaponSelectionHandle;
};
