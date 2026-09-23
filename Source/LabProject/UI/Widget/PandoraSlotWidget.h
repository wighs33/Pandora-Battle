#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/PandoraWidgetViewData.h"
#include "PandoraSlotWidget.generated.h"

class UImage;
class UPandoraComponent;
class UPandoraDefinition;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraSlotWidget : public ULocalizedMenuWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetData(const UPandoraDefinition* Target);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	const UPandoraDefinition* GetCachedData() const { return CachedData; }

protected:
	virtual void OnMenuLanguageChanged() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> TextBlock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> OwnershipText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> EquippedMark;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> HoverBorder;

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
	TObjectPtr<const UPandoraDefinition> CachedData;

private:
	UFUNCTION()
	void RefreshOwnership();
	void UnbindPandoraEvents();
	TWeakObjectPtr<UPandoraComponent> BoundPandoraComponent;

	void ApplyViewData(const FPandoraSlotViewData& ViewData);
	void RefreshWeaponRequirementImages(const FGameplayTagContainer& RequiredWeaponTags) const;
	void HideAllWeaponRequirementImages() const;
	void SetWeaponRequirementImageVisible(UImage* Image, bool bVisible) const;

	bool bIsHoverActive = false;
};
