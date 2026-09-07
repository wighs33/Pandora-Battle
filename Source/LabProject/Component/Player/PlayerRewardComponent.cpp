#include "Component/Player/PlayerRewardComponent.h"

#include "Character/PdPlayer.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "Item/RewardChest.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "Component/Player/LevelingComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Definition/Item/RewardDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UObject/PrimaryAssetId.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerRewardComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPlayerReward, Log, All);

UPlayerRewardComponent::UPlayerRewardComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

// 서버에서 프로젝트 기본 보상 데이터를 준비한다. 클라이언트는 지급 결과만 전달받는다.
void UPlayerRewardComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	PlayerKillRewardDefinition = TSoftObjectPtr<URewardDefinition>(URewardDefinition::GetDefaultRewardDefinitionPath());
	LoadedPlayerKillRewardDefinition = PlayerKillRewardDefinition.Get();
	if (LoadedPlayerKillRewardDefinition)
	{
		return;
	}

	if (PlayerKillRewardDefinition.IsNull())
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Player kill rewards require a Reward definition in the project bootstrap asset."));
		return;
	}

	PlayerKillRewardLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		PlayerKillRewardDefinition.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::HandlePlayerKillRewardLoaded),
		FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (PlayerKillRewardLoadHandle)
	{
		PlayerKillRewardLoadHandle->StartStalledHandle();
	}
	else
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Failed to request player kill reward data: %s."), *PlayerKillRewardDefinition.ToString());
	}
}

// 플레이어가 나가거나 게임피처가 해제되면 미완료 보상 로딩과 대기 중인 처치 보상을 정리한다.
void UPlayerRewardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerKillRewardLoadHandle)
	{
		PlayerKillRewardLoadHandle->CancelHandle();
		PlayerKillRewardLoadHandle.Reset();
	}
	PendingPlayerKillRewards = 0;
	LoadedPlayerKillRewardDefinition = nullptr;
	for (const auto& Entry : MonsterRewardLoadHandles)
	{
		Entry.Value->CancelHandle();
	}
	MonsterRewardLoadHandles.Reset();
	PendingMonsterRewards.Reset();
	Super::EndPlay(EndPlayReason);
}

// 서버가 확정한 다른 플레이어 처치에 보상을 지급하고, 데이터 로딩 중에는 지급 요청을 보관한다.
void UPlayerRewardComponent::GrantKillExperience(APlayerState* VictimPlayerState)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !IsValid(VictimPlayerState)
		|| VictimPlayerState == OwnerActor || VictimPlayerState->GetWorld() != OwnerActor->GetWorld())
	{
		return;
	}

	if (LoadedPlayerKillRewardDefinition)
	{
		GrantPlayerKillReward();
	}
	else if (PlayerKillRewardLoadHandle)
	{
		++PendingPlayerKillRewards;
	}
	else
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Cannot grant player kill reward: reward data is unavailable for %s."), *GetNameSafe(OwnerActor));
	}
}

// 보상 데이터가 준비되면 로딩 중 발생한 처치마다 원래 보상 규칙을 적용한다.
void UPlayerRewardComponent::HandlePlayerKillRewardLoaded()
{
	LoadedPlayerKillRewardDefinition = PlayerKillRewardDefinition.Get();
	PlayerKillRewardLoadHandle.Reset();
	const int32 RewardsToGrant = PendingPlayerKillRewards;
	PendingPlayerKillRewards = 0;
	if (!LoadedPlayerKillRewardDefinition)
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Failed to load player kill reward data %s; pending rewards: %d."),
			*PlayerKillRewardDefinition.ToString(), RewardsToGrant);
		return;
	}

	for (int32 RewardIndex = 0; RewardIndex < RewardsToGrant; ++RewardIndex)
	{
		GrantPlayerKillReward();
	}
}

// 처치 경험치를 추첨해 LevelingComponent로 지급하고, 성공한 보상만 획득 알림으로 보낸다.
void UPlayerRewardComponent::GrantPlayerKillReward()
{
	APdPlayerState* PlayerState = GetPdPlayerState();
	ULevelingComponent* LevelingComponent = PlayerState ? PlayerState->GetLevelingComponent() : nullptr;
	const int32 ExperienceReward = LoadedPlayerKillRewardDefinition->RollPlayerKillExperienceReward();
	if (!LevelingComponent || !LevelingComponent->GrantRewardExperience(ExperienceReward))
	{
		return;
	}

	if (APdPlayerController* Controller = Cast<APdPlayerController>(PlayerState->GetPlayerController()))
	{
		if (UPlayerNotificationComponent* NotificationComponent = Controller->GetPlayerNotificationComponent())
		{
			NotificationComponent->SendExperienceRewardNotification(ExperienceReward, LoadedPlayerKillRewardDefinition->Notification.ExperienceIcon);
		}
	}
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

		SkinComponent->AddSkinsByPrimaryAssetIds(RewardSkinDefinitions);
	}

	if (UPandoraComponent* PandoraComponent = PlayerState->GetPandoraComponent())
	{

		PandoraComponent->ActivatePandoras(RewardPandoraDefinitions);
	}

	if (APdPlayerController* Controller = Cast<APdPlayerController>(PlayerState->GetPlayerController()))
	{
		if (UPlayerNotificationComponent* NotificationComponent = Controller->GetPlayerNotificationComponent())
		{
			NotificationComponent->SendRewardNotifications(RewardItemDefinitions, RewardSkinDefinitions, RewardPandoraDefinitions);
		}
	}
	IInteractableInterface::Execute_OnRewardsClaimed(InteractableActor, PlayerState->GetPawn());

	return true;
}

APdPlayerState* UPlayerRewardComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}

// 몬스터가 확정한 처치 보상을 플레이어 수명에 보관하고, 같은 정의의 로딩은 공유한다.
void UPlayerRewardComponent::GrantMonsterDefeatRewards(TSoftObjectPtr<URewardDefinition> RewardDefinition)
{
	const APdPlayerState* PlayerState = GetPdPlayerState();
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return;
	}
	if (RewardDefinition.IsNull())
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Monster defeat reward has no definition for %s."), *GetNameSafe(PlayerState));
		return;
	}
	if (const URewardDefinition* LoadedDefinition = RewardDefinition.Get())
	{
		ApplyMonsterDefeatRewards(LoadedDefinition);
		return;
	}
	const FSoftObjectPath Path = RewardDefinition.ToSoftObjectPath();
	++PendingMonsterRewards.FindOrAdd(Path);
	if (MonsterRewardLoadHandles.Contains(Path))
	{
		return;
	}
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		Path, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleMonsterRewardLoaded, Path),
		FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (Handle)
	{
		MonsterRewardLoadHandles.Add(Path, Handle);
		Handle->StartStalledHandle();
	}
	else
	{
		HandleMonsterRewardLoaded(Path);
	}
}

// 로딩 중 죽은 몬스터마다 한 번씩 보상을 지급한다. 몬스터 Actor를 다시 조회하지 않는다.
void UPlayerRewardComponent::HandleMonsterRewardLoaded(FSoftObjectPath DefinitionPath)
{
	int32 RewardCount = 0;
	PendingMonsterRewards.RemoveAndCopyValue(DefinitionPath, RewardCount);
	TSharedPtr<FStreamableHandle> KeepAlive;
	MonsterRewardLoadHandles.RemoveAndCopyValue(DefinitionPath, KeepAlive);
	const URewardDefinition* Definition = Cast<URewardDefinition>(DefinitionPath.ResolveObject());
	if (!Definition)
	{
		UE_LOG(LogPlayerReward, Error, TEXT("Could not load monster reward '%s'; discarded requests: %d."),
			*DefinitionPath.ToString(), RewardCount);
		return;
	}
	for (int32 Index = 0; Index < RewardCount; ++Index)
	{
		ApplyMonsterDefeatRewards(Definition);
	}
}

// 경험치·소울더스트는 성공한 지급만 알리고, 포션은 인벤토리 추가 완료 후 알린다.
void UPlayerRewardComponent::ApplyMonsterDefeatRewards(const URewardDefinition* RewardDefinition)
{
	APdPlayerState* PlayerState = GetPdPlayerState();
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority() || !RewardDefinition)
	{
		return;
	}
	int32 Experience = RewardDefinition->RollMonsterDefeatExperienceReward();
	ULevelingComponent* Leveling = PlayerState->GetLevelingComponent();
	if (Experience > 0 && (!Leveling || !Leveling->GrantRewardExperience(Experience)))
	{
		Experience = 0;
	}
	int32 SoulDust = RewardDefinition->RollMonsterDefeatSoulDustReward();
	UPandoraTreeComponent* PandoraTree = PlayerState->GetPandoraTreeComponent();
	if (SoulDust > 0 && (!PandoraTree || !PandoraTree->AddSoulDust(SoulDust)))
	{
		SoulDust = 0;
	}
	APdPlayerController* Controller = Cast<APdPlayerController>(PlayerState->GetPlayerController());
	UPlayerNotificationComponent* Notification = Controller ? Controller->GetPlayerNotificationComponent() : nullptr;
	if (Notification)
	{
		if (Experience > 0)
		{
			Notification->SendExperienceRewardNotification(Experience, RewardDefinition->Notification.ExperienceIcon);
		}
		if (SoulDust > 0)
		{
			Notification->SendSoulDustRewardNotification(SoulDust, RewardDefinition->Notification.SoulDustIcon);
		}
	}
	const FPrimaryAssetId PotionId = RewardDefinition->RollMonsterDefeatPotionReward();
	UInventoryComponent* Inventory = PlayerState->GetInventoryComponent();
	if (PotionId.IsValid() && Inventory)
	{
		Inventory->AddItemsByPrimaryAssetIdsWithCompletion({PotionId},
			FOnPdItemsAdded::CreateWeakLambda(this, [this](const TArray<FPrimaryAssetId>& AddedItems)
			{
				APdPlayerState* CurrentPlayerState = GetPdPlayerState();
				APdPlayerController* CurrentController = CurrentPlayerState
					? Cast<APdPlayerController>(CurrentPlayerState->GetPlayerController()) : nullptr;
				UPlayerNotificationComponent* CurrentNotification = CurrentController ? CurrentController->GetPlayerNotificationComponent() : nullptr;
				if (CurrentNotification && !AddedItems.IsEmpty())
				{
					CurrentNotification->SendRewardNotifications(AddedItems, {}, {});
				}
			}));
	}
}
