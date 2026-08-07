#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QuickSlotWidget.generated.h"

class UInventoryComponent;
class UQuickSlotEntryWidget;
class USkinDefinition;
class USkinEquipmentComponent;
class UUniformGridPanel;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|QuickSlot")
	void FillQuickSlotBar();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void InitializeInventoryBinding();
	void BindInventoryChangedEvent();
	void UnbindInventoryChangedEvent();
	void HandleInventoryChanged();
	void RefreshQuickSlotIconPreload();
	void ReleaseQuickSlotIconPreload();
	UFUNCTION()
	void HandleSkinEquipmentChanged();
	void RebuildQuickSlotBar();
	void AddQuickSlotEntry(int32 SlotIndex, UInventoryComponent* InventoryComponent, USkinEquipmentComponent* SkinEquipmentComponent);
	UQuickSlotEntryWidget* CreateQuickSlotEntryWidget() const;
	void AddWidgetToBar(UWidget* Widget, int32 SlotIndex) const;
	UInventoryComponent* ResolveOwningInventoryComponent() const;
	USkinEquipmentComponent* ResolveOwningSkinEquipmentComponent() const;
	TSubclassOf<UQuickSlotEntryWidget> ResolveEntryWidgetClass() const;
	const USkinDefinition* ResolveGestureSlotSkinDefinition(const USkinEquipmentComponent* SkinEquipmentComponent, int32 QuickSlotIndex) const;
	int32 ResolveSlotCount() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|QuickSlot|Classes", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UQuickSlotEntryWidget> EntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|QuickSlot", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 SlotCount = 8;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> UniformGridPanel;

	UPROPERTY(Transient)
	TWeakObjectPtr<UInventoryComponent> BoundInventoryComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkinEquipmentComponent> BoundSkinEquipmentComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UQuickSlotEntryWidget>> EntryWidgets;

	TArray<FSoftObjectPath> PreloadedQuickSlotIconPaths;
	TSharedPtr<FStreamableHandle> QuickSlotIconPreloadHandle;
	uint32 QuickSlotIconPreloadGeneration = 0;

	FTimerHandle RetryInitializeTimerHandle;
	FTimerHandle RebuildBarTimerHandle;
};
