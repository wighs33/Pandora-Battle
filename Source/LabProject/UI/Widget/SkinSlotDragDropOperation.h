#pragma once

#include "Blueprint/DragDropOperation.h"
#include "SkinSlotDragDropOperation.generated.h"

class USkinInstance;
class USkinSlotViewData;

UCLASS(BlueprintType)
class LABPROJECT_API USkinSlotDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	void Initialize(int32 InSourceSlotIndex, USkinInstance* InSkinInstance, USkinSlotViewData* InSourceSlotData);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	int32 GetSourceSlotIndex() const { return SourceSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	USkinInstance* GetSkinInstance() const { return SkinInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin|DragDrop")
	USkinSlotViewData* GetSourceSlotData() const { return SourceSlotData; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	int32 SourceSlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinInstance> SkinInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin|DragDrop", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinSlotViewData> SourceSlotData;
};
