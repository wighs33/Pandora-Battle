#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotEntryWidget.generated.h"

class UImage;
class UInputAction;
class UItemInstance;
class USkinDefinition;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UQuickSlotEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|QuickSlot")
	void SetQuickSlotData(int32 InSlotIndex, UItemInstance* InItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!UI|QuickSlot")
	void SetGestureSlotData(int32 InSlotIndex, const USkinDefinition* InSkinDefinition);

	UFUNCTION(BlueprintPure, Category = "!UI|QuickSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|QuickSlot")
	UItemInstance* GetItemInstance() const { return ItemInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|QuickSlot")
	const USkinDefinition* GetSkinDefinition() const { return SkinDefinition; }

	UFUNCTION(BlueprintCallable, Category = "!UI|QuickSlot")
	void RefreshVisual();

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

private:
	void CacheOptionalWidgets();
	void ApplyItemVisual();
	void ApplyInputKeyIcon();
	UInputAction* ResolveInputAction() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|QuickSlot", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 SlotIndex = 0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|QuickSlot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UItemInstance> ItemInstance = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|QuickSlot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const USkinDefinition> SkinDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|QuickSlot|Input", meta = (AllowPrivateAccess = "true"))
	bool bHideInputKeyIcon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|QuickSlot|Input", meta = (AllowPrivateAccess = "true"))
	FVector2D InputKeyIconSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityTextBlock;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyIcon;
};
