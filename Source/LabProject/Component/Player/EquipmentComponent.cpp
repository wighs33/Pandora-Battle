#include "Component/Player/EquipmentComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/CharacterAbilityRuntimeComponent.h"
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
	if (const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this))
	{
		EquippedItemEffectClass =
			SettingDefinition->EquippedItemGameplayEffectClass;
		EquipmentStatGameplayEffectClass =
			SettingDefinition->EquipmentStatGameplayEffectClass;
	}
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
	if (const ACharacterBase* CharacterOwner = CachedOwner.Get())
	{
		if (UCharacterAbilityRuntimeComponent* AbilityRuntime =
			CharacterOwner->GetCharacterAbilityRuntimeComponent())
		{
			AbilityRuntime->ApplyMovementSpeedFromAttribute();
		}
	}
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

	UAnimMontage* UnequipMontage = GetCachedUnequipMontage(ItemDefinition);
	if (!UnequipMontage)
	{
		return false;
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

	const bool bHadCurrentWeaponState = CurrentWeaponId.IsValid()
		|| CurrentWeaponActor
		|| CurrentWeaponDefinition
		|| CurrentWeaponStatSnapshot.HasAnyMagnitude()
		|| CurrentWeaponTagEffectHandle.IsValid();
	if (!UnequipCurrentWeaponInternal() && bHadCurrentWeaponState)
	{
		return false;
	}

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, ItemDefinition);
	if (!SpawnedWeapon)
	{

		return false;
	}

	ApplyAndStoreWeaponStats(ItemDefinition, PendingStatSnapshot);
	ApplyCurrentWeaponTagEffect(ItemDefinition);
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
	return ASC->TryActivateAbilitiesByTag(AbilityTagContainer, true);
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

	const bool bHasCurrentWeaponState = CurrentWeaponId.IsValid()
		|| CurrentWeaponActor
		|| CurrentWeaponDefinition
		|| CurrentWeaponStatSnapshot.HasAnyMagnitude()
		|| CurrentWeaponTagEffectHandle.IsValid();
	if (!bHasCurrentWeaponState)
	{
		if (CurrentWeaponLoadoutDirection != EEnum_Direction::Center)
		{
			CurrentWeaponLoadoutDirection = EEnum_Direction::Center;
			MarkCurrentWeaponStateDirty(false, false, false, true);
			NotifyCurrentWeaponStateChanged();
		}

		return false;
	}

	if (!RemoveCurrentWeaponStats())
	{
		return false;
	}

	RemoveCurrentWeaponTagEffect(CurrentWeaponDefinition);

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
