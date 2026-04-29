#include "PlayerComponent/PlayerRewardComponent.h"

#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Item/InventoryComponent.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraComponent.h"
#include "Skin/SkinComponent.h"
#include "UObject/PrimaryAssetId.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerRewardComponent)

/** 상호작용 보상을 적용합니다. */
bool UPlayerRewardComponent::ApplyInteractRewards(AActor* InteractableActor)
{
	// =================================================================================================================
	// === 권한 및 소유자 검사

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor);
	if (!PlayerState)
	{
		return false;
	}

	// =================================================================================================================
	// === 상호작용 액터 검사

	if (!IsValid(InteractableActor) || !InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	// =================================================================================================================
	// === 보상 목록 수집

	TArray<FPrimaryAssetId> RewardItemDefinitions;
	TArray<FPrimaryAssetId> RewardSkinDefinitions;
	TArray<FPrimaryAssetId> RewardPandoraDefinitions;
	IInteractableInterface::Execute_GetRewardItems(InteractableActor, RewardItemDefinitions);
	IInteractableInterface::Execute_GetRewardSkins(InteractableActor, RewardSkinDefinitions);
	IInteractableInterface::Execute_GetRewardPandoras(InteractableActor, RewardPandoraDefinitions);

	// =================================================================================================================
	// === 아이템 보상 지급

	if (UInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent())
	{
		InventoryComponent->AddItemsByPrimaryAssetIds(RewardItemDefinitions);
	}

	// =================================================================================================================
	// === 스킨 보상 지급

	if (USkinComponent* SkinComponent = PlayerState->GetSkinComponent())
	{
		SkinComponent->MakeAndAddSkins(RewardSkinDefinitions);
	}

	// =================================================================================================================
	// === 판도라 보상 지급

	if (UPandoraComponent* PandoraComponent = PlayerState->GetPandoraComponent())
	{
		PandoraComponent->ActivatePandoras(RewardPandoraDefinitions);
	}

	return true;
}