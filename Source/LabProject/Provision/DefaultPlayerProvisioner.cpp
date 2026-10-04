#include "Provision/DefaultPlayerProvisioner.h"

#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Engine/GameInstance.h"
#include "Data/ContentDataSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultPlayerProvisioner)

DEFINE_LOG_CATEGORY_STATIC(LogDefaultPlayerProvisioner, Log, All);

namespace
{
	const FPrimaryAssetType ItemDefinitionType(TEXT("ItemDefinition"));

	bool HasItemPolicyGrant(const UDefaultProvisionDefinition& Definition, const EDefaultProvisionMode Mode)
	{
		return Definition.GetGrantAllItems().IsEnabled(Mode)
			|| Definition.GetGrantAllWeapons().IsEnabled(Mode)
			|| Definition.GetGrantAllEquipment().IsEnabled(Mode);
	}

	bool HasAnyItemGrant(const UDefaultProvisionDefinition& Definition, const EDefaultProvisionMode Mode)
	{
		const bool bHasExplicitGrant = Definition.GetItemGrants().ContainsByPredicate(
			[Mode](const FDefaultProvisionItemStackGrant& Grant)
			{
				return Grant.ItemDefinitionId.IsValid() && Grant.Counts.GetCount(Mode) > 0;
			});
		return bHasExplicitGrant || HasItemPolicyGrant(Definition, Mode);
	}

	// 지급에 필요한 정의 애셋을 모은다. "전체 지급" 정책이 켜져 있는데 아이템 목록이 비어 있으면 false다.
	bool CollectRequiredContent(
		const UDefaultProvisionDefinition& Definition, const EDefaultProvisionMode Mode, TArray<FPrimaryAssetId>& OutContentIds)
	{
		for (const FDefaultProvisionItemStackGrant& Grant : Definition.GetItemGrants())
		{
			if (Grant.ItemDefinitionId.IsValid() && Grant.Counts.GetCount(Mode) > 0)
			{
				OutContentIds.AddUnique(Grant.ItemDefinitionId);
			}
		}
		for (const FDefaultProvisionPandoraGrant& Grant : Definition.GetPandoraGrants())
		{
			if (Grant.PandoraDefinitionId.IsValid() && Grant.Levels.GetLevel(Mode) >= 0)
			{
				OutContentIds.AddUnique(Grant.PandoraDefinitionId);
			}
		}
		for (const FDefaultProvisionGestureSlotGrant& Grant : Definition.GetGestureGrants())
		{
			if (Grant.SkinDefinitionId.IsValid() && LabGameplayTags::GetGestureSlotTag(Grant.GestureSlotIndex).IsValid())
			{
				OutContentIds.AddUnique(Grant.SkinDefinitionId);
			}
		}
		if (!HasItemPolicyGrant(Definition, Mode))
		{
			return true;
		}

		TArray<FPrimaryAssetId> ItemDefinitionIds;
		UAssetManager::Get().GetPrimaryAssetIdList(ItemDefinitionType, ItemDefinitionIds);
		for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
		{
			OutContentIds.AddUnique(ItemDefinitionId);
		}
		return !ItemDefinitionIds.IsEmpty();
	}

	const UItemDefinition* GetItemDefinition(const UItemInstance* ItemInstance)
	{
		return IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
	}

	int32 GetInventoryItemQuantity(const UInventoryComponent& Inventory, const FPrimaryAssetId& ItemDefinitionId)
	{
		int32 TotalQuantity = 0;
		for (const UItemInstance* ItemInstance : Inventory.GetAllItems().Items)
		{
			const UItemDefinition* ItemDefinition = GetItemDefinition(ItemInstance);
			if (ItemDefinition && ItemDefinition->GetPrimaryAssetId() == ItemDefinitionId)
			{
				TotalQuantity += FMath::Max(ItemInstance->Quantity, 0);
			}
		}
		return TotalQuantity;
	}

	// 명시 지급의 수량과 퀵슬롯을 정의대로 맞춘다. 인벤토리에 요청을 하나라도 보냈으면 true다.
	bool ApplyExplicitItemGrants(
		const UDefaultProvisionDefinition& Definition,
		const EDefaultProvisionMode Mode,
		UInventoryComponent& Inventory,
		TSet<FPrimaryAssetId>& OutGrantedIds)
	{
		bool bIssuedRequest = false;
		for (const FDefaultProvisionItemStackGrant& Grant : Definition.GetItemGrants())
		{
			const int32 Quantity = Grant.Counts.GetCount(Mode);
			if (!Grant.ItemDefinitionId.IsValid() || Quantity <= 0)
			{
				continue;
			}
			OutGrantedIds.Add(Grant.ItemDefinitionId);

			const int32 CurrentQuantity = GetInventoryItemQuantity(Inventory, Grant.ItemDefinitionId);
			const bool bHasQuickSlot =
				Grant.QuickSlotIndex >= 0 && Grant.QuickSlotIndex < UInventoryComponent::ConsumableQuickSlotCount;
			if (!bHasQuickSlot)
			{
				if (CurrentQuantity != Quantity)
				{
					Inventory.SetItemQuantityByPrimaryAssetId(Grant.ItemDefinitionId, Quantity);
					bIssuedRequest = true;
				}
				continue;
			}

			const UItemDefinition* QuickSlotDefinition =
				GetItemDefinition(Inventory.GetConsumableQuickSlotItem(Grant.QuickSlotIndex));
			const bool bQuickSlotMatches =
				QuickSlotDefinition && QuickSlotDefinition->GetPrimaryAssetId() == Grant.ItemDefinitionId;
			if (CurrentQuantity != Quantity || !bQuickSlotMatches)
			{
				Inventory.SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
					Grant.ItemDefinitionId, Quantity, Grant.QuickSlotIndex);
				bIssuedRequest = true;
			}
		}
		return bIssuedRequest;
	}

	// "전체 아이템·무기·장비 지급" 정책으로 더 줄 아이템을 고른다. 명시 지급과 이미 가진 아이템은 뺀다.
	// 정책이 켜져 있는데 아이템 목록이 비어 있으면 false다.
	bool CollectPolicyItemIds(
		const UDefaultProvisionDefinition& Definition,
		const EDefaultProvisionMode Mode,
		const UInventoryComponent& Inventory,
		const TSet<FPrimaryAssetId>& ExplicitGrantIds,
		TArray<FPrimaryAssetId>& OutItemIds)
	{
		const bool bGrantAllItems = Definition.GetGrantAllItems().IsEnabled(Mode);
		const bool bGrantAllWeapons = Definition.GetGrantAllWeapons().IsEnabled(Mode);
		const bool bGrantAllEquipment = Definition.GetGrantAllEquipment().IsEnabled(Mode);
		if (!bGrantAllItems && !bGrantAllWeapons && !bGrantAllEquipment)
		{
			return true;
		}

		TSet<FPrimaryAssetId> ExistingItemIds;
		for (const UItemInstance* ItemInstance : Inventory.GetAllItems().Items)
		{
			if (const UItemDefinition* ItemDefinition = GetItemDefinition(ItemInstance))
			{
				ExistingItemIds.Add(ItemDefinition->GetPrimaryAssetId());
			}
		}

		const UProjectTagDefinition* TagConfig = UProjectTagDefinition::GetDefaultDefinition();
		const FGameplayTag WeaponTypeTag = TagConfig ? TagConfig->GetItemWeaponTypeTag() : LabGameplayTags::Item_Weapon;
		const FGameplayTag EquipmentTypeTag = TagConfig ? TagConfig->GetItemEquipmentTypeTag() : LabGameplayTags::Item_Equipment;

		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> AllItemDefinitionIds;
		AssetManager.GetPrimaryAssetIdList(ItemDefinitionType, AllItemDefinitionIds);
		if (AllItemDefinitionIds.IsEmpty())
		{
			return false;
		}

		for (const FPrimaryAssetId& ItemDefinitionId : AllItemDefinitionIds)
		{
			if (!ItemDefinitionId.IsValid() || ExplicitGrantIds.Contains(ItemDefinitionId) || ExistingItemIds.Contains(ItemDefinitionId))
			{
				continue;
			}

			const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(AssetManager.GetPrimaryAssetObject(ItemDefinitionId));
			if (!ItemDefinition)
			{
				continue;
			}

			const bool bMatchesWeapon = bGrantAllWeapons && ItemDefinition->IsWeaponDefinition(WeaponTypeTag);
			const bool bMatchesEquipment = bGrantAllEquipment && ItemDefinition->MatchesItemType(EquipmentTypeTag);
			if (bGrantAllItems || bMatchesWeapon || bMatchesEquipment)
			{
				OutItemIds.Add(ItemDefinitionId);
			}
		}
		return true;
	}

	// 이 모드에서 열 판도라와 레벨을 고른다. 레벨 0은 열기만 하고 판도라 트리 레벨은 주지 않는다.
	void CollectPandoraGrants(
		const UDefaultProvisionDefinition& Definition,
		const EDefaultProvisionMode Mode,
		TArray<FPrimaryAssetId>& OutUnlockedIds,
		TArray<FGrantedPandora>& OutGrantedPandoras)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		for (const FDefaultProvisionPandoraGrant& Grant : Definition.GetPandoraGrants())
		{
			const int32 Level = Grant.Levels.GetLevel(Mode);
			if (!Grant.PandoraDefinitionId.IsValid() || Level < 0)
			{
				continue;
			}

			UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(AssetManager.GetPrimaryAssetObject(Grant.PandoraDefinitionId));
			if (!PandoraDefinition)
			{
				continue;
			}

			OutUnlockedIds.AddUnique(PandoraDefinition->GetPrimaryAssetId());
			if (Level > 0)
			{
				OutGrantedPandoras.Add(FGrantedPandora(PandoraDefinition, Level));
			}
		}
	}

	// 로비에서 고른 로드아웃 슬롯을 PlayerState가 심리스 이동으로 넘겨받은 값에서 읽는다. 이번에 연 판도라만 남긴다.
	TMap<EEnum_Direction, FPrimaryAssetId> ResolveLobbyLoadout(
		const APdPlayerState& PlayerState, const TArray<FPrimaryAssetId>& UnlockedPandoraIds)
	{
		TMap<EEnum_Direction, FPrimaryAssetId> LoadoutByDirection;
		const UContentDataSubsystem* ContentDataSubsystem =
			UGameInstance::GetSubsystem<UContentDataSubsystem>(PlayerState.GetGameInstance());
		if (!ContentDataSubsystem)
		{
			return LoadoutByDirection;
		}

		for (const TPair<EEnum_Direction, FName>& Pair : PlayerState.GetLobbyTravelHandoff().PandoraNamesByDirection)
		{
			if (!PandoraLoadout::IsLoadoutDirection(Pair.Key) || Pair.Value.IsNone())
			{
				continue;
			}
			const UPandoraDefinition* PandoraDefinition = ContentDataSubsystem->GetPandoraDefinitionByName(Pair.Value);
			if (PandoraDefinition && UnlockedPandoraIds.Contains(PandoraDefinition->GetPrimaryAssetId()))
			{
				LoadoutByDirection.Add(Pair.Key, PandoraDefinition->GetPrimaryAssetId());
			}
		}
		return LoadoutByDirection;
	}
}

UWorld* UDefaultPlayerProvisioner::GetWorld() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool UDefaultPlayerProvisioner::Initialize(const UDefaultProvisionDefinition* InDefinition, const EDefaultProvisionMode InMode)
{
	if (!InDefinition || bShuttingDown)
	{
		return false;
	}
	if (ProvisionDefinition)
	{
		// 다른 맵이나 모드는 새 지급 처리 객체를 만든다. 진행 중인 지급의 설정은 바꿀 수 없다.
		if (ProvisionDefinition != InDefinition || Mode != InMode)
		{
			UE_LOG(LogDefaultPlayerProvisioner, Error,
				TEXT("Provision definition and mode are fixed for this runtime; create a new provisioner for a different configuration."));
			return false;
		}
		return true;
	}
	ProvisionDefinition = InDefinition;
	Mode = InMode;
	return true;
}

const UDefaultProvisionDefinition* UDefaultPlayerProvisioner::GetDefinition() const
{
	return IsInitialized() ? ProvisionDefinition.Get() : nullptr;
}

void UDefaultPlayerProvisioner::ProvisionPlayer(APlayerController* PlayerController)
{
	if (!IsInitialized() || !IsValid(PlayerController) || !PlayerController->HasAuthority())
	{
		return;
	}

	if (!EnsureContentLoaded(PlayerController))
	{
		return;
	}

	const EProvisionStepResult Result = TryProvisionPlayer(PlayerController);
	if (Result == EProvisionStepResult::Done)
	{
		ClearScheduledProvisionAttempt(PlayerController);
		StopWaitingForProvisionInputs(TObjectKey<APlayerController>(PlayerController));
		OnPlayerProvisioned.Broadcast(PlayerController);
		return;
	}

	WaitForProvisionInputs(PlayerController, Result);
}

bool UDefaultPlayerProvisioner::EnsureContentLoaded(APlayerController* PlayerController)
{
	if (ContentState == EContentState::Ready)
	{
		return true;
	}
	if (ContentState == EContentState::Failed)
	{
		return false;
	}
	PendingContentControllers.AddUnique(PlayerController);
	if (ContentState == EContentState::Loading)
	{
		return false;
	}

	const UDefaultProvisionDefinition* Definition = GetDefinition();
	check(Definition);
	if (!CollectRequiredContent(*Definition, Mode, RequiredContentIds))
	{
		ContentState = EContentState::Failed;
		PendingContentControllers.Reset();
		UE_LOG(LogDefaultPlayerProvisioner, Error, TEXT("Default inventory policy requires an ItemDefinition catalog, but it is empty."));
		return false;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const bool bAllLoaded = !RequiredContentIds.ContainsByPredicate(
		[&AssetManager](const FPrimaryAssetId& AssetId) { return !AssetManager.GetPrimaryAssetObject(AssetId); });
	if (bAllLoaded)
	{
		ContentState = EContentState::Ready;
		PendingContentControllers.Reset();
		return true;
	}

	// 이 플레이 공간의 플레이어는 모두 하나의 고정된 콘텐츠 요청과 그 수명을 함께 쓴다.
	ContentState = EContentState::Loading;
	ContentLoadHandle = AssetManager.LoadPrimaryAssets(RequiredContentIds, {},
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleContentLoaded));
	return false;
}

void UDefaultPlayerProvisioner::HandleContentLoaded()
{
	if (bShuttingDown || ContentState != EContentState::Loading)
	{
		return;
	}
	for (const FPrimaryAssetId& Id : RequiredContentIds)
	{
		if (!UAssetManager::Get().GetPrimaryAssetObject(Id))
		{
			ContentState = EContentState::Failed;
			PendingContentControllers.Reset();
			UE_LOG(LogDefaultPlayerProvisioner, Error,
				TEXT("Required provisioning content failed to load: %s. Default grants were not applied."), *Id.ToString());
			return;
		}
	}
	ContentState = EContentState::Ready;
	const TArray<TWeakObjectPtr<APlayerController>> WaitingPlayers = PendingContentControllers;
	for (const TWeakObjectPtr<APlayerController>& Player : WaitingPlayers)
	{
		// 앞선 플레이어의 지급 콜백이 다른 대기 플레이어를 로그아웃시키거나 이 지급 처리 객체를 종료할 수 있다.
		if (PendingContentControllers.Remove(Player) > 0)
		{
			ProvisionPlayer(Player.Get());
		}
	}
}

UDefaultPlayerProvisioner::EProvisionStepResult UDefaultPlayerProvisioner::TryProvisionPlayer(APlayerController* PlayerController)
{
	APdPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APdPlayerState>() : nullptr;
	if (!PlayerState || !PlayerState->HasAuthority() || !GetDefinition())
	{
		return EProvisionStepResult::Failed;
	}

	// 단계는 서로를 기다리지 않으므로 모두 실행한다. 하나라도 끝나지 않았으면 그중 가장 나쁜 결과를 돌려준다.
	const EProvisionStepResult Results[] = {
		ApplyModeValues(PlayerState),
		ApplyItems(PlayerState),
		ApplyPandoras(PlayerState),
		ApplyGestures(PlayerController, PlayerState),
	};
	PlayerState->ForceNetUpdate();

	EProvisionStepResult Combined = EProvisionStepResult::Done;
	for (const EProvisionStepResult Result : Results)
	{
		if (Result == EProvisionStepResult::Failed)
		{
			return EProvisionStepResult::Failed;
		}
		if (Result == EProvisionStepResult::Waiting)
		{
			Combined = EProvisionStepResult::Waiting;
		}
	}
	return Combined;
}

// 능력치 포인트와 소울 더스트를 모드 값으로 한 번 맞춘다.
UDefaultPlayerProvisioner::EProvisionStepResult UDefaultPlayerProvisioner::ApplyModeValues(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState || !PlayerState->HasAuthority())
	{
		return EProvisionStepResult::Failed;
	}
	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	if (InitializedModeValues.Contains(PlayerStateKey))
	{
		return EProvisionStepResult::Done;
	}

	const float StatusPointValue = Definition->GetStatusPointValues().GetValue(Mode);
	if (UStatUpgradeComponent* StatUpgradeComponent = PlayerState->GetStatUpgradeComponent())
	{
		if (!StatUpgradeComponent->SetPointsForAllCategories(StatusPointValue))
		{
			// 능력치 정의가 아직 로드되지 않았으면 준비 알림을 받아 다시 시도한다.
			return StatUpgradeComponent->IsDefinitionReady() ? EProvisionStepResult::Failed : EProvisionStepResult::Waiting;
		}
	}
	else if (StatusPointValue > 0.0f)
	{
		return EProvisionStepResult::Failed;
	}

	const int32 SoulDustValue = Definition->GetSoulDustValues().GetCount(Mode);
	if (UPandoraTreeComponent* PandoraTreeComponent = PlayerState->GetPandoraTreeComponent())
	{
		PandoraTreeComponent->SetSoulDust(SoulDustValue);
	}
	else if (SoulDustValue > 0)
	{
		return EProvisionStepResult::Failed;
	}
	InitializedModeValues.Add(PlayerStateKey);
	return EProvisionStepResult::Done;
}

// 명시 지급과 "전체 지급" 정책을 인벤토리에 맞춘다. 로비가 아니면 처음 지급할 때 인벤토리를 비운다.
UDefaultPlayerProvisioner::EProvisionStepResult UDefaultPlayerProvisioner::ApplyItems(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState)
	{
		return EProvisionStepResult::Failed;
	}
	// 로비는 지급할 아이템이 없으면 플레이어가 가진 인벤토리를 건드리지 않는다.
	if (Mode == EDefaultProvisionMode::Lobby && !HasAnyItemGrant(*Definition, Mode))
	{
		return EProvisionStepResult::Done;
	}

	UInventoryComponent* InventoryComponent = PlayerState->GetInventoryComponent();
	if (!InventoryComponent)
	{
		return EProvisionStepResult::Failed;
	}

	FInventoryProvisionState& State = InventoryStates.FindOrAdd(TObjectKey<APlayerState>(PlayerState));
	if (State.Inventory.Get() == InventoryComponent && State.bCompleted)
	{
		return EProvisionStepResult::Done;
	}
	// 인벤토리는 비동기로 바뀐다. 로그아웃으로 지급 기록이 지워진 뒤라도, 끝나지 않은 변경이 있으면 맞추지 않는다.
	if (InventoryComponent->HasPendingItemLoads())
	{
		return EProvisionStepResult::Waiting;
	}
	if (State.Inventory.Get() != InventoryComponent)
	{
		State.Inventory = InventoryComponent;
		State.bCompleted = false;
		if (Mode != EDefaultProvisionMode::Lobby)
		{
			InventoryComponent->ClearAllItems();
		}
	}

	TSet<FPrimaryAssetId> ExplicitGrantIds;
	bool bIssuedRequest = ApplyExplicitItemGrants(*Definition, Mode, *InventoryComponent, ExplicitGrantIds);

	TArray<FPrimaryAssetId> PolicyItemIds;
	if (!CollectPolicyItemIds(*Definition, Mode, *InventoryComponent, ExplicitGrantIds, PolicyItemIds))
	{
		return EProvisionStepResult::Failed;
	}
	if (!PolicyItemIds.IsEmpty())
	{
		InventoryComponent->AddItemsByPrimaryAssetIds(PolicyItemIds);
		bIssuedRequest = true;
	}

	if (bIssuedRequest || InventoryComponent->HasPendingItemLoads())
	{
		return EProvisionStepResult::Waiting;
	}
	// 기본 지급을 마친 뒤에는 사용·강화로 줄어든 아이템을 최소 보유량으로 되돌리지 않는다.
	State.bCompleted = true;
	return EProvisionStepResult::Done;
}

// 판도라를 열고 판도라 트리 레벨과 소울 더스트를 정한다. 경기에서는 로비에서 고른 로드아웃 슬롯을 이어받는다.
UDefaultPlayerProvisioner::EProvisionStepResult UDefaultPlayerProvisioner::ApplyPandoras(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return EProvisionStepResult::Failed;
	}
	if (Mode == EDefaultProvisionMode::Lobby && !Definition->HasPandoraGrants(Mode))
	{
		return EProvisionStepResult::Done;
	}

	UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	UPandoraTreeComponent* PandoraTreeComponent = PlayerState ? PlayerState->GetPandoraTreeComponent() : nullptr;
	if (!PandoraComponent || !PandoraTreeComponent)
	{
		return EProvisionStepResult::Failed;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	const FInitializedPandoraState* InitializedState = InitializedPandoras.Find(PlayerStateKey);
	if (InitializedState
		&& InitializedState->PandoraComponent.Get() == PandoraComponent
		&& InitializedState->PandoraTreeComponent.Get() == PandoraTreeComponent)
	{
		return EProvisionStepResult::Done;
	}

	TArray<FPrimaryAssetId> UnlockedPandoraIds;
	TArray<FGrantedPandora> GrantedPandoras;
	CollectPandoraGrants(*Definition, Mode, UnlockedPandoraIds, GrantedPandoras);

	PandoraComponent->ClearAllPandoras();
	PandoraTreeComponent->InitializeFromDefaultProvision(GrantedPandoras, Definition->GetSoulDustValues().GetCount(Mode));
	const TMap<EEnum_Direction, FPrimaryAssetId> LoadoutByDirection = Mode == EDefaultProvisionMode::Gameplay
		? ResolveLobbyLoadout(*PlayerState, UnlockedPandoraIds)
		: TMap<EEnum_Direction, FPrimaryAssetId>();
	PandoraComponent->GrantPandorasWithLoadout(UnlockedPandoraIds, LoadoutByDirection);

	FInitializedPandoraState& NewState = InitializedPandoras.FindOrAdd(PlayerStateKey);
	NewState.PandoraComponent = PandoraComponent;
	NewState.PandoraTreeComponent = PandoraTreeComponent;
	return EProvisionStepResult::Done;
}

// 제스처 스킨을 주고 칸에 장착한다. 장착은 캐릭터에 붙은 컴포넌트가 하므로 Pawn을 기다린다.
UDefaultPlayerProvisioner::EProvisionStepResult UDefaultPlayerProvisioner::ApplyGestures(
	APlayerController* PlayerController, APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return EProvisionStepResult::Failed;
	}
	if (Definition->GetGestureGrants().IsEmpty())
	{
		return EProvisionStepResult::Done;
	}

	USkinComponent* SkinComponent = PlayerState ? PlayerState->GetSkinComponent() : nullptr;
	if (!SkinComponent)
	{
		return EProvisionStepResult::Failed;
	}
	ACharacterBase* Character = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	USkinEquipmentComponent* SkinEquipmentComponent = Character ? Character->GetSkinEquipmentComponent() : nullptr;
	if (!SkinEquipmentComponent)
	{
		return EProvisionStepResult::Waiting;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	const TWeakObjectPtr<USkinEquipmentComponent>* InitializedEquipment = InitializedGestureEquipment.Find(PlayerStateKey);
	if (InitializedEquipment && InitializedEquipment->Get() == SkinEquipmentComponent)
	{
		return EProvisionStepResult::Done;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FDefaultProvisionGestureSlotGrant& Grant : Definition->GetGestureGrants())
	{
		const FGameplayTag SlotTag = LabGameplayTags::GetGestureSlotTag(Grant.GestureSlotIndex);
		USkinDefinition* SkinDefinition = Grant.SkinDefinitionId.IsValid() && SlotTag.IsValid()
			? Cast<USkinDefinition>(AssetManager.GetPrimaryAssetObject(Grant.SkinDefinitionId))
			: nullptr;
		if (!SkinDefinition)
		{
			continue;
		}

		SkinComponent->AddSkinDefinitions({ SkinDefinition });
		SkinEquipmentComponent->RequestEquipSkinDefinition(SkinDefinition, SlotTag);
	}
	InitializedGestureEquipment.Add(PlayerStateKey, SkinEquipmentComponent);
	return EProvisionStepResult::Done;
}

// 인벤토리 아이템 로딩, 능력치 정의 준비, 제스처를 받을 캐릭터 빙의를 완료 알림으로 기다렸다가 다시 지급한다.
// 기다릴 대상이 없는데도 끝나지 않았으면 구성 요소나 정의가 빠진 것이라 다시 시도하지 않고 오류로 알린다.
void UDefaultPlayerProvisioner::WaitForProvisionInputs(APlayerController* PlayerController, const EProvisionStepResult Result)
{
	if (bShuttingDown || !PlayerController)
	{
		return;
	}

	APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>();
	UInventoryComponent* InventoryComponent = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	UStatUpgradeComponent* StatUpgradeComponent = PlayerState ? PlayerState->GetStatUpgradeComponent() : nullptr;
	const bool bWaitForInventory = InventoryComponent && InventoryComponent->HasPendingItemLoads();
	const bool bWaitForStatUpgrade = StatUpgradeComponent && !StatUpgradeComponent->IsDefinitionReady();
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	const ACharacterBase* Character = Cast<ACharacterBase>(PlayerController->GetPawn());
	const bool bWaitForPawn = Definition && !Definition->GetGestureGrants().IsEmpty()
		&& (!Character || !Character->GetSkinEquipmentComponent());
	if (!bWaitForInventory && !bWaitForStatUpgrade && !bWaitForPawn)
	{
		UE_LOG(LogDefaultPlayerProvisioner, Error,
			TEXT("Default provisioning for %s %s with nothing left to wait for. Check the player state components and the provision definition."),
			*GetNameSafe(PlayerController),
			Result == EProvisionStepResult::Failed ? TEXT("is missing a component or definition") : TEXT("did not finish"));
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	const TWeakObjectPtr<APlayerController> WeakPlayer(PlayerController);
	const auto Resume = [this, WeakPlayer, ControllerKey]()
	{
		StopWaitingForProvisionInputs(ControllerKey);
		if (APlayerController* Player = WeakPlayer.Get())
		{
			ScheduleProvisionAttempt(Player);
		}
	};

	FProvisionWait& Wait = ProvisionWaits.FindOrAdd(ControllerKey);
	if (bWaitForInventory && !(Wait.Inventory.Get() == InventoryComponent && Wait.InventoryHandle.IsValid()))
	{
		if (UInventoryComponent* PreviousInventory = Wait.Inventory.Get())
		{
			PreviousInventory->OnItemLoadsFinished().Remove(Wait.InventoryHandle);
		}
		Wait.Inventory = InventoryComponent;
		Wait.InventoryHandle = InventoryComponent->OnItemLoadsFinished().AddWeakLambda(this, Resume);
	}
	if (bWaitForStatUpgrade && !(Wait.StatUpgrade.Get() == StatUpgradeComponent && Wait.StatUpgradeHandle.IsValid()))
	{
		if (UStatUpgradeComponent* PreviousStatUpgrade = Wait.StatUpgrade.Get())
		{
			PreviousStatUpgrade->OnDefinitionReady().Remove(Wait.StatUpgradeHandle);
		}
		Wait.StatUpgrade = StatUpgradeComponent;
		Wait.StatUpgradeHandle = StatUpgradeComponent->OnDefinitionReady().AddWeakLambda(this, Resume);
	}
	if (bWaitForPawn && !(Wait.Controller.Get() == PlayerController && Wait.PawnHandle.IsValid()))
	{
		Wait.Controller = PlayerController;
		Wait.PawnHandle = PlayerController->GetOnNewPawnNotifier().AddWeakLambda(this, [Resume](APawn*) { Resume(); });
	}
}

void UDefaultPlayerProvisioner::StopWaitingForProvisionInputs(const TObjectKey<APlayerController> ControllerKey)
{
	FProvisionWait Wait;
	if (!ProvisionWaits.RemoveAndCopyValue(ControllerKey, Wait))
	{
		return;
	}

	if (UInventoryComponent* InventoryComponent = Wait.Inventory.Get())
	{
		InventoryComponent->OnItemLoadsFinished().Remove(Wait.InventoryHandle);
	}
	if (UStatUpgradeComponent* StatUpgradeComponent = Wait.StatUpgrade.Get())
	{
		StatUpgradeComponent->OnDefinitionReady().Remove(Wait.StatUpgradeHandle);
	}
	if (APlayerController* PlayerController = Wait.Controller.Get())
	{
		PlayerController->GetOnNewPawnNotifier().Remove(Wait.PawnHandle);
	}
}

void UDefaultPlayerProvisioner::ScheduleProvisionAttempt(APlayerController* PlayerController)
{
	UWorld* World = GetWorld();
	if (bShuttingDown || !World || !PlayerController)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	const FTimerHandle* ExistingTimer = PendingProvisionAttempts.Find(ControllerKey);
	if (ExistingTimer && World->GetTimerManager().IsTimerActive(*ExistingTimer))
	{
		return;
	}

	const TWeakObjectPtr<APlayerController> WeakPlayer(PlayerController);
	const FTimerHandle TimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, WeakPlayer, ControllerKey]()
		{
			PendingProvisionAttempts.Remove(ControllerKey);
			ProvisionPlayer(WeakPlayer.Get());
		}));
	PendingProvisionAttempts.Add(ControllerKey, TimerHandle);
}

void UDefaultPlayerProvisioner::ClearScheduledProvisionAttempt(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	UWorld* World = GetWorld();
	if (FTimerHandle* TimerHandle = World ? PendingProvisionAttempts.Find(ControllerKey) : nullptr)
	{
		World->GetTimerManager().ClearTimer(*TimerHandle);
	}
	PendingProvisionAttempts.Remove(ControllerKey);
}

void UDefaultPlayerProvisioner::ClearRuntimeStateForController(AController* Controller, APlayerState* PlayerState)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		ClearScheduledProvisionAttempt(PlayerController);
		StopWaitingForProvisionInputs(TObjectKey<APlayerController>(PlayerController));
		PendingContentControllers.Remove(PlayerController);
	}
	if (!PlayerState)
	{
		return;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	InventoryStates.Remove(PlayerStateKey);
	InitializedModeValues.Remove(PlayerStateKey);
	InitializedPandoras.Remove(PlayerStateKey);
	InitializedGestureEquipment.Remove(PlayerStateKey);
}

void UDefaultPlayerProvisioner::Shutdown()
{
	bShuttingDown = true;
	if (UWorld* World = GetWorld())
	{
		for (TPair<TObjectKey<APlayerController>, FTimerHandle>& Pair : PendingProvisionAttempts)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	PendingProvisionAttempts.Reset();
	TArray<TObjectKey<APlayerController>> WaitingControllers;
	ProvisionWaits.GetKeys(WaitingControllers);
	for (const TObjectKey<APlayerController>& ControllerKey : WaitingControllers)
	{
		StopWaitingForProvisionInputs(ControllerKey);
	}
	PendingContentControllers.Reset();
	InventoryStates.Reset();
	InitializedModeValues.Reset();
	InitializedPandoras.Reset();
	InitializedGestureEquipment.Reset();

	if (ContentLoadHandle.IsValid())
	{
		ContentLoadHandle->CancelHandle();
		ContentLoadHandle->ReleaseHandle();
		ContentLoadHandle.Reset();
	}
	RequiredContentIds.Reset();
	ProvisionDefinition = nullptr;
	OnPlayerProvisioned.Clear();
}
