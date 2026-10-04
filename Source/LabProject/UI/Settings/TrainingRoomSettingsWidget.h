#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "TrainingRoomSettingsWidget.generated.h"

class FContentLease;
class UButtonClickRelay;
class UCheckBox;
class UImage;
class UItemDefinition;

/** 훈련 봇 무기 버튼 하나. 맨손 버튼은 무기 정의 없이 bUseUnarmed만 켠다. */
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

	/** 선택되면 색이 바뀌는 테두리 이미지. 선택되지 않아도 기본 색으로 보인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot")
	FName SelectionBorderImageName;
};

/** 게임 설정의 훈련장 탭. 훈련 봇의 무기와 공격 여부를 바꾸며, 훈련장이 아닌 맵에서는 아무것도 하지 않는다. */
UCLASS()
class LABPROJECT_API UTrainingRoomSettingsWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Training|Bot")
	void SelectWeaponOption(int32 OptionIndex);

	UFUNCTION(BlueprintPure, Category = "!Training|Bot")
	int32 GetSelectedWeaponOptionIndex() const { return SelectedOptionIndex; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleBotCanAttackChanged(bool bIsChecked);

	void CompletePendingWeaponSelection(int32 OptionIndex);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyWeaponOption(int32 OptionIndex);
	void ApplyAttackEnabledToTrainingBots(bool bEnabled) const;
	void SyncSelectionFromTrainingBot();
	int32 FindWeaponOptionIndex(const UItemDefinition* WeaponDefinition) const;
	void RefreshSelectionBorders();
	void BeginWeaponPreload();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot", meta = (IncludeAssetBundles))
	TArray<FTrainingBotWeaponOption> WeaponOptions;

	/** 훈련 봇이 아직 없을 때 공격 체크 상자에 보여 줄 값. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot")
	bool bInitialBotAttackEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Training|Bot|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!Training|Bot", meta = (BindWidget))
	TObjectPtr<UCheckBox> Chk_BotCanAttack;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButtonClickRelay>> WeaponClickRelays;

	int32 SelectedOptionIndex = INDEX_NONE;
	bool bActiveInTrainingRoom = false;
	TSharedPtr<FContentLease> WeaponPreloadLease;
	TSharedPtr<FContentLease> PendingWeaponLease;
};
