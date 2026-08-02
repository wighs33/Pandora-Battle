#include "Component/Player/PlayerRewardComponent.h"

#include "Character/PdPlayer.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Item/RewardChest.h"
#include "Component/Item/InventoryComponent.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "Component/Skin/SkinComponent.h"
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

	if (APdPlayer* PlayerPawn = Cast<APdPlayer>(PlayerState->GetPawn()))
	{
		if (!PlayerPawn->CanInteractWithActor(InteractableActor))
		{

			return false;
		}
	}

	if (!IInteractableInterface::Execute_CanInteract(InteractableActor, PlayerState->GetPawn()))
	{

		return false;
	}

	TArray<FPrimaryAssetId> RewardItemDefinitions;
	TArray<FPrimaryAssetId> RewardSkinDefinitions;
	TArray<FPrimaryAssetId> RewardPandoraDefinitions;
	UInventoryComponent* InventoryComponent =
		PlayerState->GetInventoryComponent();
	if (ARewardChest* RewardChest = Cast<ARewardChest>(InteractableActor))
	{
		RewardChest->GetRewardItemsForInventory(
			InventoryComponent,
			RewardItemDefinitions);
	}
	else
	{
		IInteractableInterface::Execute_GetRewardItems(
			InteractableActor,
			RewardItemDefinitions);
	}
	IInteractableInterface::Execute_GetRewardSkins(InteractableActor, RewardSkinDefinitions);
	IInteractableInterface::Execute_GetRewardPandoras(InteractableActor, RewardPandoraDefinitions);



	if (InventoryComponent)
	{

		InventoryComponent->AddItemsByPrimaryAssetIds(RewardItemDefinitions);
		if (!RewardItemDefinitions.IsEmpty())
		{
			FString PlayerId;
			if (const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId(); UniqueId.IsValid())
			{
				if (const FUniqueNetIdPtr UniqueNetId = UniqueId.GetUniqueNetId(); UniqueNetId.IsValid())
				{
					PlayerId = UniqueNetId->ToString();
				}
			}

			APdPlayerController* PlayerController = Cast<APdPlayerController>(PlayerState->GetOwner());
			if (!PlayerController)
			{
				if (const APawn* PlayerPawn = Cast<APawn>(PlayerState->GetPawn()))
				{
					PlayerController = Cast<APdPlayerController>(PlayerPawn->GetController());
				}
			}

			if (PlayerController)
			{
				PlayerController->Client_AddCollectedItemCount(PlayerId, RewardItemDefinitions.Num());
			}
		}
	}

	if (USkinComponent* SkinComponent = PlayerState->GetSkinComponent())
	{

		SkinComponent->MakeAndAddSkins(RewardSkinDefinitions);
	}

	if (UPandoraComponent* PandoraComponent = PlayerState->GetPandoraComponent())
	{

		PandoraComponent->ActivatePandoras(RewardPandoraDefinitions);
	}

	if (UPlayerNotificationComponent* NotificationComponent = PlayerState->GetPlayerNotificationComponent())
	{
		NotificationComponent->SendRewardNotifications(RewardItemDefinitions, RewardSkinDefinitions, RewardPandoraDefinitions);
	}
	IInteractableInterface::Execute_OnRewardsClaimed(InteractableActor, PlayerState->GetPawn());


	return true;
}

APdPlayerState* UPlayerRewardComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}
