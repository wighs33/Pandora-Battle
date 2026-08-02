#include "Item/RewardChest.h"

#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Component/Item/InventoryComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/RewardDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RewardChest)

DEFINE_LOG_CATEGORY_STATIC(LogRewardChest, Log, All);

ARewardChest::ARewardChest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(true);

	RandomRewardItemCountChances =
	{
		FRewardChestItemCountChance(1, 40.0f),
		FRewardChestItemCountChance(2, 30.0f),
		FRewardChestItemCountChance(3, 20.0f),
		FRewardChestItemCountChance(4, 10.0f)
	};

	ChestMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ChestMesh"));
	SetRootComponent(ChestMesh);
	if (ChestMesh)
	{
		ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ChestMesh->SetCollisionObjectType(ECC_WorldDynamic);
		ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
		ChestMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		ChestMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		ChestMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap); // OverlapBox
		ChestMesh->SetGenerateOverlapEvents(true);
		ChestMesh->SetCanEverAffectNavigation(false);
		ChestMesh->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleChestBeginOverlap);
		ChestMesh->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleChestEndOverlap);
	}

	InteractionBillboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("InteractionBillboard"));
	if (InteractionBillboard)
	{
		InteractionBillboard->SetupAttachment(ChestMesh);
		InteractionBillboard->SetHiddenInGame(false);
		InteractionBillboard->SetVisibility(true);
	}

	InteractionTipWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionTipWidget"));
	if (InteractionTipWidget)
	{
		InteractionTipWidget->SetupAttachment(InteractionBillboard ? static_cast<USceneComponent*>(InteractionBillboard) : ChestMesh);
		InteractionTipWidget->SetHiddenInGame(true);
		InteractionTipWidget->SetVisibility(false);
		InteractionTipWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionTipWidget->SetDrawAtDesiredSize(true);
		InteractionTipWidget->SetWidgetSpace(EWidgetSpace::Screen);
	}

	OpenEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("OpenEffect"));
	if (OpenEffect)
	{
		OpenEffect->SetupAttachment(ChestMesh);
		OpenEffect->SetAutoActivate(false);
	}
}

void ARewardChest::BeginPlay()
{
	Super::BeginPlay();
	OriginalSpawnTransform = GetActorTransform();
	bOriginalSpawnTransformCaptured = true;

	ConfigureChestCollision(ChestState == ERewardChestState::Closed);
	bRewardContentReady = !HasAuthority();
	if (HasAuthority())
	{
		BeginRewardContentPreload();
	}
	ApplyChestState(nullptr);
}

void ARewardChest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseRewardContentPreload();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARewardChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARewardChest, ChestState, Params);
}

bool ARewardChest::CanInteract_Implementation(AActor* InteractingActor)
{
	return ChestState == ERewardChestState::Closed
		&& IsValid(InteractingActor)
		&& (!HasAuthority() || bRewardContentReady);
}

bool ARewardChest::Interact_Implementation(AActor* InteractingActor)
{



	// Reward application is owned by PlayerRewardComponent.
	return false;
}

FText ARewardChest::GetInteractText_Implementation(AActor* InteractingActor)
{
	return CanInteract_Implementation(InteractingActor)
		? NSLOCTEXT("RewardChest", "OpenChestInteractText", "Open")
		: FText::GetEmpty();
}

void ARewardChest::GetRewardItems_Implementation(TArray<FPrimaryAssetId>& OutItemDefinitionList)

{
	GetRewardItemsForInventory(nullptr, OutItemDefinitionList);
}

void ARewardChest::GetRewardItemsForInventory(
	const UInventoryComponent* InventoryComponent,
	TArray<FPrimaryAssetId>& OutItemDefinitionList)
{
	OutItemDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed
		|| (HasAuthority() && !bRewardContentReady))
	{

		return;
	}

	TSet<FPrimaryAssetId> ExcludedUniqueItemIds;
	if (InventoryComponent)
	{
		InventoryComponent->GetOwnedOrPendingItemDefinitionIds(
			ExcludedUniqueItemIds);
	}

	if (bUseItemDefinitionDropRates)
	{
		AppendRandomItemPrimaryAssetIds(
			ExcludedUniqueItemIds,
			OutItemDefinitionList);
	}
	else
	{
		AppendConfiguredItemPrimaryAssetIds(
			ExcludedUniqueItemIds,
			OutItemDefinitionList);
	}


}

void ARewardChest::GetRewardSkins_Implementation(TArray<FPrimaryAssetId>& OutSkinDefinitionList)
{
	OutSkinDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed
		|| (HasAuthority() && !bRewardContentReady))
	{

		return;
	}

	AppendPrimaryAssetIds(RewardSkins, OutSkinDefinitionList);

}

void ARewardChest::GetRewardPandoras_Implementation(TArray<FPrimaryAssetId>& OutPandoraDefinitionList)
{
	OutPandoraDefinitionList.Reset();
	if (ChestState != ERewardChestState::Closed
		|| (HasAuthority() && !bRewardContentReady))
	{

		return;
	}

	AppendPrimaryAssetIds(RewardPandoras, OutPandoraDefinitionList);

}

void ARewardChest::OnRewardsClaimed_Implementation(AActor* RewardReceiver)
{


	MarkOpened(RewardReceiver);
}

void ARewardChest::MarkOpened(AActor* RewardReceiver)
{
	if (!HasAuthority() || ChestState != ERewardChestState::Closed)
	{

		return;
	}



	PlayCharacterInteractionAnimation(RewardReceiver);
	SetChestState(ERewardChestState::Opening, RewardReceiver);
	ScheduleRespawnAfterOpen();
}

void ARewardChest::DeactivateForSpawnPool()
{
	if (!HasAuthority())
	{
		return;
	}

	SetChestState(ERewardChestState::Hidden, nullptr);
}

void ARewardChest::OnRep_ChestState()
{


	ApplyChestState(nullptr);
}

void ARewardChest::HandleChestBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	APdPlayer* Player = Cast<APdPlayer>(OtherActor);
	const bool bShouldShowTip = ChestState == ERewardChestState::Closed && Player && Player->IsLocallyControlled();


	if (bShouldShowTip)
	{
		SetInteractionTipVisible(true);
	}
}

void ARewardChest::HandleChestEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	static_cast<void>(OtherBodyIndex);

	APdPlayer* Player = Cast<APdPlayer>(OtherActor);
	const bool bShouldHideTip = ChestState == ERewardChestState::Closed && Player && Player->IsLocallyControlled();


	if (bShouldHideTip)
	{
		SetInteractionTipVisible(false);
	}
}

template <typename DefinitionType>
void ARewardChest::AppendPrimaryAssetIds(
	const TArray<TSoftObjectPtr<DefinitionType>>& SourceDefinitions,
	TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const
{
	for (const TSoftObjectPtr<DefinitionType>& SourceDefinition : SourceDefinitions)
	{
		if (SourceDefinition.IsNull())
		{
			continue;
		}

		const DefinitionType* LoadedDefinition = SourceDefinition.Get();
		if (!LoadedDefinition)
		{

			continue;
		}

		const FPrimaryAssetId PrimaryAssetId = LoadedDefinition->GetPrimaryAssetId();
		if (PrimaryAssetId.IsValid())
		{
			OutPrimaryAssetIds.Add(PrimaryAssetId);
		}
	}
}

void ARewardChest::AppendRandomItemPrimaryAssetIds(
	const TSet<FPrimaryAssetId>& ExcludedUniqueItemIds,
	TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const
{
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FPrimaryAssetId> AllItemDefinitionIds;
	AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("ItemDefinition")), AllItemDefinitionIds);

	TArray<FPrimaryAssetId> WeaponCandidateIds;
	TArray<float> WeaponCandidateWeights;
	float TotalWeaponWeight = 0.0f;
	TArray<FPrimaryAssetId> NonWeaponCandidateIds;
	TArray<float> NonWeaponCandidateWeights;
	TArray<bool> NonWeaponCandidateUniqueEquipmentFlags;
	float TotalNonWeaponWeight = 0.0f;

	for (const FPrimaryAssetId& ItemDefinitionId : AllItemDefinitionIds)
	{
		const UItemDefinition* ItemDefinition =
			AssetManager.GetPrimaryAssetObject<UItemDefinition>(ItemDefinitionId);
		if (!ItemDefinition)
		{

			continue;
		}

		const float DropRate = ItemDefinition->GetRewardChestDropWeight();
		if (!ItemDefinition->CanDropFromRewardChest())
		{
			continue;
		}

		const bool bUniqueEquipment =
			IsUniqueEquipmentItemDefinition(ItemDefinition);
		if (bUniqueEquipment
			&& ExcludedUniqueItemIds.Contains(ItemDefinitionId))
		{
			continue;
		}

		if (IsWeaponItemDefinition(ItemDefinition))
		{
			WeaponCandidateIds.Add(ItemDefinitionId);
			WeaponCandidateWeights.Add(DropRate);
			TotalWeaponWeight += DropRate;
		}
		else
		{
			NonWeaponCandidateIds.Add(ItemDefinitionId);
			NonWeaponCandidateWeights.Add(DropRate);
			NonWeaponCandidateUniqueEquipmentFlags.Add(bUniqueEquipment);
			TotalNonWeaponWeight += DropRate;
		}
	}

	const int32 WeaponSelectedIndex =
		SelectWeightedItemIndex(WeaponCandidateWeights, TotalWeaponWeight);
	const int32 DropCount = ResolveRandomRewardItemCount();
	if (WeaponCandidateIds.IsValidIndex(WeaponSelectedIndex))
	{
		OutPrimaryAssetIds.Add(WeaponCandidateIds[WeaponSelectedIndex]);
	}
	const int32 NonWeaponDropCount = FMath::Max(
		0,
		DropCount - (OutPrimaryAssetIds.IsEmpty() ? 0 : 1));
	for (int32 DropIndex = 0;
		DropIndex < NonWeaponDropCount
			&& !NonWeaponCandidateIds.IsEmpty()
			&& TotalNonWeaponWeight > 0.0f;
		++DropIndex)
	{
		const int32 SelectedIndex =
			SelectWeightedItemIndex(NonWeaponCandidateWeights, TotalNonWeaponWeight);
		if (!NonWeaponCandidateIds.IsValidIndex(SelectedIndex))
		{
			break;
		}

		OutPrimaryAssetIds.Add(NonWeaponCandidateIds[SelectedIndex]);

		const bool bSelectedUniqueEquipment =
			NonWeaponCandidateUniqueEquipmentFlags.IsValidIndex(SelectedIndex)
			&& NonWeaponCandidateUniqueEquipmentFlags[SelectedIndex];
		if (!bAllowDuplicateRandomItems || bSelectedUniqueEquipment)
		{
			TotalNonWeaponWeight -= NonWeaponCandidateWeights[SelectedIndex];
			NonWeaponCandidateIds.RemoveAt(SelectedIndex);
			NonWeaponCandidateWeights.RemoveAt(SelectedIndex);
			NonWeaponCandidateUniqueEquipmentFlags.RemoveAt(SelectedIndex);
		}
	}
}

void ARewardChest::AppendConfiguredItemPrimaryAssetIds(
	const TSet<FPrimaryAssetId>& ExcludedUniqueItemIds,
	TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const
{
	TArray<FPrimaryAssetId> WeaponIds;
	TArray<float> WeaponWeights;
	float TotalWeaponWeight = 0.0f;
	TArray<FPrimaryAssetId> NonWeaponIds;

	for (const TSoftObjectPtr<UItemDefinition>& RewardItem : RewardItems)
	{
		if (RewardItem.IsNull())
		{
			continue;
		}

		const UItemDefinition* ItemDefinition = RewardItem.Get();
		if (!ItemDefinition)
		{
			continue;
		}

		const FPrimaryAssetId PrimaryAssetId = ItemDefinition->GetPrimaryAssetId();
		if (!PrimaryAssetId.IsValid())
		{
			continue;
		}

		const bool bUniqueEquipment =
			IsUniqueEquipmentItemDefinition(ItemDefinition);
		if (bUniqueEquipment
			&& ExcludedUniqueItemIds.Contains(PrimaryAssetId))
		{
			continue;
		}

		if (IsWeaponItemDefinition(ItemDefinition))
		{
			if (!ItemDefinition->CanDropFromRewardChest())
			{
				continue;
			}

			if (WeaponIds.Contains(PrimaryAssetId))
			{
				continue;
			}
			WeaponIds.Add(PrimaryAssetId);
			const float DropWeight = ItemDefinition->GetRewardChestDropWeight();
			WeaponWeights.Add(DropWeight);
			TotalWeaponWeight += DropWeight;
		}
		else
		{
			if (bUniqueEquipment)
			{
				NonWeaponIds.AddUnique(PrimaryAssetId);
			}
			else
			{
				NonWeaponIds.Add(PrimaryAssetId);
			}
		}
	}

	const int32 SelectedWeaponIndex =
		SelectWeightedItemIndex(WeaponWeights, TotalWeaponWeight);
	if (WeaponIds.IsValidIndex(SelectedWeaponIndex))
	{
		OutPrimaryAssetIds.Add(WeaponIds[SelectedWeaponIndex]);
	}
	OutPrimaryAssetIds.Append(NonWeaponIds);
}

int32 ARewardChest::ResolveRandomRewardItemCount() const
{
	if (!bUseRandomRewardItemCountChances)
	{
		return FMath::Max(1, RandomRewardItemCount);
	}

	TArray<int32> CandidateCounts;
	TArray<float> CandidateWeights;
	float TotalChance = 0.0f;

	for (const FRewardChestItemCountChance& CountChance : RandomRewardItemCountChances)
	{
		const int32 ItemCount = FMath::Max(1, CountChance.ItemCount);
		const float Chance = FMath::Max(0.0f, CountChance.Chance);
		if (Chance <= 0.0f)
		{
			continue;
		}

		CandidateCounts.Add(ItemCount);
		CandidateWeights.Add(Chance);
		TotalChance += Chance;
	}

	const int32 SelectedIndex = SelectWeightedItemIndex(CandidateWeights, TotalChance);
	if (!CandidateCounts.IsValidIndex(SelectedIndex))
	{

		return FMath::Max(1, RandomRewardItemCount);
	}

	return CandidateCounts[SelectedIndex];
}

int32 ARewardChest::SelectWeightedItemIndex(const TArray<float>& Weights, const float TotalWeight)
{
	if (Weights.IsEmpty() || TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	float RemainingWeight = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Weights.Num(); ++Index)
	{
		RemainingWeight -= FMath::Max(0.0f, Weights[Index]);
		if (RemainingWeight <= 0.0f)
		{
			return Index;
		}
	}

	return Weights.Num() - 1;
}

bool ARewardChest::IsWeaponItemDefinition(
	const UItemDefinition* ItemDefinition) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	return ItemDefinition
		&& (ItemDefinition->HasWeaponData()
			|| ItemDefinition->MatchesItemType(
				TagConfig->GetItemWeaponTypeTag()));
}

bool ARewardChest::IsUniqueEquipmentItemDefinition(
	const UItemDefinition* ItemDefinition) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(this);
	return ItemDefinition
		&& (IsWeaponItemDefinition(ItemDefinition)
			|| ItemDefinition->MatchesItemType(
				TagConfig->GetItemEquipmentTypeTag()));
}

void ARewardChest::BeginRewardContentPreload()
{
	ReleaseRewardContentPreload();
	bRewardContentReady = false;

	TSet<FSoftObjectPath> AssetPaths;
	const auto AddSoftPath = [&AssetPaths](const auto& SoftObject)
	{
		if (!SoftObject.IsNull())
		{
			AssetPaths.Add(SoftObject.ToSoftObjectPath());
		}
	};

	AddSoftPath(RewardDefinition);
	for (const TSoftObjectPtr<UItemDefinition>& RewardItem : RewardItems)
	{
		AddSoftPath(RewardItem);
	}
	for (const TSoftObjectPtr<USkinDefinition>& RewardSkin : RewardSkins)
	{
		AddSoftPath(RewardSkin);
	}
	for (const TSoftObjectPtr<UPandoraDefinition>& RewardPandora : RewardPandoras)
	{
		AddSoftPath(RewardPandora);
	}

	if (bUseItemDefinitionDropRates)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> ItemDefinitionIds;
		AssetManager.GetPrimaryAssetIdList(
			FPrimaryAssetType(TEXT("ItemDefinition")),
			ItemDefinitionIds);
		for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
		{
			const FSoftObjectPath ItemDefinitionPath =
				AssetManager.GetPrimaryAssetPath(ItemDefinitionId);
			if (ItemDefinitionPath.IsValid())
			{
				AssetPaths.Add(ItemDefinitionPath);
			}
		}
	}

	if (AssetPaths.IsEmpty())
	{
		bRewardContentReady = true;
		return;
	}

	RewardContentPreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			AssetPaths.Array(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleRewardContentPreloadComplete));
	if (!RewardContentPreloadHandle.IsValid())
	{
		UE_LOG(
			LogRewardChest,
			Error,
			TEXT("Reward chest '%s' failed to start its reward-content preload."),
			*GetPathName());
		bRewardContentReady = true;
	}
}

void ARewardChest::HandleRewardContentPreloadComplete()
{
	bRewardContentReady = true;
}

void ARewardChest::ReleaseRewardContentPreload()
{
	bRewardContentReady = false;
	if (RewardContentPreloadHandle.IsValid())
	{
		RewardContentPreloadHandle->CancelHandle();
		RewardContentPreloadHandle->ReleaseHandle();
		RewardContentPreloadHandle.Reset();
	}
}

void ARewardChest::ConfigureChestCollision(const bool bEnableInteraction) const
{
	if (!ChestMesh)
	{

		return;
	}

	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChestMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, bEnableInteraction ? ECR_Overlap : ECR_Ignore);
	ChestMesh->SetCollisionResponseToChannel(
		ECC_GameTraceChannel3,
		bEnableInteraction ? ECR_Overlap : ECR_Ignore); // OverlapBox
	ChestMesh->SetGenerateOverlapEvents(bEnableInteraction);
	ChestMesh->SetCanEverAffectNavigation(false);
	ChestMesh->UpdateOverlaps();


}

void ARewardChest::PlayCharacterInteractionAnimation(AActor* RewardReceiver) const
{
	if (!CharacterInteractionMontage)
	{

		return;
	}

	APdPlayer* Player = Cast<APdPlayer>(RewardReceiver);
	if (!Player)
	{

		return;
	}


	Player->PlayInteractionMontage(CharacterInteractionMontage, CharacterInteractionMontagePlayRate);
}

void ARewardChest::SetChestState(const ERewardChestState NewState, AActor* RewardReceiver)
{
	if (ChestState == NewState)
	{

		return;
	}



	ChestState = NewState;
	MARK_PROPERTY_DIRTY_FROM_NAME(ARewardChest, ChestState, this);
	ApplyChestState(RewardReceiver);
}

void ARewardChest::ApplyChestState(AActor* RewardReceiver)
{


	switch (ChestState)
	{
	case ERewardChestState::Closed:
		ApplyClosedState();
		break;
	case ERewardChestState::Opening:
		ApplyOpeningState(RewardReceiver);
		break;
	case ERewardChestState::Opened:
		ApplyOpenedState();
		break;
	case ERewardChestState::Hidden:
		ApplyHiddenState();
		break;
	default:
		ApplyClosedState();
		break;
	}
}

void ARewardChest::ApplyClosedState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	if (ChestMesh)
	{
		ChestMesh->Stop();
		if (OpenAnimation)
		{
			ChestMesh->SetAnimation(OpenAnimation);
			ChestMesh->SetPosition(0.0f, false);
			ChestMesh->Stop();
		}
		ChestMesh->SetHiddenInGame(false, false);
		ChestMesh->SetVisibility(true, false);
	}
	if (OpenEffect)
	{
		OpenEffect->Deactivate();
	}

	if (InteractionBillboard)
	{
		SetInteractionAnchorVisible(true);
	}
	SetInteractionTipVisible(false);

	ConfigureChestCollision(true);

}

void ARewardChest::ApplyOpeningState(AActor* RewardReceiver)
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);
	ConfigureChestCollision(false);

	if (ChestMesh)
	{
		ChestMesh->SetHiddenInGame(false, false);
		ChestMesh->SetVisibility(true, false);
	}

	if (ChestMesh && OpenAnimation)
	{

		ChestMesh->PlayAnimation(OpenAnimation, false);
	}

	if (OpenEffect)
	{

		OpenEffect->Activate(true);
	}

	if (OpenSound)
	{

		UGameplayStatics::PlaySoundAtLocation(this, OpenSound, GetActorLocation(), OpenSoundVolume);
	}

	BP_OnChestOpened(RewardReceiver);
	ScheduleFinishOpening();
}

void ARewardChest::ApplyOpenedState()
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);
	ConfigureChestCollision(false);
	ScheduleHideOpenedChest();
}

void ARewardChest::ApplyHiddenState()
{
	SetInteractionTipVisible(false);
	SetInteractionAnchorVisible(false);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);
		World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);
	}

	if (ChestMesh)
	{
		ChestMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ChestMesh->SetGenerateOverlapEvents(false);
		ChestMesh->SetHiddenInGame(true, false);
		ChestMesh->SetVisibility(false, false);
	}
	if (OpenEffect)
	{
		OpenEffect->Deactivate();
	}


}

void ARewardChest::ScheduleFinishOpening()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FinishOpening();
		return;
	}

	World->GetTimerManager().ClearTimer(FinishOpeningTimerHandle);

	const float AnimationLength = OpenAnimation ? OpenAnimation->GetPlayLength() : 0.0f;
	const float FinishDelay = AnimationLength > 0.0f ? AnimationLength : HideAfterOpenFallbackDelay;


	if (FinishDelay <= 0.0f)
	{
		FinishOpening();
		return;
	}

	World->GetTimerManager().SetTimer(
		FinishOpeningTimerHandle,
		this,
		&ThisClass::FinishOpening,
		FinishDelay,
		false);
}

void ARewardChest::FinishOpening()
{
	if (!HasAuthority() || ChestState != ERewardChestState::Opening)
	{

		return;
	}

	SetChestState(ERewardChestState::Opened, nullptr);
}

void ARewardChest::ScheduleHideOpenedChest()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		HideOpenedChest();
		return;
	}

	World->GetTimerManager().ClearTimer(HideOpenedChestTimerHandle);


	if (HideAfterOpenFallbackDelay <= 0.0f)
	{
		HideOpenedChest();
		return;
	}

	World->GetTimerManager().SetTimer(
		HideOpenedChestTimerHandle,
		this,
		&ThisClass::HideOpenedChest,
		HideAfterOpenFallbackDelay,
		false);
}

void ARewardChest::HideOpenedChest()
{
	if (!HasAuthority() || ChestState != ERewardChestState::Opened)
	{

		return;
	}

	SetChestState(ERewardChestState::Hidden, nullptr);
}

void ARewardChest::ScheduleRespawnAfterOpen()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	const float RespawnDelay = FMath::Max(RespawnDelayAfterOpen, 0.0f);
	if (RespawnDelay <= 0.0f)
	{
		RespawnTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::RetryRespawnAtAvailableLocation);
		return;
	}

	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::RetryRespawnAtAvailableLocation,
		RespawnDelay,
		false);
}

void ARewardChest::RetryRespawnAtAvailableLocation()
{
	if (!HasAuthority() || ChestState == ERewardChestState::Closed)
	{
		return;
	}

	if (ChestState == ERewardChestState::Hidden
		&& TryRespawnAtRandomAvailableLocation())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RespawnTimerHandle,
			this,
			&ThisClass::RetryRespawnAtAvailableLocation,
			0.25f,
			false);
	}
}

bool ARewardChest::TryRespawnAtRandomAvailableLocation()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || ChestState != ERewardChestState::Hidden)
	{
		return false;
	}

	TArray<FTransform> AvailableSpawnTransforms;
	for (TActorIterator<ARewardChest> SpawnIterator(World);
		SpawnIterator;
		++SpawnIterator)
	{
		const ARewardChest* SpawnAnchor = *SpawnIterator;
		if (!IsValid(SpawnAnchor))
		{
			continue;
		}

		const FTransform CandidateTransform =
			SpawnAnchor->bOriginalSpawnTransformCaptured
				? SpawnAnchor->OriginalSpawnTransform
				: SpawnAnchor->GetActorTransform();
		const bool bAlreadyAdded = AvailableSpawnTransforms.ContainsByPredicate(
			[&CandidateTransform](const FTransform& ExistingTransform)
			{
				return ExistingTransform.GetLocation().Equals(
					CandidateTransform.GetLocation(),
					1.0f);
			});
		if (bAlreadyAdded)
		{
			continue;
		}

		bool bOccupied = false;
		for (TActorIterator<ARewardChest> OccupantIterator(World);
			OccupantIterator;
			++OccupantIterator)
		{
			const ARewardChest* Occupant = *OccupantIterator;
			if (IsValid(Occupant)
				&& Occupant->IsOccupyingSpawnLocation(CandidateTransform))
			{
				bOccupied = true;
				break;
			}
		}

		if (!bOccupied)
		{
			AvailableSpawnTransforms.Add(CandidateTransform);
		}
	}

	if (AvailableSpawnTransforms.IsEmpty())
	{
		return false;
	}

	const int32 SpawnIndex = FMath::RandRange(
		0,
		AvailableSpawnTransforms.Num() - 1);
	SetActorTransform(
		AvailableSpawnTransforms[SpawnIndex],
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetChestState(ERewardChestState::Closed, nullptr);
	ForceNetUpdate();
	return true;
}

bool ARewardChest::IsOccupyingSpawnLocation(
	const FTransform& SpawnTransform) const
{
	return ChestState != ERewardChestState::Hidden
		&& GetActorLocation().Equals(SpawnTransform.GetLocation(), 1.0f);
}

void ARewardChest::SetInteractionAnchorVisible(const bool bVisible) const
{


	if (InteractionBillboard)
	{
		InteractionBillboard->SetHiddenInGame(!bVisible);
		InteractionBillboard->SetVisibility(bVisible);
	}
}

void ARewardChest::SetInteractionTipVisible(const bool bVisible) const
{
	if (bVisible)
	{
		SetInteractionAnchorVisible(false);
	}
	else if (ChestState == ERewardChestState::Closed)
	{
		SetInteractionAnchorVisible(true);
	}



	if (InteractionTipWidget)
	{
		InteractionTipWidget->SetHiddenInGame(!bVisible, true);
		InteractionTipWidget->SetVisibility(bVisible, true);
	}
}
