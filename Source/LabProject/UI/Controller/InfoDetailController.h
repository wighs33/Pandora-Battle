#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InfoDetailController.generated.h"

class UInfoWidget;
class UItemDetailWidget;
class UItemInstance;
class ULeftEquipmentWidget;
class UPandoraDescriptionWidget;
class UPandoraDefinition;
class USkinDefinition;
class UWidget;

/** Owns hover-detail widgets and their viewport positioning for the Info screen. */
UCLASS()
class LABPROJECT_API UInfoDetailController : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(
		UInfoWidget* InOwnerWidget,
		ULeftEquipmentWidget* InEquipmentWidget,
		TSubclassOf<UItemDetailWidget> InItemDetailWidgetClass,
		TSubclassOf<UPandoraDescriptionWidget> InPandoraDescriptionWidgetClass,
		FVector2D InPopupOffset);
	void Shutdown();

	void ShowItem(UItemInstance* ItemInstance, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);
	void ShowSkinDefinition(const USkinDefinition* SkinDefinition, UWidget* AnchorWidget, bool bPlaceLeftOfWidget);
	void ShowPandora(
		const UPandoraDefinition* PandoraDefinition,
		UWidget* AnchorWidget,
		bool bPlaceLeftOfWidget,
		bool bPlayShowAnimation);
	void HideAll();
	void HidePandoraForAnchor(const UWidget* AnchorWidget);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	UItemDetailWidget* GetOrCreateItemDetailWidget();
	UPandoraDescriptionWidget* GetOrCreatePandoraDescriptionWidget();
	void PositionAdjacent(UUserWidget* DetailWidget, const UWidget* AnchorWidget, bool bPlaceLeftOfWidget) const;
	UItemInstance* ResolveEquippedItemForComparison(UItemInstance* HoveredItem) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> OwnerWidget;
	UPROPERTY(Transient)
	TObjectPtr<ULeftEquipmentWidget> EquipmentWidget;
	UPROPERTY(Transient)
	TSubclassOf<UItemDetailWidget> ItemDetailWidgetClass;
	UPROPERTY(Transient)
	TSubclassOf<UPandoraDescriptionWidget> PandoraDescriptionWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UItemDetailWidget> ItemDetailWidget;
	UPROPERTY(Transient)
	TObjectPtr<UPandoraDescriptionWidget> PandoraDescriptionWidget;

	TWeakObjectPtr<UWidget> ActivePandoraAnchor;
	TWeakObjectPtr<const UPandoraDefinition> ActivePandoraDefinition;
	FVector2D PopupOffset = FVector2D(18.0f, 0.0f);
};
