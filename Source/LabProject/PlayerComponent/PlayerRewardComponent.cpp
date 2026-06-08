#include "PlayerComponent/PlayerRewardComponent.h"

#include "Character/PdPlayer.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Item/InventoryComponent.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraComponent.h"
#include "PlayerComponent/PlayerNotificationComponent.h"
#include "Skin/SkinComponent.h"
#include "UObject/PrimaryAssetId.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerRewardComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPlayerRewardComponent, Log, All);

UPlayerRewardComponent::UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPlayerRewardComponent::ApplyInteractRewards_Implementation(AActor* InteractableActor)
{
	UE_LOG(LogPlayerRewardComponent, Log,
		TEXT("[Reward] server request received. component=%s owner=%s target=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(InteractableActor));
	ApplyInteractRewardsInternal(InteractableActor);
}

bool UPlayerRewardComponent::ApplyInteractRewardsInternal(AActor* InteractableActor)
{
	APdPlayerState* PlayerState = GetPdPlayerState();
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] failed: invalid authority state. component=%s owner=%s playerState=%s hasAuthority=%s target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(PlayerState),
			PlayerState && PlayerState->HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(InteractableActor));
		return false;
	}

	if (!IsValid(InteractableActor) || !InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] failed: target is invalid or not interactable. playerState=%s target=%s class=%s implements=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(InteractableActor),
			*GetNameSafe(IsValid(InteractableActor) ? InteractableActor->GetClass() : nullptr),
			IsValid(InteractableActor) && InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()) ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (APdPlayer* PlayerPawn = Cast<APdPlayer>(PlayerState->GetPawn()))
	{
		if (!PlayerPawn->CanInteractWithActor(InteractableActor))
		{
			UE_LOG(LogPlayerRewardComponent, Warning,
				TEXT("[Reward] failed: player cannot interact with target. player=%s target=%s"),
				*GetNameSafe(PlayerPawn),
				*GetNameSafe(InteractableActor));
			return false;
		}
	}
	else
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] warning: player state pawn is not APdPlayer. playerState=%s pawn=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PlayerState->GetPawn()));
	}

	if (!IInteractableInterface::Execute_CanInteract(InteractableActor, PlayerState->GetPawn()))
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] failed: target rejected interaction. playerState=%s pawn=%s target=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(PlayerState->GetPawn()),
			*GetNameSafe(InteractableActor));
		return false;
	}

	TArray<FPrimaryAssetId> RewardItemDefinitions;
	TArray<FPrimaryAssetId> RewardSkinDefinitions;
	TArray<FPrimaryAssetId> RewardPandoraDefinitions;
	IInteractableInterface::Execute_GetRewardItems(InteractableActor, RewardItemDefinitions);
	IInteractableInterface::Execute_GetRewardSkins(InteractableActor, RewardSkinDefinitions);
	IInteractableInterface::Execute_GetRewardPandoras(InteractableActor, RewardPandoraDefinitions);

	UE_LOG(LogPlayerRewardComponent, Log,
		TEXT("[Reward] target rewards read. target=%s items=%d skins=%d pandoras=%d"),
		*GetNameSafe(InteractableActor),
		RewardItemDefinitions.Num(),
		RewardSkinDefinitions.Num(),
		RewardPandoraDefinitions.Num());

	if (UInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent())
	{
		UE_LOG(LogPlayerRewardComponent, Log,
			TEXT("[Reward] adding item rewards. inventory=%s count=%d"),
			*GetNameSafe(InventoryComponent),
			RewardItemDefinitions.Num());
		InventoryComponent->AddItemsByPrimaryAssetIds(RewardItemDefinitions);
	}
	else
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] skipped item rewards: InventoryComponent missing. playerState=%s count=%d"),
			*GetNameSafe(PlayerState),
			RewardItemDefinitions.Num());
	}

	if (USkinComponent* SkinComponent = PlayerState->GetSkinComponent())
	{
		UE_LOG(LogPlayerRewardComponent, Log,
			TEXT("[Reward] adding skin rewards. skinComponent=%s count=%d"),
			*GetNameSafe(SkinComponent),
			RewardSkinDefinitions.Num());
		SkinComponent->MakeAndAddSkins(RewardSkinDefinitions);
	}
	else
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] skipped skin rewards: SkinComponent missing. playerState=%s count=%d"),
			*GetNameSafe(PlayerState),
			RewardSkinDefinitions.Num());
	}

	if (UPandoraComponent* PandoraComponent = PlayerState->GetPandoraComponent())
	{
		UE_LOG(LogPlayerRewardComponent, Log,
			TEXT("[Reward] adding pandora rewards. pandoraComponent=%s count=%d"),
			*GetNameSafe(PandoraComponent),
			RewardPandoraDefinitions.Num());
		PandoraComponent->ActivatePandoras(RewardPandoraDefinitions);
	}
	else
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] skipped pandora rewards: PandoraComponent missing. playerState=%s count=%d"),
			*GetNameSafe(PlayerState),
			RewardPandoraDefinitions.Num());
	}

	UE_LOG(LogPlayerRewardComponent, Log,
		TEXT("[Reward] sending notifications and claiming target. target=%s receiverPawn=%s"),
		*GetNameSafe(InteractableActor),
		*GetNameSafe(PlayerState->GetPawn()));
	if (UPlayerNotificationComponent* NotificationComponent = PlayerState->GetPlayerNotificationComponent())
	{
		NotificationComponent->SendRewardNotifications(RewardItemDefinitions, RewardSkinDefinitions, RewardPandoraDefinitions);
	}
	else
	{
		UE_LOG(LogPlayerRewardComponent, Warning,
			TEXT("[Reward] skipped notifications: PlayerNotificationComponent missing. playerState=%s target=%s"),
			*GetNameSafe(PlayerState),
			*GetNameSafe(InteractableActor));
	}
	IInteractableInterface::Execute_OnRewardsClaimed(InteractableActor, PlayerState->GetPawn());

	UE_LOG(LogPlayerRewardComponent, Log,
		TEXT("[Reward] complete. target=%s"),
		*GetNameSafe(InteractableActor));
	return true;
}

APdPlayerState* UPlayerRewardComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}
