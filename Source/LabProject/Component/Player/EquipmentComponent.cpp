#include "Component/Player/EquipmentComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/Enum_Operation.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentComponent)

DEFINE_LOG_CATEGORY(EquipmentComponentLog);

namespace
{
	bool IsWeaponLoadoutDirection(const EEnum_Direction Direction)
	{
		return Direction == EEnum_Direction::Left
			|| Direction == EEnum_Direction::Up
			|| Direction == EEnum_Direction::Right;
	}

	EEnum_Direction SanitizeWeaponLoadoutDirection(const EEnum_Direction Direction)
	{
		return IsWeaponLoadoutDirection(Direction) ? Direction : EEnum_Direction::Center;
	}
}

UEquipmentComponent::UEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	static ConstructorHelpers::FClassFinder<UGameplayEffect> EquippedItemEffectFinder(TEXT("/Game/GAS/Effect/GE_DynamicItemEquipped"));
	if (EquippedItemEffectFinder.Succeeded())
	{
		EquippedItemEffectClass = EquippedItemEffectFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UGameplayEffect> StatUpEffectFinder(TEXT("/Game/GAS/Effect/GE_StatUp"));
	if (StatUpEffectFinder.Succeeded())
	{
		StatUpGameplayEffectClass = StatUpEffectFinder.Class;
	}
}

void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedReferences();
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseWeaponPresentationLoads();
	Super::EndPlay(EndPlayReason);
}

void UEquipmentComponent::RefreshCachedReferences()
{
	CachedOwner = Cast<ACharacterBase>(GetOwner());
	const APdPlayerState* PdPlayerState = CachedOwner ? Cast<APdPlayerState>(CachedOwner->GetPlayerState()) : nullptr;
	CachedASC = CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
	CachedInventory = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;


}

void UEquipmentComponent::OnRep_CurrentWeaponDefinition()
{
	RefreshCachedReferences();
	AttachWeaponToOwner(CurrentWeaponActor, CurrentWeaponDefinition);
	RefreshCurrentWeaponPresentation();
	NotifyCurrentWeaponDefinitionChanged();
}

void UEquipmentComponent::OnRep_CurrentWeaponActor()
{
	RefreshCachedReferences();
	AttachWeaponToOwner(CurrentWeaponActor, CurrentWeaponDefinition);
	RefreshCurrentWeaponPresentation();
	NotifyCurrentWeaponStateChanged();
}

void UEquipmentComponent::NotifyCurrentWeaponDefinitionChanged()
{
	NotifyCurrentWeaponStateChanged();
	OnCurrentWeaponDefinitionChanged.Broadcast();
}

void UEquipmentComponent::NotifyCurrentWeaponStateChanged()
{
	RefreshCachedReferences();

	if (CachedASC)
	{
		CachedASC->NotifyAbilitiesChanged();
	}
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// =================================================================================================================

	FDoRepLifetimeParams CurrentWeaponParams;
	CurrentWeaponParams.bIsPushBased = true;

	FDoRepLifetimeParams CurrentWeaponIdParams;
	CurrentWeaponIdParams.bIsPushBased = true;
	CurrentWeaponIdParams.Condition = COND_OwnerOnly;

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponActor, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponDefinition, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponId, CurrentWeaponIdParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponLoadoutDirection, CurrentWeaponParams);
}

const UItemDefinition* UEquipmentComponent::GetRequestedWeaponDefinition() const
{
	// =================================================================================================================

	if (RequestedWeaponId.IsValid())
	{
		if (const UItemInstance* ItemInstance = FindOwnedItemInstanceById(RequestedWeaponId))
		{
			return ItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

UAnimMontage* UEquipmentComponent::GetCachedEquipMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedEquipDataItemDefinition != ItemDefinition)
	{
		CachedEquipDataItemDefinition = ItemDefinition;
		CachedEquipMontage.Reset();
		CachedEquipAnimLayerClass.Reset();
	}

	if (!CachedEquipMontage.IsValid() && !ItemDefinition->WeaponData.Equip.EquipMontage.IsNull())
	{
		CachedEquipMontage = ItemDefinition->WeaponData.Equip.EquipMontage.Get();
	}

	return CachedEquipMontage.Get();
}

TSubclassOf<UAnimInstance> UEquipmentComponent::GetCachedEquipAnimLayer(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedEquipDataItemDefinition != ItemDefinition)
	{
		CachedEquipDataItemDefinition = ItemDefinition;
		CachedEquipMontage.Reset();
		CachedEquipAnimLayerClass.Reset();
	}

	if (!CachedEquipAnimLayerClass.IsValid() && !ItemDefinition->WeaponData.Equip.AnimLayer.IsNull())
	{
		CachedEquipAnimLayerClass = ItemDefinition->WeaponData.Equip.AnimLayer.Get();
	}

	return CachedEquipAnimLayerClass.Get();
}

void UEquipmentComponent::RefreshCurrentWeaponAnimationLayer()
{
	RefreshCachedReferences();

	ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		return;
	}

	const UItemDefinition* WeaponDefinition = GetCurrentWeaponDefinition();
	if (TSubclassOf<UAnimInstance> EquipAnimLayer = GetCachedEquipAnimLayer(WeaponDefinition))
	{
		CharacterOwner->SetCurrentAnimLayer(EquipAnimLayer);
		return;
	}

	CharacterOwner->ResetAnimationToDefault();
}

bool UEquipmentComponent::IsWeaponPresentationLoaded(const UItemDefinition* ItemDefinition) const
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FWeaponDefinitionData& WeaponData = ItemDefinition->WeaponData;
	const bool bRequiresEquipTransitionMontages = !ShouldEquipWeaponsWithoutAnimation();
	return (WeaponData.Equip.ActorClass.IsNull() || WeaponData.Equip.ActorClass.IsValid())
		&& (!bRequiresEquipTransitionMontages
			|| WeaponData.Equip.EquipMontage.IsNull()
			|| WeaponData.Equip.EquipMontage.IsValid())
		&& (!bRequiresEquipTransitionMontages
			|| WeaponData.Equip.UnequipMontage.IsNull()
			|| WeaponData.Equip.UnequipMontage.IsValid())
		&& (WeaponData.Equip.AnimLayer.IsNull() || WeaponData.Equip.AnimLayer.IsValid())
		&& (WeaponData.Attack.AttackMontage.IsNull() || WeaponData.Attack.AttackMontage.IsValid())
		&& (WeaponData.HitReact.HitReactMontage.IsNull() || WeaponData.HitReact.HitReactMontage.IsValid())
		&& (WeaponData.Bow.WeaponMontage.IsNull() || WeaponData.Bow.WeaponMontage.IsValid())
		&& (WeaponData.Gun.ImpactDecalMaterial.IsNull() || WeaponData.Gun.ImpactDecalMaterial.IsValid());
}

bool UEquipmentComponent::RequestWeaponPresentationLoad(
	const UItemDefinition* ItemDefinition,
	FSimpleDelegate OnLoaded)
{
	if (!IsValid(ItemDefinition))
	{
		return false;
	}

	const FPrimaryAssetId ItemDefinitionId = ItemDefinition->GetPrimaryAssetId();
	if (!ItemDefinitionId.IsValid())
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Cannot preload weapon presentation for '%s': invalid PrimaryAssetId."),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	if (IsWeaponPresentationLoaded(ItemDefinition))
	{
		if (!WeaponPresentationLoadHandles.Contains(ItemDefinitionId))
		{
			TArray<FSoftObjectPath> PresentationAssetPaths;
			PresentationAssetPaths.Add(FSoftObjectPath(ItemDefinition));
			ItemDefinition->GetWeaponPresentationAssetPaths(PresentationAssetPaths);
			if (TSharedPtr<FStreamableHandle> RetainHandle =
				UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
					PresentationAssetPaths))
			{
				WeaponPresentationLoadHandles.Add(ItemDefinitionId, MoveTemp(RetainHandle));
			}
		}

		OnLoaded.ExecuteIfBound();
		return true;
	}

	if (OnLoaded.IsBound())
	{
		PendingWeaponPresentationCallbacks.FindOrAdd(ItemDefinitionId).Add(MoveTemp(OnLoaded));
	}

	if (const TSharedPtr<FStreamableHandle>* ExistingHandle =
		WeaponPresentationLoadHandles.Find(ItemDefinitionId))
	{
		if (ExistingHandle->IsValid() && !(*ExistingHandle)->HasLoadCompleted())
		{
			return true;
		}

		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Weapon presentation bundle completed but required assets are unavailable for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	TArray<FSoftObjectPath> PresentationAssetPaths;
	PresentationAssetPaths.Add(FSoftObjectPath(ItemDefinition));
	ItemDefinition->GetWeaponPresentationAssetPaths(PresentationAssetPaths);
	TSharedPtr<FStreamableHandle> LoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		PresentationAssetPaths,
		FStreamableDelegate::CreateUObject(
			this,
			&ThisClass::HandleWeaponPresentationLoaded,
			ItemDefinitionId));
	if (!LoadHandle.IsValid())
	{
		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Failed to start weapon presentation preload for '%s'."),
			*ItemDefinitionId.ToString());
		return false;
	}

	WeaponPresentationLoadHandles.Add(ItemDefinitionId, MoveTemp(LoadHandle));
	return true;
}

void UEquipmentComponent::HandleWeaponPresentationLoaded(const FPrimaryAssetId ItemDefinitionId)
{
	const UItemDefinition* ItemDefinition =
		UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(ItemDefinitionId);
	TArray<FSimpleDelegate> Callbacks;
	if (TArray<FSimpleDelegate>* PendingCallbacks =
		PendingWeaponPresentationCallbacks.Find(ItemDefinitionId))
	{
		Callbacks = MoveTemp(*PendingCallbacks);
		PendingWeaponPresentationCallbacks.Remove(ItemDefinitionId);
	}

	if (!IsWeaponPresentationLoaded(ItemDefinition))
	{
		UE_LOG(
			EquipmentComponentLog,
			Error,
			TEXT("Weapon presentation preload did not resolve all required assets for '%s'."),
			*ItemDefinitionId.ToString());
		return;
	}

	for (FSimpleDelegate& Callback : Callbacks)
	{
		Callback.ExecuteIfBound();
	}
}

void UEquipmentComponent::RefreshCurrentWeaponPresentation()
{
	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!IsValid(ItemDefinition))
	{
		RefreshCurrentWeaponAnimationLayer();
		return;
	}

	if (IsWeaponPresentationLoaded(ItemDefinition))
	{
		RequestWeaponPresentationLoad(ItemDefinition, FSimpleDelegate());
		RefreshCurrentWeaponAnimationLayer();
		return;
	}

	const FPrimaryAssetId ItemDefinitionId = ItemDefinition->GetPrimaryAssetId();
	const bool bLoadRequested = RequestWeaponPresentationLoad(
		ItemDefinition,
		FSimpleDelegate::CreateWeakLambda(this, [this, ItemDefinitionId]()
		{
			const UItemDefinition* CurrentDefinition = GetCurrentWeaponDefinition();
			if (IsValid(CurrentDefinition)
				&& CurrentDefinition->GetPrimaryAssetId() == ItemDefinitionId)
			{
				RefreshCurrentWeaponAnimationLayer();
			}
		}));
	if (!bLoadRequested)
	{
		RefreshCurrentWeaponAnimationLayer();
	}
}

void UEquipmentComponent::ReleaseWeaponPresentationLoads()
{
	++WeaponPresentationRequestGeneration;
	PendingDefinitionEquipAssetId = FPrimaryAssetId();
	PendingWeaponPresentationCallbacks.Reset();
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& HandlePair :
		WeaponPresentationLoadHandles)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->ReleaseHandle();
		}
	}
	WeaponPresentationLoadHandles.Reset();
}

UAnimMontage* UEquipmentComponent::GetCachedUnequipMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedUnequipDataItemDefinition != ItemDefinition)
	{
		CachedUnequipDataItemDefinition = ItemDefinition;
		CachedUnequipMontage.Reset();
	}

	if (!CachedUnequipMontage.IsValid() && !ItemDefinition->WeaponData.Equip.UnequipMontage.IsNull())
	{
		CachedUnequipMontage = ItemDefinition->WeaponData.Equip.UnequipMontage.Get();
	}

	return CachedUnequipMontage.Get();
}

UAnimMontage* UEquipmentComponent::GetCachedAttackMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedAttackDataItemDefinition != ItemDefinition)
	{
		CachedAttackDataItemDefinition = ItemDefinition;
		CachedAttackMontage.Reset();
	}

	if (!CachedAttackMontage.IsValid() && !ItemDefinition->WeaponData.Attack.AttackMontage.IsNull())
	{
		CachedAttackMontage = ItemDefinition->WeaponData.Attack.AttackMontage.Get();
	}

	return CachedAttackMontage.Get();
}

UAnimMontage* UEquipmentComponent::GetCachedHitReactMontage(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{
		return nullptr;
	}

	if (CachedHitReactDataItemDefinition != ItemDefinition)
	{
		CachedHitReactDataItemDefinition = ItemDefinition;
		CachedHitReactMontage.Reset();
	}

	if (!CachedHitReactMontage.IsValid() && !ItemDefinition->WeaponData.HitReact.HitReactMontage.IsNull())
	{
		CachedHitReactMontage = ItemDefinition->WeaponData.HitReact.HitReactMontage.Get();
	}

	return CachedHitReactMontage.Get();
}

bool UEquipmentComponent::GetEquipData(FEquipData& OutEquipData) const
{
	// =================================================================================================================

	OutEquipData = FEquipData();

	const UItemDefinition* ItemDefinition = GetRequestedWeaponDefinition();
	if (!ItemDefinition)
	{

		return false;
	}

	UAnimMontage* EquipMontage = nullptr;
	if (!ShouldEquipWeaponsWithoutAnimation())
	{
		EquipMontage = GetCachedEquipMontage(ItemDefinition);
		if (!EquipMontage)
		{
			return false;
		}
	}

	// =================================================================================================================

	OutEquipData.ItemDefinition = ItemDefinition;
	OutEquipData.EquipMontage = EquipMontage;
	OutEquipData.EquipAnimLayer = GetCachedEquipAnimLayer(ItemDefinition);
	return true;
}

bool UEquipmentComponent::GetUnequipData(FUnequipData& OutUnequipData) const
{
	// =================================================================================================================

	OutUnequipData = FUnequipData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition)
	{

		return false;
	}

	UAnimMontage* UnequipMontage = nullptr;
	if (!ShouldEquipWeaponsWithoutAnimation())
	{
		UnequipMontage = GetCachedUnequipMontage(ItemDefinition);
		if (!UnequipMontage)
		{
			return false;
		}
	}

	// =================================================================================================================

	OutUnequipData.ItemDefinition = ItemDefinition;
	OutUnequipData.UnequipMontage = UnequipMontage;
	return true;
}

bool UEquipmentComponent::ShouldEquipWeaponsWithoutAnimation() const
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	return SettingDefinition && SettingDefinition->bEquipWeaponsWithoutAnimation;
}

bool UEquipmentComponent::SetRequestedWeaponInstance(UItemInstance* WeaponInstance)
{
	// =================================================================================================================

	RefreshCachedReferences();

	FGuid WeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, WeaponId))
	{

		ClearRequestedWeapon();
		return false;
	}

	// =================================================================================================================

	RequestedWeaponId = WeaponId;

	if (!HasEquipmentAuthority())
	{

		ServerSetRequestedWeapon(WeaponId, RequestedWeaponLoadoutDirection);
	}

	return true;
}

void UEquipmentComponent::ClearRequestedWeaponInstance()
{
	ClearRequestedWeapon();
}

bool UEquipmentComponent::RequestWeaponSelectionForDirection(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	RefreshCachedReferences();
	if (CachedASC && CachedASC->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
	{
		return false;
	}

	RequestedWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(Direction);

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();



	FGuid SelectedWeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, SelectedWeaponId))
	{

		return false;
	}

	const UItemDefinition* SelectedWeaponDefinition =
		IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
	if (!IsWeaponPresentationLoaded(SelectedWeaponDefinition))
	{
		const uint32 RequestGeneration = ++WeaponPresentationRequestGeneration;
		const EEnum_Direction DeferredDirection = RequestedWeaponLoadoutDirection;
		return RequestWeaponPresentationLoad(
			SelectedWeaponDefinition,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, SelectedWeaponId, DeferredDirection, RequestGeneration]()
				{
					if (WeaponPresentationRequestGeneration != RequestGeneration)
					{
						return;
					}

					RefreshCachedReferences();
					if (UItemInstance* LoadedWeaponInstance =
						FindOwnedItemInstanceById(SelectedWeaponId))
					{
						RequestWeaponSelectionForDirection(
							DeferredDirection,
							LoadedWeaponInstance);
					}
				}));
	}

	++WeaponPresentationRequestGeneration;
	RequestWeaponPresentationLoad(SelectedWeaponDefinition, FSimpleDelegate());

	if (IsCurrentWeapon(SelectedWeaponId))
	{
		const bool bHasRequestedLoadoutDirection = IsWeaponLoadoutDirection(RequestedWeaponLoadoutDirection);
		const bool bDirectionChanged = bHasRequestedLoadoutDirection
			&& CurrentWeaponLoadoutDirection != RequestedWeaponLoadoutDirection;



		if (bHasRequestedLoadoutDirection && !HasEquipmentAuthority())
		{
			ServerSetCurrentWeaponLoadoutDirection(SelectedWeaponId, RequestedWeaponLoadoutDirection);
			return true;
		}

		if (bDirectionChanged)
		{
			ApplyCurrentWeaponLoadoutDirection(SelectedWeaponId, RequestedWeaponLoadoutDirection);
		}

		return true;
	}

	if (!SetRequestedWeaponInstance(WeaponInstance))
	{

		return false;
	}

	if (CurrentWeaponActor)
	{
		FGameplayTagContainer UnequipTagContainer;
		UnequipTagContainer.AddTag(UnequipAbilityTag);
		if (HasActiveAbilityWithTags(UnequipTagContainer))
		{

			return false;
		}

		const bool bActivatedUnequip = TryActivateSingleAbilityTag(UnequipAbilityTag);

		return bActivatedUnequip;
	}

	const bool bActivatedEquip = TryActivateSingleAbilityTag(EquipAbilityTag);

	return bActivatedEquip;
}

bool UEquipmentComponent::RequestWeaponUnequip()
{
	RefreshCachedReferences();
	++WeaponPresentationRequestGeneration;
	PendingDefinitionEquipAssetId = FPrimaryAssetId();
	ClearRequestedWeaponInstance();

	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();



	FGameplayTagContainer UnequipTagContainer;
	UnequipTagContainer.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(UnequipTagContainer))
	{

		return false;
	}

	const bool bActivated = TryActivateSingleAbilityTag(UnequipAbilityTag);

	return bActivated;
}

bool UEquipmentComponent::EquipWeapon()
{
	if (!RequestedWeaponId.IsValid())
	{

		return false;
	}

	if (!GetOwner())
	{
		return false;
	}

	if (!HasEquipmentAuthority())
	{

		ServerEquipWeapon();
		ClearRequestedWeapon();
		return true;
	}

	const FGuid WeaponId = RequestedWeaponId;
	const EEnum_Direction WeaponLoadoutDirection = RequestedWeaponLoadoutDirection;
	ClearRequestedWeapon();

	UItemInstance* WeaponInstance = FindOwnedItemInstanceById(WeaponId);
	if (!WeaponInstance)
	{

		return false;
	}

	const bool bEquipped = EquipWeaponInternal(WeaponInstance, WeaponLoadoutDirection);

	return bEquipped;
}

bool UEquipmentComponent::CompletePendingWeaponSelectionWithoutAnimation()
{
	RefreshCachedReferences();

	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	const bool bDeathTransitionActive = CachedASC
		&& (CachedASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| CachedASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f);
	if (bDeathTransitionActive)
	{
		ClearRequestedWeapon();
		return false;
	}

	if (!HasEquipmentAuthority())
	{
		// The authoritative copy completes the same pending request and
		// replicates CurrentWeapon state back. Do not leave a stale local
		// request after its predicted equipment ability was cancelled.
		ClearRequestedWeapon();
		return true;
	}

	if (!EquipWeapon())
	{
		return false;
	}

	const UItemDefinition* EquippedDefinition = GetCurrentWeaponDefinition();
	ApplyCurrentWeaponTagEffect(EquippedDefinition);
	ApplyEquipAbilityCooldown();
	RefreshCurrentWeaponAnimationLayer();
	return true;
}

bool UEquipmentComponent::TryResumePendingWeaponSelection()
{
	RefreshCachedReferences();

	if (!RequestedWeaponId.IsValid())
	{
		return false;
	}

	const bool bDeathTransitionActive = CachedASC
		&& (CachedASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| CachedASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f);
	if (bDeathTransitionActive)
	{
		ClearRequestedWeapon();
		return false;
	}

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();
	FGameplayTagContainer EquipmentTransitionTags;
	EquipmentTransitionTags.AddTag(EquipAbilityTag);
	EquipmentTransitionTags.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(EquipmentTransitionTags))
	{
		return true;
	}

	return CurrentWeaponActor
		? TryActivateSingleAbilityTag(UnequipAbilityTag)
		: TryActivateSingleAbilityTag(EquipAbilityTag);
}

bool UEquipmentComponent::EquipWeaponDefinition(const UItemDefinition* WeaponDefinition)
{
	RefreshCachedReferences();

	if (!HasEquipmentAuthority())
	{

		return false;
	}

	if (!IsWeaponDefinitionEquipable(WeaponDefinition))
	{

		return false;
	}

	if (!IsWeaponPresentationLoaded(WeaponDefinition))
	{
		const FPrimaryAssetId WeaponDefinitionId = WeaponDefinition->GetPrimaryAssetId();
		PendingDefinitionEquipAssetId = WeaponDefinitionId;
		return RequestWeaponPresentationLoad(
			WeaponDefinition,
			FSimpleDelegate::CreateWeakLambda(this, [this, WeaponDefinitionId]()
			{
				if (!HasEquipmentAuthority()
					|| PendingDefinitionEquipAssetId != WeaponDefinitionId)
				{
					return;
				}

				PendingDefinitionEquipAssetId = FPrimaryAssetId();
				if (const UItemDefinition* LoadedDefinition =
					UAssetManager::Get().GetPrimaryAssetObject<UItemDefinition>(WeaponDefinitionId))
				{
					EquipWeaponDefinition(LoadedDefinition);
				}
			}));
	}

	PendingDefinitionEquipAssetId = FPrimaryAssetId();
	RequestWeaponPresentationLoad(WeaponDefinition, FSimpleDelegate());

	if (CurrentWeaponActor && CurrentWeaponDefinition == WeaponDefinition)
	{

		return true;
	}

	TSubclassOf<AWeaponBase> WeaponClass = LoadWeaponActorClass(WeaponDefinition);
	if (!WeaponClass)
	{

		return false;
	}

	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemDefinitionStatSnapshot(WeaponDefinition, PendingStatSnapshot))
	{

		return false;
	}

	UnequipCurrentWeaponInternal();

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, WeaponDefinition);
	if (!SpawnedWeapon)
	{

		return false;
	}

	ApplyAndStoreWeaponStats(WeaponDefinition, PendingStatSnapshot);
	ApplyCurrentWeaponTagEffect(WeaponDefinition);
	CommitCurrentWeaponState(FGuid::NewGuid(), SpawnedWeapon, WeaponDefinition, EEnum_Direction::Center);
	if (ACharacterBase* CharacterOwner = CachedOwner.Get())
	{
		if (TSubclassOf<UAnimInstance> EquipAnimLayer = GetCachedEquipAnimLayer(WeaponDefinition))
		{
			CharacterOwner->SetCurrentAnimLayer(EquipAnimLayer);
		}
	}

	return true;
}

bool UEquipmentComponent::UnequipCurrentWeapon()
{
	if (!HasEquipmentAuthority())
	{
		return false;
	}

	return UnequipCurrentWeaponInternal();
}

void UEquipmentComponent::ServerEquipWeapon_Implementation()
{
	RefreshCachedReferences();
	EquipWeapon();
}

void UEquipmentComponent::ServerSetRequestedWeapon_Implementation(
	const FGuid WeaponId,
	const EEnum_Direction RequestedDirection)
{
	// =================================================================================================================

	RefreshCachedReferences();

	UItemInstance* FoundItem = nullptr;
	const UItemDefinition* ItemDefinition = nullptr;
	if (!ResolveOwnedWeaponById(WeaponId, FoundItem, ItemDefinition))
	{

		ClearRequestedWeapon();
		return;
	}

	// =================================================================================================================

	++WeaponPresentationRequestGeneration;
	RequestedWeaponId = WeaponId;
	RequestedWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(RequestedDirection);
}

bool UEquipmentComponent::RequestCurrentWeaponLoadoutDirection(
	const EEnum_Direction Direction,
	UItemInstance* WeaponInstance)
{
	RefreshCachedReferences();

	const EEnum_Direction SanitizedDirection = SanitizeWeaponLoadoutDirection(Direction);
	if (!IsWeaponLoadoutDirection(SanitizedDirection))
	{

		return false;
	}

	FGuid WeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, WeaponId) || !IsCurrentWeapon(WeaponId))
	{

		return false;
	}

	if (!HasEquipmentAuthority())
	{
		ServerSetCurrentWeaponLoadoutDirection(WeaponId, SanitizedDirection);
		return true;
	}

	return ApplyCurrentWeaponLoadoutDirection(WeaponId, SanitizedDirection);
}

void UEquipmentComponent::ServerSetCurrentWeaponLoadoutDirection_Implementation(
	const FGuid WeaponId,
	const EEnum_Direction RequestedDirection)
{
	RefreshCachedReferences();
	ApplyCurrentWeaponLoadoutDirection(WeaponId, RequestedDirection);
}

bool UEquipmentComponent::ApplyCurrentWeaponLoadoutDirection(
	const FGuid WeaponId,
	const EEnum_Direction Direction)
{
	RefreshCachedReferences();

	const EEnum_Direction SanitizedDirection = SanitizeWeaponLoadoutDirection(Direction);
	if (!HasEquipmentAuthority())
	{

		return false;
	}

	if (!WeaponId.IsValid() || !IsCurrentWeapon(WeaponId) || !IsWeaponLoadoutDirection(SanitizedDirection))
	{

		return false;
	}

	if (CurrentWeaponLoadoutDirection == SanitizedDirection)
	{

		return true;
	}

	CurrentWeaponLoadoutDirection = SanitizedDirection;
	MarkCurrentWeaponStateDirty(false, false, false, true);
	RefreshPandoraForWeaponChange();
	NotifyCurrentWeaponStateChanged();

	return true;
}

bool UEquipmentComponent::EquipWeaponInternal(
	UItemInstance* WeaponInstance,
	const EEnum_Direction WeaponLoadoutDirection)
{
	const EEnum_Direction SanitizedWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(WeaponLoadoutDirection);


	const UItemDefinition* ItemDefinition = nullptr;
	FGuid NewCurrentWeaponId;
	if (!ResolveWeaponEquipRequest(WeaponInstance, ItemDefinition, NewCurrentWeaponId))
	{

		return false;
	}

	if (IsCurrentWeapon(NewCurrentWeaponId))
	{
		const bool bWeaponLoadoutDirectionChanged = CurrentWeaponLoadoutDirection != SanitizedWeaponLoadoutDirection;
		if (CurrentWeaponLoadoutDirection != SanitizedWeaponLoadoutDirection)
		{
			CurrentWeaponLoadoutDirection = SanitizedWeaponLoadoutDirection;
			MarkCurrentWeaponStateDirty(false, false, false, true);
		}
		if (HasEquipmentAuthority() && bWeaponLoadoutDirectionChanged)
		{
			RefreshPandoraForWeaponChange();
			NotifyCurrentWeaponStateChanged();
		}

		return true;
	}

	if (!IsWeaponPresentationLoaded(ItemDefinition))
	{
		const uint32 RequestGeneration = WeaponPresentationRequestGeneration;
		return RequestWeaponPresentationLoad(
			ItemDefinition,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, NewCurrentWeaponId, SanitizedWeaponLoadoutDirection, RequestGeneration]()
				{
					if (!HasEquipmentAuthority()
						|| WeaponPresentationRequestGeneration != RequestGeneration)
					{
						return;
					}

					if (UItemInstance* LoadedWeaponInstance =
						FindOwnedItemInstanceById(NewCurrentWeaponId))
					{
						EquipWeaponInternal(
							LoadedWeaponInstance,
							SanitizedWeaponLoadoutDirection);
					}
				}));
	}

	RequestWeaponPresentationLoad(ItemDefinition, FSimpleDelegate());

	TSubclassOf<AWeaponBase> WeaponClass = LoadWeaponActorClass(ItemDefinition);
	if (!WeaponClass)
	{

		return false;
	}

	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemStatSnapshot(WeaponInstance, PendingStatSnapshot))
	{

		return false;
	}

	UnequipCurrentWeaponInternal();

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, ItemDefinition);
	if (!SpawnedWeapon)
	{

		return false;
	}

	ApplyAndStoreWeaponStats(ItemDefinition, PendingStatSnapshot);
	CommitCurrentWeaponState(NewCurrentWeaponId, SpawnedWeapon, ItemDefinition, SanitizedWeaponLoadoutDirection);

	return true;
}

bool UEquipmentComponent::ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const
{
	OutItemDefinition = nullptr;
	OutWeaponId.Invalidate();

	ACharacterBase* CharacterOwner = CachedOwner.Get();
	const UItemDefinition* ItemDefinition = WeaponInstance ? WeaponInstance->ItemDefinition.Get() : nullptr;
	if (!CharacterOwner || !ItemDefinition || !IsWeaponDefinitionEquipable(ItemDefinition))
	{

		return false;
	}

	FGuid NewCurrentWeaponId;
	if (!ResolveWeaponIdFromInstance(WeaponInstance, NewCurrentWeaponId))
	{

		return false;
	}

	OutItemDefinition = ItemDefinition;
	OutWeaponId = NewCurrentWeaponId;
	return true;
}

bool UEquipmentComponent::IsCurrentWeapon(FGuid WeaponId) const
{
	return CurrentWeaponActor
		&& CurrentWeaponId.IsValid()
		&& CurrentWeaponId == WeaponId;
}

bool UEquipmentComponent::HasEquipmentAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

bool UEquipmentComponent::ResolveWeaponIdFromInstance(UItemInstance* WeaponInstance, FGuid& OutWeaponId) const
{
	OutWeaponId.Invalidate();

	if (!IsValid(WeaponInstance) || !IsWeaponDefinitionEquipable(WeaponInstance->ItemDefinition.Get()))
	{
		return false;
	}

	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!InventoryComponent)
	{
		const ACharacterBase* CharacterOwner = CachedOwner.Get();
		if (!CharacterOwner)
		{
			CharacterOwner = Cast<ACharacterBase>(GetOwner());
		}

		const APdPlayerState* PdPlayerState = CharacterOwner ? Cast<APdPlayerState>(CharacterOwner->GetPlayerState()) : nullptr;
		InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	}

	if (!InventoryComponent)
	{
		return false;
	}

	bool bOwnsWeaponInstance = false;
	for (const TObjectPtr<UItemInstance>& OwnedItemInstance : InventoryComponent->GetAllItems().Items)
	{
		if (OwnedItemInstance.Get() == WeaponInstance)
		{
			bOwnsWeaponInstance = true;
			break;
		}
	}

	if (!bOwnsWeaponInstance)
	{
		return false;
	}

	const FGuid WeaponId = InventoryComponent->GetOrCreateItemId(WeaponInstance);
	if (!WeaponId.IsValid())
	{
		return false;
	}

	const UItemInstance* OwnedWeaponInstance = InventoryComponent->FindItemInstanceById(WeaponId);
	if (!IsValid(OwnedWeaponInstance) || OwnedWeaponInstance->ItemDefinition.Get() != WeaponInstance->ItemDefinition.Get())
	{
		return false;
	}

	OutWeaponId = WeaponId;
	return true;
}

bool UEquipmentComponent::ResolveOwnedWeaponById(
	const FGuid WeaponId,
	UItemInstance*& OutWeaponInstance,
	const UItemDefinition*& OutItemDefinition) const
{
	OutWeaponInstance = nullptr;
	OutItemDefinition = nullptr;

	if (!WeaponId.IsValid())
	{
		return false;
	}

	UItemInstance* WeaponInstance = FindOwnedItemInstanceById(WeaponId);
	const UItemDefinition* ItemDefinition = IsValid(WeaponInstance) ? WeaponInstance->ItemDefinition.Get() : nullptr;
	if (!IsValid(WeaponInstance) || !IsWeaponDefinitionEquipable(ItemDefinition))
	{
		return false;
	}

	OutWeaponInstance = WeaponInstance;
	OutItemDefinition = ItemDefinition;
	return true;
}

bool UEquipmentComponent::IsWeaponDefinitionEquipable(const UItemDefinition* ItemDefinition) const
{
	return ItemDefinition && !ItemDefinition->WeaponData.Equip.ActorClass.IsNull();
}

void UEquipmentComponent::MarkCurrentWeaponStateDirty(
	const bool bCurrentWeaponChanged,
	const bool bCurrentWeaponIdChanged,
	const bool bCurrentWeaponDefinitionChanged,
	const bool bCurrentWeaponLoadoutDirectionChanged)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	if (bCurrentWeaponChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponActor, this);
	}

	if (bCurrentWeaponIdChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponId, this);
	}

	if (bCurrentWeaponDefinitionChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponDefinition, this);
	}

	if (bCurrentWeaponLoadoutDirectionChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponLoadoutDirection, this);
	}
}

void UEquipmentComponent::RefreshPandoraForWeaponChange() const
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	const ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}

	const APdPlayerState* PlayerStateOwner = CharacterOwner ? Cast<APdPlayerState>(CharacterOwner->GetPlayerState()) : nullptr;
	if (UPandoraComponent* PandoraComponent = PlayerStateOwner ? PlayerStateOwner->GetPandoraComponent() : nullptr)
	{
		PandoraComponent->RefreshCurrentPandoraForWeaponChange();
	}
}

bool UEquipmentComponent::TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const
{
	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || !AbilityTag.IsValid())
	{

		return false;
	}

	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(AbilityTag);
	const bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTagContainer, true);

	return bActivated;
}

bool UEquipmentComponent::HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || AbilityTags.IsEmpty())
	{
		return false;
	}

	return ASC->HasActiveAbilityWithTags(AbilityTags);
}

FGameplayTag UEquipmentComponent::GetEquipAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetEquipmentEquipAbilityTag();
}

FGameplayTag UEquipmentComponent::GetUnequipAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetEquipmentUnequipAbilityTag();
}

TSubclassOf<AWeaponBase> UEquipmentComponent::LoadWeaponActorClass(const UItemDefinition* ItemDefinition) const
{
	if (!ItemDefinition)
	{

		return nullptr;
	}

	if (CachedWeaponActorClassItemDefinition != ItemDefinition)
	{
		CachedWeaponActorClassItemDefinition = ItemDefinition;
		CachedWeaponActorClass.Reset();
	}

	if (!CachedWeaponActorClass.IsValid() && !ItemDefinition->WeaponData.Equip.ActorClass.IsNull())
	{
		CachedWeaponActorClass = ItemDefinition->WeaponData.Equip.ActorClass.Get();
	}

	UClass* WeaponClass = CachedWeaponActorClass.Get();
	if (!WeaponClass)
	{

		return nullptr;
	}


	return WeaponClass;
}

AWeaponBase* UEquipmentComponent::SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const
{
	ACharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!WeaponClass || !ItemDefinition || !CharacterOwner || !OwnerMesh || !World)
	{

		return nullptr;
	}

	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	const FName AttachSocketName = ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName();
	if (AttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(AttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CharacterOwner;
	SpawnParams.Instigator = CharacterOwner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* SpawnedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);
	if (!SpawnedWeapon)
	{

		return nullptr;
	}

	SpawnedWeapon->InitializeFromItemDefinition(ItemDefinition);
	SpawnedWeapon->SetReplicates(true);
	AttachWeaponToOwner(SpawnedWeapon, ItemDefinition);
	SpawnedWeapon->ForceNetUpdate();

	return SpawnedWeapon;
}

void UEquipmentComponent::ApplyAndStoreWeaponStats(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& PendingStatSnapshot)
{
	if (ApplyItemStatSnapshot(PendingStatSnapshot, 1.f)
		&& PendingStatSnapshot.HasAnyMagnitude())
	{
		CurrentWeaponStatSnapshot = MoveTemp(PendingStatSnapshot);
	}
}

void UEquipmentComponent::ApplyCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	RefreshCachedReferences();

	if (!CachedASC)
	{

		return;
	}

	if (!EquippedItemEffectClass || !ItemDefinition || !ItemDefinition->IdTag.IsValid())
	{

		return;
	}

	FGameplayEffectContextHandle EffectContext = CachedASC->MakeEffectContext();
	EffectContext.AddSourceObject(ItemDefinition);

	FGameplayEffectSpecHandle SpecHandle = CachedASC->MakeOutgoingSpec(EquippedItemEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{

		return;
	}

	SpecHandle.Data->DynamicGrantedTags.AddTag(ItemDefinition->IdTag);
	const FActiveGameplayEffectHandle EffectHandle = CachedASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (EffectHandle.WasSuccessfullyApplied())
	{
		CurrentWeaponTagEffectHandle = EffectHandle;
	}
}

bool UEquipmentComponent::ApplyEquipAbilityCooldown()
{
	if (!HasEquipmentAuthority())
	{
		return false;
	}

	RefreshCachedReferences();
	if (!CachedASC)
	{
		return false;
	}

	if (CachedASC->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon))
	{
		return true;
	}

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	for (const FGameplayAbilitySpec& AbilitySpec : CachedASC->GetActivatableAbilities())
	{
		const UGameplayAbility* Ability = AbilitySpec.Ability.Get();
		if (!Ability || !Ability->GetAssetTags().HasTagExact(EquipAbilityTag))
		{
			continue;
		}

		const UGameplayEffect* CooldownEffect = Ability->GetCooldownGameplayEffect();
		if (!CooldownEffect)
		{
			return false;
		}

		FGameplayEffectQuery ExistingCooldownQuery;
		ExistingCooldownQuery.EffectDefinition = CooldownEffect->GetClass();
		if (!CachedASC->GetActiveEffects(ExistingCooldownQuery).IsEmpty())
		{
			return true;
		}

		FGameplayEffectContextHandle EffectContext = CachedASC->MakeEffectContext();
		EffectContext.AddSourceObject(const_cast<UGameplayAbility*>(Ability));
		FGameplayEffectSpecHandle CooldownSpec = CachedASC->MakeOutgoingSpec(
			CooldownEffect->GetClass(),
			FMath::Max(AbilitySpec.Level, 1),
			EffectContext);
		if (!CooldownSpec.IsValid() || !CooldownSpec.Data.IsValid())
		{
			return false;
		}

		CooldownSpec.Data->DynamicGrantedTags.AddTag(LabGameplayTags::Cooldown_EquipWeapon);
		CooldownSpec.Data->AppendDynamicAssetTags(
			FGameplayTagContainer(LabGameplayTags::Cooldown_EquipWeapon));
		if (const UPdGameplayAbility* PdAbility = Cast<UPdGameplayAbility>(Ability))
		{
			PdAbility->AppendCooldownRemovalPolicyTags(CooldownSpec, false);
		}
		else
		{
			CooldownSpec.Data->AppendDynamicAssetTags(
				FGameplayTagContainer(LabGameplayTags::Effect_Policy_RemoveOnDeath));
		}
		return CachedASC->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get()).WasSuccessfullyApplied();
	}

	return false;
}

void UEquipmentComponent::RemoveCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition)
{
	if (!HasEquipmentAuthority())
	{
		return;
	}

	RefreshCachedReferences();

	if (!CachedASC)
	{
		return;
	}

	if (CurrentWeaponTagEffectHandle.IsValid())
	{
		CachedASC->RemoveActiveGameplayEffect(CurrentWeaponTagEffectHandle);
		CurrentWeaponTagEffectHandle.Invalidate();
		return;
	}

	if (ItemDefinition && ItemDefinition->IdTag.IsValid())
	{
		FGameplayTagContainer GrantedTags;
		GrantedTags.AddTag(ItemDefinition->IdTag);
		CachedASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
	}
}

void UEquipmentComponent::RemoveCurrentWeaponStats()
{
	if (!CurrentWeaponStatSnapshot.HasAnyMagnitude())
	{
		return;
	}


	CurrentWeaponStatSnapshot.Reset();
}

void UEquipmentComponent::CommitCurrentWeaponState(
	const FGuid NewCurrentWeaponId,
	AWeaponBase* NewWeaponActor,
	const UItemDefinition* NewWeaponDefinition,
	const EEnum_Direction NewWeaponLoadoutDirection)
{
	const EEnum_Direction SanitizedNewWeaponLoadoutDirection = SanitizeWeaponLoadoutDirection(NewWeaponLoadoutDirection);
	const bool bCurrentWeaponChanged = CurrentWeaponActor != NewWeaponActor;
	const bool bCurrentWeaponIdChanged = CurrentWeaponId != NewCurrentWeaponId;
	const bool bCurrentWeaponDefinitionChanged = CurrentWeaponDefinition != NewWeaponDefinition;
	const bool bCurrentWeaponLoadoutDirectionChanged = CurrentWeaponLoadoutDirection != SanitizedNewWeaponLoadoutDirection;
	CurrentWeaponActor = NewWeaponActor;
	CurrentWeaponId = NewCurrentWeaponId;
	CurrentWeaponDefinition = NewWeaponDefinition;
	CurrentWeaponLoadoutDirection = SanitizedNewWeaponLoadoutDirection;



	MarkCurrentWeaponStateDirty(
		bCurrentWeaponChanged,
		bCurrentWeaponIdChanged,
		bCurrentWeaponDefinitionChanged,
		bCurrentWeaponLoadoutDirectionChanged);

	if (HasEquipmentAuthority())
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->ForceNetUpdate();
		}

		if (NewWeaponActor)
		{
			NewWeaponActor->ForceNetUpdate();
		}
	}

	if (HasEquipmentAuthority()
		&& (bCurrentWeaponDefinitionChanged || bCurrentWeaponLoadoutDirectionChanged))
	{
		RefreshPandoraForWeaponChange();
	}

	if (bCurrentWeaponDefinitionChanged)
	{
		NotifyCurrentWeaponDefinitionChanged();
	}
	else if (bCurrentWeaponLoadoutDirectionChanged || bCurrentWeaponChanged || bCurrentWeaponIdChanged)
	{
		NotifyCurrentWeaponStateChanged();
	}
}

bool UEquipmentComponent::UnequipCurrentWeaponInternal()
{
	// =================================================================================================================

	if (!CurrentWeaponId.IsValid() && !CurrentWeaponActor)
	{
		if (CurrentWeaponLoadoutDirection != EEnum_Direction::Center)
		{
			CurrentWeaponLoadoutDirection = EEnum_Direction::Center;
			MarkCurrentWeaponStateDirty(false, false, false, true);
			NotifyCurrentWeaponStateChanged();
		}

		return false;
	}



	RemoveCurrentWeaponTagEffect(CurrentWeaponDefinition);
	RemoveCurrentWeaponStats();

	// =================================================================================================================

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->Destroy();
	}

	// =================================================================================================================

	const bool bCurrentWeaponChanged = CurrentWeaponActor != nullptr;
	const bool bCurrentWeaponIdChanged = CurrentWeaponId.IsValid();
	const bool bCurrentWeaponDefinitionChanged = CurrentWeaponDefinition != nullptr;
	const bool bCurrentWeaponLoadoutDirectionChanged = CurrentWeaponLoadoutDirection != EEnum_Direction::Center;
	CurrentWeaponActor = nullptr;
	CurrentWeaponId.Invalidate();
	CurrentWeaponDefinition = nullptr;
	CurrentWeaponLoadoutDirection = EEnum_Direction::Center;

	MarkCurrentWeaponStateDirty(
		bCurrentWeaponChanged,
		bCurrentWeaponIdChanged,
		bCurrentWeaponDefinitionChanged,
		bCurrentWeaponLoadoutDirectionChanged);

	if (HasEquipmentAuthority()
		&& (bCurrentWeaponDefinitionChanged || bCurrentWeaponLoadoutDirectionChanged))
	{
		RefreshPandoraForWeaponChange();
	}

	if (bCurrentWeaponDefinitionChanged)
	{
		NotifyCurrentWeaponDefinitionChanged();
	}
	else if (bCurrentWeaponLoadoutDirectionChanged || bCurrentWeaponChanged || bCurrentWeaponIdChanged)
	{
		NotifyCurrentWeaponStateChanged();
	}

	return true;
}

bool UEquipmentComponent::GetAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition)
	{
		return false;
	}

	UAnimMontage* AttackMontage = GetCachedAttackMontage(ItemDefinition);
	if (!AttackMontage)
	{
		return false;
	}

	OutAttackData.ItemDefinition = ItemDefinition;
	OutAttackData.AttackMontage = AttackMontage;
	return true;
}

bool UEquipmentComponent::AllowsMovementDuringAttack() const
{
	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	return !ItemDefinition || ItemDefinition->WeaponData.Attack.bAllowMovementDuringAttack;
}

bool UEquipmentComponent::GetHitReactData(FHitReactData& OutHitReactData) const
{
	OutHitReactData = FHitReactData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition || ItemDefinition->WeaponData.HitReact.HitReactMontage.IsNull())
	{
		return false;
	}

	UAnimMontage* HitReactMontage = GetCachedHitReactMontage(ItemDefinition);
	if (!HitReactMontage)
	{
		return false;
	}

	OutHitReactData.ItemDefinition = ItemDefinition;
	OutHitReactData.HitReactMontage = HitReactMontage;
	return true;
}

void UEquipmentComponent::ClearRequestedWeapon()
{
	++WeaponPresentationRequestGeneration;
	RequestedWeaponId.Invalidate();
	RequestedWeaponLoadoutDirection = EEnum_Direction::Center;
}

const UItemDefinition* UEquipmentComponent::GetCurrentWeaponDefinition() const
{
	if (CurrentWeaponDefinition)
	{
		return CurrentWeaponDefinition.Get();
	}

	if (CurrentWeaponId.IsValid())
	{
		if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentWeaponId))
		{
			return EquippedItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

bool UEquipmentComponent::BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const
{
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!BuildItemDefinitionStatSnapshot(ItemDefinition, OutSnapshot))
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemInstance->Map_EnhancedStat_Magnitude)
	{
		if (!Pair.Key.IsValid()
			|| Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.EnhancedStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	return true;
}

bool UEquipmentComponent::BuildItemDefinitionStatSnapshot(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();

	if (!ItemDefinition)
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemDefinition->Map_Stat_Magnitude)
	{
		if (!Pair.Key.IsValid()
			|| Pair.Key.MatchesTagExact(LabGameplayTags::Status_Offense_Strength)
			|| FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.BaseStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	return true;
}

bool UEquipmentComponent::ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const
{
	if (!StatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{

		return false;
	}

	TMap<FGameplayTag, float> CombinedStatMagnitudes;
	const auto AddMagnitudeMap = [MagnitudeScale, &CombinedStatMagnitudes](const TMap<FGameplayTag, float>& StatMagnitudes)
	{
		for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
		{
			const float ScaledMagnitude = Pair.Value * MagnitudeScale;
			if (!Pair.Key.IsValid() || FMath::IsNearlyZero(ScaledMagnitude))
			{
				continue;
			}

			CombinedStatMagnitudes.FindOrAdd(Pair.Key) += ScaledMagnitude;
		}
	};

	AddMagnitudeMap(StatSnapshot.BaseStatMagnitudes);
	AddMagnitudeMap(StatSnapshot.EnhancedStatMagnitudes);
	if (CombinedStatMagnitudes.IsEmpty())
	{
		return true;
	}

	if (!StatUpGameplayEffectClass)
	{

		return false;
	}

	if (!ASC->ApplyStatUpEffectByTags(StatUpGameplayEffectClass, CombinedStatMagnitudes, EEnum_Operation::Add))
	{

		return false;
	}

	return true;
}

UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(FGuid ItemId) const
{
	// =================================================================================================================

	if (!ItemId.IsValid())
	{
		return nullptr;
	}

	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!InventoryComponent)
	{
		const ACharacterBase* CharacterOwner = CachedOwner.Get();
		if (!CharacterOwner)
		{
			CharacterOwner = Cast<ACharacterBase>(GetOwner());
		}

		const APdPlayerState* PdPlayerState = CharacterOwner ? Cast<APdPlayerState>(CharacterOwner->GetPlayerState()) : nullptr;
		InventoryComponent = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	}

	if (!InventoryComponent)
	{
		return nullptr;
	}

	return InventoryComponent->FindItemInstanceById(ItemId);
}

void UEquipmentComponent::AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const
{
	ACharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!WeaponActor || !OwnerMesh)
	{

		return;
	}

	// =================================================================================================================

	const FName AttachSocketName = ItemDefinition ? ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName() : NAME_None;
	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachSocketName);


}
