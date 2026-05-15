#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "SkinSlotWidget.generated.h"

class USkinInstance;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USkinSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Skin")
	void SetData(USkinInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	USkinInstance* GetCachedData() const { return CachedData; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Skin|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Skin")
	TObjectPtr<USkinInstance> CachedData;
};
