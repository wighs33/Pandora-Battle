#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "ItemSlotWidget.generated.h"

class UItemInstance;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Inventory")
	void SetData(UItemInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Inventory")
	UItemInstance* GetCachedData() const { return CachedData; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Inventory|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Inventory")
	TObjectPtr<UItemInstance> CachedData;
};
