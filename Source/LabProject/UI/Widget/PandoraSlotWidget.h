#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "PandoraSlotWidget.generated.h"

class UImage;
class UPandoraInstance;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraSlotWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetData(UPandoraInstance* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UPandoraInstance* GetCachedData() const { return CachedData; }

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraInstance> CachedData;
};
