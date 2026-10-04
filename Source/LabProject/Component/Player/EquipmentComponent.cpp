#include "Component/Player/EquipmentComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentEffectComponent.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Item/ItemInstance.h"
#include "GameFramework/PlayerState.h"
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

// ASC는 빙의와 PlayerState 도착 순서에 따라 늦게 준비되므로, 준비 알림에서만 장착 쿨다운 태그를 구독한다.
void UEquipmentComponent::BeginPlay()
{
	bEndingPlay = false;
	Super::BeginPlay();

	AbilitySystemSubscription.SubscribeToCharacter(
		GetCharacter(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReady),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReleased));
}

// Pawn 종료 시 직접 생성한 무기를 정리한다. 무기 효과는 장비 효과 컴포넌트가 스스로 거두고, 일반 교체 알림은 보내지 않는다.
void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	// 늦게 도착한 로딩 완료와 쿨다운 알림이 종료 중인 Pawn의 장착을 다시 시작하지 않게 한다.
	ClearRequestedWeaponInstance();
	PresentationLoader.Reset();
	AbilitySystemSubscription.Reset();
	BindEquipCooldownTag(nullptr);
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

void UEquipmentComponent::HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem)
{
	BindEquipCooldownTag(ReadyAbilitySystem);
}

void UEquipmentComponent::HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem)
{
	BindEquipCooldownTag(nullptr);
}

void UEquipmentComponent::BindEquipCooldownTag(UPdAbilitySystemComponent* AbilitySystem)
{
	if (CooldownTagAbilitySystem.Get() == AbilitySystem)
	{
		return;
	}

	if (UPdAbilitySystemComponent* PreviousAbilitySystem = CooldownTagAbilitySystem.Get())
	{
		PreviousAbilitySystem->RegisterGameplayTagEvent(LabGameplayTags::Cooldown_EquipWeapon,
			EGameplayTagEventType::NewOrRemoved).Remove(EquipCooldownTagChangedDelegateHandle);
	}
	EquipCooldownTagChangedDelegateHandle.Reset();

	CooldownTagAbilitySystem = AbilitySystem;
	if (AbilitySystem)
	{
		EquipCooldownTagChangedDelegateHandle = AbilitySystem->RegisterGameplayTagEvent(LabGameplayTags::Cooldown_EquipWeapon,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleEquipCooldownTagChanged);
	}
}

ACharacterBase* UEquipmentComponent::GetCharacter() const
{
	return Cast<ACharacterBase>(GetOwner());
}

UInventoryComponent* UEquipmentComponent::GetInventory() const
{
	const ACharacterBase* Character = GetCharacter();
	const APlayerState* PlayerState = Character ? Character->GetPlayerState<APlayerState>() : nullptr;
	return PlayerState ? PlayerState->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UEquipmentEffectComponent* UEquipmentComponent::GetEquipmentEffects() const
{
	const ACharacterBase* Character = GetCharacter();
	return Character ? Character->GetEquipmentEffectComponent() : nullptr;
}

void UEquipmentComponent::OnRep_CurrentWeaponDefinition()
{
	AttachWeaponToOwner(CurrentWeaponActor, CurrentWeaponDefinition);
	RefreshCurrentWeaponPresentation();
	SyncWeaponEffect();
	NotifyCurrentWeaponDefinitionChanged();
}

void UEquipmentComponent::OnRep_CurrentWeaponActor()
{
	AttachWeaponToOwner(CurrentWeaponActor, CurrentWeaponDefinition);
	RefreshCurrentWeaponPresentation();
	NotifyCurrentWeaponStateChanged();
}

void UEquipmentComponent::OnRep_CurrentWeaponId()
{
	SyncWeaponEffect();
	NotifyCurrentWeaponStateChanged();
}

// 클라이언트도 현재 무기를 장비 효과 컴포넌트에 알려, 화면에 보여 줄 장비 능력치 합계에 무기를 넣게 한다.
void UEquipmentComponent::SyncWeaponEffect() const
{
	if (UEquipmentEffectComponent* Effects = GetEquipmentEffects())
	{
		Effects->SetWeapon(GetCurrentWeaponDefinition(), CurrentWeaponId);
	}
}

void UEquipmentComponent::NotifyCurrentWeaponDefinitionChanged()
{
	NotifyCurrentWeaponStateChanged();
	CurrentWeaponDefinitionChanged.Broadcast();
}

void UEquipmentComponent::NotifyCurrentWeaponStateChanged()
{
	if (UPdAbilitySystemComponent* AbilitySystem = AbilitySystemSubscription.GetReadyAbilitySystem())
	{
		AbilitySystem->OnAbilitiesChangedNative.Broadcast();
	}
}

void UEquipmentComponent::HandleEquipCooldownTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	if (CallbackTag != LabGameplayTags::Cooldown_EquipWeapon || NewCount > 0 || !bHasLatestWeaponRequest || !HasEquipmentAuthority())
	{
		return;
	}

	// 쿨다운 동안 막힌 마지막 요청을 다시 시도한다. 이미 그 상태면 두 요청 모두 아무 일도 하지 않는다.
	if (UItemInstance* LatestWeapon = LatestRequestedWeapon.Get())
	{
		RequestWeaponSelectionForDirection(LatestRequestedWeaponDirection, LatestWeapon);
	}
	else
	{
		RequestWeaponUnequip();
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
		EquipMontage = ItemDefinition->WeaponData.Equip.EquipMontage.Get();
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

	UAnimMontage* UnequipMontage = ItemDefinition->WeaponData.Equip.UnequipMontage.Get();
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
		if (CurrentWeaponLoadoutDirection != SanitizedWeaponLoadoutDirection)
		{
			CurrentWeaponLoadoutDirection = SanitizedWeaponLoadoutDirection;
			MarkCurrentWeaponStateDirty(false, false, false, true);
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
	return ReplaceWeapon(ItemDefinition, NewCurrentWeaponId, SanitizedWeaponLoadoutDirection);
}

bool UEquipmentComponent::ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const
{
	OutItemDefinition = nullptr;
	OutWeaponId.Invalidate();

	const UItemDefinition* ItemDefinition = WeaponInstance ? WeaponInstance->ItemDefinition.Get() : nullptr;
	if (!GetCharacter() || !ItemDefinition || !IsWeaponDefinitionEquipable(ItemDefinition))
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

	UInventoryComponent* InventoryComponent = GetInventory();
	if (!IsValid(WeaponInstance) || !IsWeaponDefinitionEquipable(WeaponInstance->ItemDefinition.Get()) || !InventoryComponent)
	{
		return false;
	}

	const bool bOwnsWeaponInstance = InventoryComponent->GetAllItems().Items.ContainsByPredicate(
		[WeaponInstance](const TObjectPtr<UItemInstance>& OwnedItemInstance)
		{
			return OwnedItemInstance.Get() == WeaponInstance;
		});
	if (!bOwnsWeaponInstance)
	{
		return false;
	}

	const FGuid WeaponId = WeaponInstance->GetOrCreateItemId();
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

	const ACharacterBase* CharacterOwner = GetCharacter();
	const APlayerState* PlayerStateOwner = CharacterOwner ? CharacterOwner->GetPlayerState<APlayerState>() : nullptr;
	if (UPandoraComponent* PandoraComponent = PlayerStateOwner ? PlayerStateOwner->FindComponentByClass<UPandoraComponent>() : nullptr)
	{
		PandoraComponent->RefreshCurrentPandoraSkills();
	}
}

bool UEquipmentComponent::TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const
{
	// Pawn이 존재해도 비동기 초기화나 빙의 전환 중에는 ASC의 Avatar가 아직 연결되지 않을 수 있다.
	// 대기 중인 무기 선택은 유지하고 현재 캐릭터가 연결된 뒤 다시 적용한다.
	UPdAbilitySystemComponent* AbilitySystem = AbilitySystemSubscription.GetReadyAbilitySystem();
	if (!AbilitySystem || !AbilityTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(AbilityTag);
	return AbilitySystem->TryActivateAbilitiesByTag(AbilityTagContainer, true);
}

bool UEquipmentComponent::HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	const UPdAbilitySystemComponent* AbilitySystem = AbilitySystemSubscription.GetReadyAbilitySystem();
	return AbilitySystem && !AbilityTags.IsEmpty() && AbilitySystem->HasActiveAbilityWithTags(AbilityTags);
}

bool UEquipmentComponent::IsDeathTransitionActive() const
{
	const UPdAbilitySystemComponent* AbilitySystem = AbilitySystemSubscription.GetReadyAbilitySystem();
	return AbilitySystem
		&& (AbilitySystem->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| AbilitySystem->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f);
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
	if (!CurrentWeaponId.IsValid() && !CurrentWeaponActor && !CurrentWeaponDefinition)
	{
		if (CurrentWeaponLoadoutDirection != EEnum_Direction::Center)
		{
			CurrentWeaponLoadoutDirection = EEnum_Direction::Center;
			MarkCurrentWeaponStateDirty(false, false, false, true);
			NotifyCurrentWeaponStateChanged();
		}
		return false;
	}

	// 무기 효과(능력치·아이템 태그)를 먼저 거둬, 판도라 스킬 갱신이 이전 무기 태그를 보지 않게 한다.
	if (UEquipmentEffectComponent* Effects = GetEquipmentEffects())
	{
		Effects->SetWeapon(nullptr, FGuid());
	}

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

	UAnimMontage* AttackMontage = ItemDefinition->WeaponData.Attack.AttackMontage.Get();
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

	UAnimMontage* HitReactMontage = ItemDefinition->WeaponData.HitReact.HitReactMontage.Get();
	if (!HitReactMontage)
	{
		return false;
	}

	OutHitReactData.ItemDefinition = ItemDefinition;
	OutHitReactData.HitReactMontage = HitReactMontage;
	return true;
}

const UItemDefinition* UEquipmentComponent::GetCurrentWeaponDefinition() const
{
	if (CurrentWeaponDefinition)
	{
		return CurrentWeaponDefinition.Get();
	}

	if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentWeaponId))
	{
		return EquippedItemInstance->ItemDefinition.Get();
	}
	return nullptr;
}

// 무기 피해량(힘)처럼 속성에 더하지 않는 무기 능력치는 전투가 여기서 직접 읽는다.
float UEquipmentComponent::GetCurrentWeaponStatMagnitude(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return 0.0f;
	}

	if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentWeaponId))
	{
		return EquippedItemInstance->GetEffectiveStatMagnitude(StatTag);
	}

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	return ItemDefinition ? ItemDefinition->Map_Stat_Magnitude.FindRef(StatTag) : 0.0f;
}

UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(const FGuid ItemId) const
{
	const UInventoryComponent* InventoryComponent = ItemId.IsValid() ? GetInventory() : nullptr;
	return InventoryComponent ? InventoryComponent->FindItemInstanceById(ItemId) : nullptr;
}

// 선택 변경과 해제는 같은 세대 번호로 이전 소유 무기·AI 무기의 로딩 완료를 무효화한다.
void UEquipmentComponent::ClearRequestedWeaponInstance()
{
	++WeaponPresentationRequestGeneration;
	RequestedWeaponId.Invalidate();
	RequestedWeaponLoadoutDirection = EEnum_Direction::Center;
}

// 소유 아이템과 AI 기본 무기가 같은 Actor 생성·무기 효과 적용·복제 확정 절차를 사용한다.
bool UEquipmentComponent::ReplaceWeapon(const UItemDefinition* Definition, const FGuid WeaponId, const EEnum_Direction Direction)
{
	const TSubclassOf<AWeaponBase> WeaponClass = Definition ? Definition->WeaponData.Equip.ActorClass.Get() : nullptr;
	if (bEndingPlay || !HasEquipmentAuthority() || !WeaponClass)
	{
		return false;
	}

	UnequipCurrentWeaponInternal();

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, Definition);
	if (!SpawnedWeapon)
	{
		return false;
	}

	// 판도라 스킬 갱신이 새 무기 태그를 보도록 상태를 확정하기 전에 무기 효과를 건다.
	if (UEquipmentEffectComponent* Effects = GetEquipmentEffects())
	{
		Effects->SetWeapon(Definition, WeaponId);
	}
	CommitCurrentWeaponState(WeaponId, SpawnedWeapon, Definition, Direction);
	return true;
}
