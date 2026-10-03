#include "UI/Info/Skin/RightSkinWidget.h"

#include "Character/CharacterBase.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Components/Button.h"
#include "Components/TileView.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Info/Skin/SkinSlotViewData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightSkinWidget)

void URightSkinWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshSkinEquipmentBinding();
}

void URightSkinWidget::NativeDestruct()
{
	ClearSkinEquipmentBinding();

	Super::NativeDestruct();
}

void URightSkinWidget::ApplyWidgetDefinitionSettings()
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const FSkinWidgetSettings* Settings = WidgetDefinition ? &WidgetDefinition->GetSkinWidgetSettings() : nullptr;
	if (Settings)
	{
		SkinSlotCount = FMath::Max(Settings->SkinSlotCount, 0);
	}

	AddTypeFilter(CosmeticsButton, Settings ? Settings->CosmeticsTypeTag : FGameplayTag(), &UProjectTagDefinition::GetSkinCosmeticsTypeTag);
	AddTypeFilter(GestureButton, Settings ? Settings->GestureTypeTag : FGameplayTag(), &UProjectTagDefinition::GetSkinGestureTypeTag);
	AddTypeFilter(RidingButton, Settings ? Settings->RidingTypeTag : FGameplayTag(), &UProjectTagDefinition::GetSkinRidingTypeTag);
	AddTypeFilter(PetButton, Settings ? Settings->PetTypeTag : FGameplayTag(), &UProjectTagDefinition::GetSkinPetTypeTag);
}

// 검색 중에는 맞는 스킨만, 아니면 빈 칸을 포함해 슬롯 수만큼 보여 준다.
void URightSkinWidget::RebuildTileView()
{
	if (!TileView)
	{
		return;
	}

	TileView->ClearListItems();
	CachedSlotViewData.Reset();
	RefreshSkinEquipmentBinding();

	TSet<const USkinDefinition*> AssignedSkinDefinitions;
	if (const USkinEquipmentComponent* SkinEquipment = BoundSkinEquipmentComponent.Get())
	{
		TArray<FEquippedSkinSlot> EquippedSkinSlots;
		SkinEquipment->GetEquippedSkinSlots(EquippedSkinSlots);
		for (const FEquippedSkinSlot& EquippedSkinSlot : EquippedSkinSlots)
		{
			if (const USkinDefinition* SkinDefinition = EquippedSkinSlot.SkinDefinition.Get())
			{
				AssignedSkinDefinitions.Add(SkinDefinition);
			}
		}
	}

	TArray<const USkinDefinition*> SkinDefinitions;
	SkinDefinitions.Reserve(CachedSourceListItems.Num());
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(ListItem.Get());
		if (SkinDefinition && MatchesSearch(SkinDefinition, SkinDefinition->DisplayName, ActiveSearchText))
		{
			SkinDefinitions.Add(SkinDefinition);
		}
	}

	const int32 SlotCountToDisplay = IsSearching() ? SkinDefinitions.Num() : FMath::Max(SkinSlotCount, SkinDefinitions.Num());
	CachedSlotViewData.Reserve(SlotCountToDisplay);

	for (int32 SlotIndex = 0; SlotIndex < SlotCountToDisplay; ++SlotIndex)
	{
		const USkinDefinition* SkinDefinition = SkinDefinitions.IsValidIndex(SlotIndex)
			? SkinDefinitions[SlotIndex]
			: nullptr;
		USkinSlotViewData* SlotViewData = NewObject<USkinSlotViewData>(this);
		SlotViewData->Initialize(
			SlotIndex,
			SkinDefinition,
			AssignedSkinDefinitions.Contains(SkinDefinition));
		CachedSlotViewData.Add(SlotViewData);
		TileView->AddItem(SlotViewData);
	}
}

void URightSkinWidget::HandleEquippedSkinsChanged()
{
	RebuildTileView();
}

void URightSkinWidget::RefreshSkinEquipmentBinding()
{
	const ACharacterBase* Character =
		Cast<ACharacterBase>(GetOwningPlayerPawn());
	USkinEquipmentComponent* ResolvedSkinEquipment = Character
		? Character->GetSkinEquipmentComponent()
		: nullptr;
	if (BoundSkinEquipmentComponent.Get() == ResolvedSkinEquipment)
	{
		return;
	}

	ClearSkinEquipmentBinding();
	BoundSkinEquipmentComponent = ResolvedSkinEquipment;
	if (ResolvedSkinEquipment)
	{
		ResolvedSkinEquipment->OnEquippedSkinsChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleEquippedSkinsChanged);
	}
}

void URightSkinWidget::ClearSkinEquipmentBinding()
{
	if (USkinEquipmentComponent* SkinEquipment =
		BoundSkinEquipmentComponent.Get())
	{
		SkinEquipment->OnEquippedSkinsChanged.RemoveDynamic(
			this,
			&ThisClass::HandleEquippedSkinsChanged);
	}
	BoundSkinEquipmentComponent.Reset();
}
