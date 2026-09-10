#include "Component/Player/EquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/CharacterAbilityRuntimeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentComponent)

DEFINE_LOG_CATEGORY(EquipmentComponentLog);

UEquipmentComponent::UEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// Pawn이 사용할 ASC와 인벤토리의 변경 알림을 연결한다.
void UEquipmentComponent::BeginPlay()
{
	bEndingPlay = false;
	Super::BeginPlay();

	RefreshCachedReferences();
}

// Pawn 종료 시 적용 능력치와 직접 생성한 무기를 정리한다. 일반 교체 알림은 보내지 않는다.
void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	// 늦게 도착한 로딩 완료와 쿨다운 알림이 종료 중인 Pawn의 장착을 다시 시작하지 않게 한다.
	ClearRequestedWeaponInstance();
	ReleaseWeaponPresentationLoads();
	if (CachedASC && EquipCooldownTagChangedDelegateHandle.IsValid())
	{
		CachedASC->RegisterGameplayTagEvent(LabGameplayTags::Cooldown_EquipWeapon,
			EGameplayTagEventType::NewOrRemoved).Remove(EquipCooldownTagChangedDelegateHandle);
	}
	EquipCooldownTagChangedDelegateHandle.Reset();
	UnbindEquipmentSlotsChanged();
	UnbindInventoryChanged();
	if (HasEquipmentAuthority())
	{
		if (!ClearAppliedEquipmentState(CachedASC))
		{
			UE_LOG(
				EquipmentComponentLog,
				Error,
				TEXT("Failed to clear applied equipment state while ending play for %s."),
				*GetNameSafe(GetOwner()));
		}
	}
	if (HasEquipmentAuthority() && IsValid(CurrentWeaponActor))
	{
		CurrentWeaponActor->Destroy();
	}
	CurrentWeaponActor = nullptr;
	CurrentWeaponDefinition = nullptr;
	CurrentWeaponId.Invalidate();
	CurrentWeaponLoadoutDirection = EEnum_Direction::Center;
	Super::EndPlay(EndPlayReason);
}

// 빙의와 PlayerState 도착 순서에 맞춰 참조·구독을 갱신하고 아직 적용하지 않은 장비 능력치를 동기화한다.
void UEquipmentComponent::RefreshCachedReferences()
{
	if (bEndingPlay)
	{
		return;
	}
	CachedOwner = Cast<ACharacterBase>(GetOwner());
	const APdPlayerState* PdPlayerState = CachedOwner ? Cast<APdPlayerState>(CachedOwner->GetPlayerState()) : nullptr;
	UPdAbilitySystemComponent* NewAbilitySystem =
		CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
	UInventoryComponent* NewInventory =
		PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
	const bool bAbilitySystemChanged = CachedASC != NewAbilitySystem;
	const bool bInventoryChanged = CachedInventory != NewInventory;
	if (bAbilitySystemChanged && HasEquipmentAuthority())
	{
		if (!ClearAppliedEquipmentState(CachedASC))
		{
			UE_LOG(
				EquipmentComponentLog,
				Error,
				TEXT("Failed to clear applied equipment state before changing the ability system for %s."),
				*GetNameSafe(GetOwner()));
			// 이전 ASC의 적용 기록을 새 ASC에 적용한 것으로 간주하지 않는다.
			CurrentWeaponStatSnapshot.Reset();
			EquippedItemsStatSnapshot.Reset();
		}
	}
	if (CachedASC != NewAbilitySystem)
	{
		if (CachedASC && EquipCooldownTagChangedDelegateHandle.IsValid())
		{
			CachedASC->RegisterGameplayTagEvent(
				LabGameplayTags::Cooldown_EquipWeapon,
				EGameplayTagEventType::NewOrRemoved).Remove(
					EquipCooldownTagChangedDelegateHandle);
		}
		EquipCooldownTagChangedDelegateHandle.Reset();
		CachedASC = NewAbilitySystem;
		if (CachedASC)
		{
			EquipCooldownTagChangedDelegateHandle =
				CachedASC->RegisterGameplayTagEvent(
					LabGameplayTags::Cooldown_EquipWeapon,
					EGameplayTagEventType::NewOrRemoved).AddUObject(
						this,
						&ThisClass::HandleEquipCooldownTagChanged);
		}
	}
	if (bInventoryChanged)
	{
		UnbindEquipmentSlotsChanged();
		UnbindInventoryChanged();
		CachedInventory = NewInventory;
		if (CachedInventory)
		{
			EquipmentSlotsChangedDelegateHandle =
				CachedInventory->OnEquipmentSlotsChanged.AddUObject(
					this,
					&ThisClass::HandleEquipmentSlotsChanged);
			InventoryChangedDelegateHandle =
				CachedInventory->OnInventoryChanged.AddUObject(
					this,
					&ThisClass::HandleInventoryChanged);
		}
	}
	if (bAbilitySystemChanged || bInventoryChanged)
	{
		bEquipmentStatsInitialized = false;
	}
	if (const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this))
	{
		EquippedItemEffectClass =
			SettingDefinition->EquippedItemGameplayEffectClass;
		EquipmentStatGameplayEffectClass =
			SettingDefinition->EquipmentStatGameplayEffectClass;
	}

	if (HasEquipmentAuthority()
		&& CachedASC
		&& CachedInventory
		&& !bEquipmentStatsInitialized
		&& !bRefreshingEquipmentStats)
	{
		RefreshEquipmentStats();
	}

	if (bAbilitySystemChanged
		&& HasEquipmentAuthority()
		&& CachedASC
		&& CurrentWeaponDefinition)
	{
		ApplyCurrentWeaponTagEffect(CachedASC, CurrentWeaponDefinition);
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

void UEquipmentComponent::OnRep_CurrentWeaponId()
{
	RefreshCachedReferences();
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
		CachedASC->OnAbilitiesChangedNative.Broadcast();
	}
	OnEquipmentStatsChanged.Broadcast();
}

void UEquipmentComponent::HandleEquipCooldownTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	if (CallbackTag != LabGameplayTags::Cooldown_EquipWeapon || NewCount > 0)
	{
		return;
	}

	RefreshCachedReferences();
	APdPlayerState* PlayerState = CachedOwner
		? CachedOwner->GetPlayerState<APdPlayerState>()
		: nullptr;
	if (PlayerState)
	{
		if (USelectingPandoraAndWeaponComponent* PandoraAndWeaponComponent = PlayerState->GetSelectingPandoraAndWeaponComponent())
		{
			PandoraAndWeaponComponent->ApplySelectedPandoraAndWeapon();
		}
	}
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams CurrentWeaponParams;
	CurrentWeaponParams.bIsPushBased = true;

	FDoRepLifetimeParams CurrentWeaponIdParams;
	CurrentWeaponIdParams.bIsPushBased = true;
	CurrentWeaponIdParams.Condition = COND_OwnerOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponActor, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponDefinition, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponId, CurrentWeaponIdParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponLoadoutDirection, CurrentWeaponParams);
}

const UItemDefinition* UEquipmentComponent::GetRequestedWeaponDefinition() const
{
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
	OutEquipData = FEquipData();

	const UItemDefinition* ItemDefinition = GetRequestedWeaponDefinition();
	if (!ItemDefinition)
	{
		return false;
	}

	UAnimMontage* EquipMontage = nullptr;
	if (!ShouldEquipWeaponsWithoutAnimation())
	{
		EquipMontage = GetLoadedEquipMontage(ItemDefinition);
		if (!EquipMontage)
		{
			return false;
		}
	}

	OutEquipData.ItemDefinition = ItemDefinition;
	OutEquipData.EquipMontage = EquipMontage;
	OutEquipData.EquipAnimLayer = GetLoadedEquipAnimLayer(ItemDefinition);
	return true;
}

bool UEquipmentComponent::GetUnequipData(FUnequipData& OutUnequipData) const
{
	OutUnequipData = FUnequipData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition)
	{
		return false;
	}

	UAnimMontage* UnequipMontage = GetLoadedUnequipMontage(ItemDefinition);
	if (!UnequipMontage)
	{
		return false;
	}

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

// 장착 완료 시점에 인벤토리 소유권을 다시 확인하고 준비된 무기를 적용한다.
bool UEquipmentComponent::EquipWeaponInternal(
	UItemInstance* WeaponInstance,
	const EEnum_Direction WeaponLoadoutDirection)
{
	if (bEndingPlay || !HasEquipmentAuthority())
	{
		return false;
	}

	const EEnum_Direction SanitizedWeaponLoadoutDirection =
		PandoraLoadout::IsLoadoutDirection(WeaponLoadoutDirection) ? WeaponLoadoutDirection : EEnum_Direction::Center;

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

	FEquippedItemStatSnapshot PendingStatSnapshot;
	return BuildItemStatSnapshot(WeaponInstance, PendingStatSnapshot)
		&& ReplaceWeapon(ItemDefinition, NewCurrentWeaponId, SanitizedWeaponLoadoutDirection, PendingStatSnapshot);
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
	if (bEndingPlay || !HasEquipmentAuthority())
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
	if (bEndingPlay || !HasEquipmentAuthority())
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
	const EEnum_Direction SanitizedNewWeaponLoadoutDirection =
		PandoraLoadout::IsLoadoutDirection(NewWeaponLoadoutDirection) ? NewWeaponLoadoutDirection : EEnum_Direction::Center;
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

	RemoveCurrentWeaponTagEffect(CachedASC, CurrentWeaponDefinition);

	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->Destroy();
	}

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

	UAnimMontage* AttackMontage = GetLoadedAttackMontage(ItemDefinition);
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

	UAnimMontage* HitReactMontage = GetLoadedHitReactMontage(ItemDefinition);
	if (!HitReactMontage)
	{
		return false;
	}

	OutHitReactData.ItemDefinition = ItemDefinition;
	OutHitReactData.HitReactMontage = HitReactMontage;
	return true;
}

// 선택 변경과 해제는 같은 세대 번호로 이전 소유 무기·AI 무기의 로딩 완료를 무효화한다.
void UEquipmentComponent::ClearRequestedWeaponInstance()
{
	++WeaponPresentationRequestGeneration;
	RequestedWeaponId.Invalidate();
	RequestedWeaponLoadoutDirection = EEnum_Direction::Center;
}

// 소유 아이템과 AI 기본 무기가 같은 Actor 생성·능력치 적용·복제 확정 절차를 사용한다.
bool UEquipmentComponent::ReplaceWeapon(const UItemDefinition* Definition, const FGuid WeaponId,
	const EEnum_Direction Direction, const FEquippedItemStatSnapshot& StatSnapshot)
{
	const TSubclassOf<AWeaponBase> WeaponClass = GetLoadedWeaponActorClass(Definition);
	if (bEndingPlay || !HasEquipmentAuthority() || !WeaponClass)
	{
		return false;
	}

	const bool bHadCurrentWeaponState = CurrentWeaponId.IsValid() || CurrentWeaponActor || CurrentWeaponDefinition
		|| CurrentWeaponStatSnapshot.HasAnyMagnitude() || CurrentWeaponTagEffectHandle.IsValid();
	if (!UnequipCurrentWeaponInternal() && bHadCurrentWeaponState)
	{
		return false;
	}

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, Definition);
	if (!SpawnedWeapon)
	{
		return false;
	}
	if (!ApplyAndStoreWeaponStats(StatSnapshot))
	{
		SpawnedWeapon->Destroy();
		return false;
	}
	ApplyCurrentWeaponTagEffect(CachedASC, Definition);
	CommitCurrentWeaponState(WeaponId, SpawnedWeapon, Definition, Direction);
	return true;
}
