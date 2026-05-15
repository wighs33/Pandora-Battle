#include "PlayerComponent/PlayerRewardComponent.h"

#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Item/InventoryComponent.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraComponent.h"
#include "Skin/SkinComponent.h"
#include "UObject/PrimaryAssetId.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerRewardComponent)

UPlayerRewardComponent::UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPlayerRewardComponent::ApplyInteractRewards_Implementation(AActor* InteractableActor)
{
	ApplyInteractRewardsInternal(InteractableActor);
}

bool UPlayerRewardComponent::ApplyInteractRewardsInternal(AActor* InteractableActor)
{
	APdPlayerState* PlayerState = GetPdPlayerState();
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}

	if (!IsValid(InteractableActor) || !InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	TArray<FPrimaryAssetId> RewardItemDefinitions;
	TArray<FPrimaryAssetId> RewardSkinDefinitions;
	TArray<FPrimaryAssetId> RewardPandoraDefinitions;
	IInteractableInterface::Execute_GetRewardItems(InteractableActor, RewardItemDefinitions);
	IInteractableInterface::Execute_GetRewardSkins(InteractableActor, RewardSkinDefinitions);
	IInteractableInterface::Execute_GetRewardPandoras(InteractableActor, RewardPandoraDefinitions);

	if (UInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent())
	{
		InventoryComponent->AddItemsByPrimaryAssetIds(RewardItemDefinitions);
	}

	if (USkinComponent* SkinComponent = PlayerState->GetSkinComponent())
	{
		SkinComponent->MakeAndAddSkins(RewardSkinDefinitions);
	}

	if (UPandoraComponent* PandoraComponent = PlayerState->GetPandoraComponent())
	{
		PandoraComponent->ActivatePandoras(RewardPandoraDefinitions);
	}

	return true;
}

APdPlayerState* UPlayerRewardComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}
