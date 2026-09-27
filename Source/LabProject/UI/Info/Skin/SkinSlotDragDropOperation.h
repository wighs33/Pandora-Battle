#pragma once

#include "Blueprint/DragDropOperation.h"
#include "SkinSlotDragDropOperation.generated.h"

class USkinDefinition;
class USkinSlotViewData;

UCLASS(BlueprintType)
class LABPROJECT_API USkinSlotDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(int32 InSourceSlotIndex, const USkinDefinition* InSkinDefinition, USkinSlotViewData* InSourceSlotData);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	int32 GetSourceSlotIndex() const { return SourceSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	const USkinDefinition* GetSkinDefinition() const { return SkinDefinition; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	USkinSlotViewData* GetSourceSlotData() const { return SourceSlotData; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const USkinDefinition> SkinDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinSlotViewData> SourceSlotData;
};
