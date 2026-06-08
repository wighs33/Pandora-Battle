#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/PandoraWidgetViewData.h"
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
	virtual void NativeConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Axe;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Bow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Dagger;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Greatsword;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Sword;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind|Weapon")
	TObjectPtr<UImage> Gun;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraInstance> CachedData;

private:
	void ApplyViewData(const FPandoraSlotViewData& ViewData);
	void RefreshWeaponRequirementImages(const FGameplayTagContainer& RequiredWeaponTags) const;
	void HideAllWeaponRequirementImages() const;
	void SetWeaponRequirementImageVisible(UImage* Image, bool bVisible) const;
};
