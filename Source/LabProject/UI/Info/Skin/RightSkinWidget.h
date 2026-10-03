#pragma once

#include "CoreMinimal.h"
#include "UI/Info/RightListPanelWidget.h"
#include "RightSkinWidget.generated.h"

class USkinEquipmentComponent;
class USkinDefinition;
class USkinSlotViewData;

/** 보유 스킨 목록. 빈 칸까지 슬롯 수만큼 보여 주고, 장착 중인 스킨을 표시한다. */
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightSkinWidget : public URightListPanelWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void ApplyWidgetDefinitionSettings() override;
	virtual void RebuildTileView() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	int32 GetSkinSlotCount() const { return SkinSlotCount; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleEquippedSkinsChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshSkinEquipmentBinding();
	void ClearSkinEquipmentBinding();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> CosmeticsButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> GestureButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidget))
	TObjectPtr<UButton> RidingButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Skin", meta = (BindWidgetOptional))
	TObjectPtr<UButton> PetButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Skin|Slots", meta = (ClampMin = "0"))
	int32 SkinSlotCount = 40;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin|Slots")
	TArray<TObjectPtr<USkinSlotViewData>> CachedSlotViewData;

private:
	TWeakObjectPtr<USkinEquipmentComponent> BoundSkinEquipmentComponent;
};
