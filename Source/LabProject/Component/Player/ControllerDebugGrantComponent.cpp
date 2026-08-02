#include "Component/Player/ControllerDebugGrantComponent.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerDebugGrantComponent)

UControllerDebugGrantComponent::UControllerDebugGrantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UControllerDebugGrantComponent::RequestGrantTestResources()
{
#if !UE_BUILD_SHIPPING
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	if (Controller->HasAuthority())
	{
		GrantTestResourcesOnServer();
		return;
	}

	Controller->Server_GrantDebugTestResources();
#endif
}

void UControllerDebugGrantComponent::GrantTestResourcesOnServer() const
{
#if !UE_BUILD_SHIPPING
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	APdPlayerState* PdPlayerState = Controller->GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return;
	}

	if (UInventoryComponent* InventoryComponent = PdPlayerState->GetInventoryComponent())
	{
		TArray<FPrimaryAssetId> WeaponDefinitionIds;
		CollectAllWeaponDefinitionIds(WeaponDefinitionIds);
		if (!WeaponDefinitionIds.IsEmpty())
		{
			InventoryComponent->AddItemsByPrimaryAssetIds(WeaponDefinitionIds);
		}
	}

	if (UPandoraTreeComponent* PandoraTreeComponent = PdPlayerState->GetPandoraTreeComponent())
	{
		PandoraTreeComponent->AddSoulDust(FMath::Max(Settings.SoulDustGrantAmount, 0));
	}

	if (UStatUpgradeComponent* StatUpgradeComponent = PdPlayerState->GetStatUpgradeComponent())
	{
		StatUpgradeComponent->GrantPointsToAllCategories(
			FMath::Max(Settings.StatusPointGrantAmount, 0.0f));
	}
#endif
}

APdPlayerController* UControllerDebugGrantComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

void UControllerDebugGrantComponent::CollectAllWeaponDefinitionIds(
	TArray<FPrimaryAssetId>& OutWeaponDefinitionIds) const
{
	OutWeaponDefinitionIds.Reset();

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> ItemDefinitionIds;
	AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("ItemDefinition")), ItemDefinitionIds);
	if (ItemDefinitionIds.IsEmpty())
	{
		return;
	}

	const FGameplayTag WeaponTypeTag = UProjectTagConfig::Get(this)->GetItemWeaponTypeTag();
	for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
	{
		const FSoftObjectPath ItemDefinitionPath = AssetManager.GetPrimaryAssetPath(ItemDefinitionId);
		const UItemDefinition* ItemDefinition =
			Cast<UItemDefinition>(ItemDefinitionPath.TryLoad());
		if (ItemDefinition && ItemDefinition->IsWeaponDefinition(WeaponTypeTag))
		{
			OutWeaponDefinitionIds.AddUnique(ItemDefinitionId);
		}
	}
}
