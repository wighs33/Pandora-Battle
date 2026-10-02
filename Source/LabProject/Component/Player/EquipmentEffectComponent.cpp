#include "Component/Player/EquipmentEffectComponent.h"

#include "AbilitySystem/Effects/EquipmentStatsEffect.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/AbilityStateComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentEffectComponent)

DEFINE_LOG_CATEGORY(LogEquipmentEffect);

namespace
{
	// 무기의 힘은 무기 피해량이라 전투 컴포넌트가 무기에서 직접 읽는다. 속성에는 더하지 않는다.
	bool IsWeaponDamageStat(const FGameplayTag& StatTag)
	{
		return StatTag.MatchesTagExact(LabGameplayTags::Status_Offense_Strength);
	}

	bool AreStatMagnitudesEqual(const TMap<FGameplayTag, float>& Left, const TMap<FGameplayTag, float>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const TPair<FGameplayTag, float>& Pair : Left)
		{
			const float* RightMagnitude = Right.Find(Pair.Key);
			if (!RightMagnitude || !FMath::IsNearlyEqual(Pair.Value, *RightMagnitude))
			{
				return false;
			}
		}
		return true;
	}
}

bool UEquipmentEffectComponent::FItemEffect::HasSameEffect(const FItemEffect& Other) const
{
	return bWeapon == Other.bWeapon
		&& GrantedTag == Other.GrantedTag
		&& AreStatMagnitudesEqual(StatMagnitudes, Other.StatMagnitudes);
}

UEquipmentEffectComponent::UEquipmentEffectComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEquipmentEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemSubscription.SubscribeToCharacter(
		Cast<ACharacterBase>(GetOwner()),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReady),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReleased));
}

void UEquipmentEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbilitySystemSubscription.Reset();
	RemoveAllEffects();
	AbilitySystem.Reset();
	BindInventory(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UEquipmentEffectComponent::SetWeapon(const UItemDefinition* InWeaponDefinition, const FGuid InWeaponItemId)
{
	WeaponDefinition = InWeaponDefinition;
	WeaponItemId = InWeaponDefinition ? InWeaponItemId : FGuid();
	RefreshEffects();
}

void UEquipmentEffectComponent::GetEquipmentBonusStatMagnitudes(TMap<FGameplayTag, float>& OutStatMagnitudes) const
{
	OutStatMagnitudes.Reset();

	TMap<FGameplayTag, FItemEffect> DesiredSlotEffects;
	FItemEffect DesiredWeaponEffect;
	BuildDesiredEffects(DesiredSlotEffects, DesiredWeaponEffect);

	const auto Append = [&OutStatMagnitudes](const FItemEffect& Effect)
	{
		for (const TPair<FGameplayTag, float>& Pair : Effect.StatMagnitudes)
		{
			OutStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
		}
	};
	for (const TPair<FGameplayTag, FItemEffect>& Pair : DesiredSlotEffects)
	{
		Append(Pair.Value);
	}
	Append(DesiredWeaponEffect);
}

// 캐릭터가 ASC에 연결될 때마다 효과를 그 ASC에 맞춘다. 다른 ASC로 옮겨 왔으면 이전 ASC의 효과부터 거둔다.
void UEquipmentEffectComponent::HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem)
{
	if (AbilitySystem.Get() != ReadyAbilitySystem)
	{
		RemoveAllEffects();
		AbilitySystem = ReadyAbilitySystem;
	}

	const APdPlayerState* PlayerState = Character ? Character->GetPlayerState<APdPlayerState>() : nullptr;
	BindInventory(PlayerState ? PlayerState->GetInventoryComponent() : nullptr);
	RefreshEffects();
}

// 빙의가 풀리거나 Pawn이 바뀌면 이 캐릭터가 건 효과를 거둔다. 다시 연결되면 준비 알림에서 다시 건다.
void UEquipmentEffectComponent::HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem)
{
	if (AbilitySystem.Get() == ReleasedAbilitySystem)
	{
		RemoveAllEffects();
		AbilitySystem.Reset();
	}
	BindInventory(nullptr);
}

void UEquipmentEffectComponent::HandleInventoryChanged()
{
	RefreshEffects();
}

bool UEquipmentEffectComponent::HasEffectAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UEquipmentEffectComponent::BindInventory(UInventoryComponent* NewInventory)
{
	if (Inventory.Get() == NewInventory)
	{
		return;
	}

	if (UInventoryComponent* PreviousInventory = Inventory.Get())
	{
		PreviousInventory->OnEquipmentSlotsChanged.Remove(EquipmentSlotsChangedHandle);
		PreviousInventory->OnInventoryChanged.Remove(InventoryChangedHandle);
	}
	EquipmentSlotsChangedHandle.Reset();
	InventoryChangedHandle.Reset();

	Inventory = NewInventory;
	if (NewInventory)
	{
		// 슬롯 교체와 장착한 아이템의 강화가 모두 능력치를 바꾼다.
		EquipmentSlotsChangedHandle = NewInventory->OnEquipmentSlotsChanged.AddUObject(this, &ThisClass::HandleInventoryChanged);
		InventoryChangedHandle = NewInventory->OnInventoryChanged.AddUObject(this, &ThisClass::HandleInventoryChanged);
	}
}

void UEquipmentEffectComponent::BuildDesiredEffects(
	TMap<FGameplayTag, FItemEffect>& OutSlotEffects,
	FItemEffect& OutWeaponEffect) const
{
	OutSlotEffects.Reset();
	OutWeaponEffect = FItemEffect();

	const UInventoryComponent* BoundInventory = Inventory.Get();
	if (BoundInventory)
	{
		TArray<FGameplayTag> SlotTags;
		UProjectTagDefinition::Get(this)->GetItemEquipmentSlotTags(SlotTags);
		for (const FGameplayTag& SlotTag : SlotTags)
		{
			const UItemInstance* Item = BoundInventory->GetEquipmentSlotItem(SlotTag);
			if (IsValid(Item) && Item->ItemDefinition)
			{
				CollectItemStats(Item->ItemDefinition, Item, OutSlotEffects.Add(SlotTag));
			}
		}
	}

	if (WeaponDefinition)
	{
		const UItemInstance* WeaponItem = BoundInventory && WeaponItemId.IsValid()
			? BoundInventory->FindItemInstanceById(WeaponItemId)
			: nullptr;
		OutWeaponEffect.bWeapon = true;
		OutWeaponEffect.GrantedTag = WeaponDefinition->IdTag;
		CollectItemStats(WeaponDefinition, WeaponItem && WeaponItem->ItemDefinition == WeaponDefinition ? WeaponItem : nullptr,
			OutWeaponEffect);
	}
}

// 인벤토리 아이템은 기본·추가·강화 능력치를 모두 더하고, 인벤토리 밖의 무기(AI)는 정의의 능력치만 쓴다.
void UEquipmentEffectComponent::CollectItemStats(
	const UItemDefinition* Definition,
	const UItemInstance* Instance,
	FItemEffect& OutEffect)
{
	TMap<FGameplayTag, float> Magnitudes;
	if (Instance)
	{
		Instance->BuildEffectiveStatMagnitudes(Magnitudes);
	}
	else if (Definition)
	{
		Magnitudes = Definition->Map_Stat_Magnitude;
	}

	OutEffect.SourceObject = Definition;
	for (const TPair<FGameplayTag, float>& Pair : Magnitudes)
	{
		if (Pair.Key.IsValid() && FMath::IsFinite(Pair.Value) && !FMath::IsNearlyZero(Pair.Value))
		{
			OutEffect.StatMagnitudes.Add(Pair.Key, Pair.Value);
		}
	}
}

void UEquipmentEffectComponent::RefreshEffects()
{
	TMap<FGameplayTag, FItemEffect> DesiredSlotEffects;
	FItemEffect DesiredWeaponEffect;
	BuildDesiredEffects(DesiredSlotEffects, DesiredWeaponEffect);

	bool bChanged = false;
	{
		// 최대 체력·마나 같은 능력치가 바뀌어도 현재 자원의 비율은 유지한다.
		FScopedResourceRatio KeepResourceRatio(HasEffectAuthority() ? AbilitySystem.Get() : nullptr);
		for (auto It = SlotEffects.CreateIterator(); It; ++It)
		{
			if (!DesiredSlotEffects.Contains(It.Key()))
			{
				RemoveEffect(It.Value());
				It.RemoveCurrent();
				bChanged = true;
			}
		}
		for (const TPair<FGameplayTag, FItemEffect>& Desired : DesiredSlotEffects)
		{
			bChanged |= UpdateEffect(SlotEffects.FindOrAdd(Desired.Key), Desired.Value);
		}
		bChanged |= UpdateEffect(WeaponEffect, DesiredWeaponEffect);
	}

	if (bChanged)
	{
		NotifyEquipmentStatsChanged();
	}
}

// 능력치나 태그가 달라진 효과만 지우고 다시 건다. ASC가 늦게 준비되어 아직 걸지 못한 효과도 여기서 건다.
bool UEquipmentEffectComponent::UpdateEffect(FItemEffect& AppliedEffect, const FItemEffect& DesiredEffect)
{
	const bool bEffectChanged = !AppliedEffect.HasSameEffect(DesiredEffect);
	if (bEffectChanged)
	{
		RemoveEffect(AppliedEffect);
		AppliedEffect = DesiredEffect;
	}

	const bool bWasApplied = AppliedEffect.Handle.IsValid();
	ApplyEffect(AppliedEffect);
	return bEffectChanged || bWasApplied != AppliedEffect.Handle.IsValid();
}

void UEquipmentEffectComponent::ApplyEffect(FItemEffect& Effect)
{
	UPdAbilitySystemComponent* TargetAbilitySystem = AbilitySystem.Get();
	if (!HasEffectAuthority() || !TargetAbilitySystem || Effect.Handle.IsValid())
	{
		return;
	}

	TMap<FGameplayTag, float> AttributeMagnitudes;
	for (const TPair<FGameplayTag, float>& Pair : Effect.StatMagnitudes)
	{
		if (Effect.bWeapon && IsWeaponDamageStat(Pair.Key))
		{
			continue;
		}
		if (!UEquipmentStatsEffect::GetStatTags().Contains(Pair.Key))
		{
			UE_LOG(LogEquipmentEffect, Warning, TEXT("%s: stat %s is not an equipment attribute and is not applied."),
				*GetNameSafe(Effect.SourceObject.Get()), *Pair.Key.ToString());
			continue;
		}
		AttributeMagnitudes.Add(Pair.Key, Pair.Value);
	}
	if (AttributeMagnitudes.IsEmpty() && !Effect.GrantedTag.IsValid())
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle =
		UEquipmentStatsEffect::MakeSpec(*TargetAbilitySystem, AttributeMagnitudes, Effect.SourceObject.Get());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	if (Effect.GrantedTag.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(Effect.GrantedTag);
	}
	Effect.Handle = TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	UE_LOG(LogEquipmentEffect, Verbose, TEXT("%s: applied %s (%d stats, tag %s)."), *GetNameSafe(GetOwner()),
		*GetNameSafe(Effect.SourceObject.Get()), AttributeMagnitudes.Num(), *Effect.GrantedTag.ToString());
}

void UEquipmentEffectComponent::RemoveEffect(FItemEffect& Effect)
{
	if (!Effect.Handle.IsValid())
	{
		return;
	}

	if (UPdAbilitySystemComponent* TargetAbilitySystem = AbilitySystem.Get())
	{
		TargetAbilitySystem->RemoveActiveGameplayEffect(Effect.Handle);
	}
	Effect.Handle.Invalidate();
	UE_LOG(LogEquipmentEffect, Verbose, TEXT("%s: removed %s."), *GetNameSafe(GetOwner()), *GetNameSafe(Effect.SourceObject.Get()));
}

// 걸어 둔 효과만 거두고 무엇을 장착했는지는 남긴다. 다시 준비되면 RefreshEffects가 같은 효과를 다시 건다.
void UEquipmentEffectComponent::RemoveAllEffects()
{
	FScopedResourceRatio KeepResourceRatio(HasEffectAuthority() ? AbilitySystem.Get() : nullptr);
	for (TPair<FGameplayTag, FItemEffect>& Pair : SlotEffects)
	{
		RemoveEffect(Pair.Value);
	}
	RemoveEffect(WeaponEffect);
}

void UEquipmentEffectComponent::NotifyEquipmentStatsChanged()
{
	if (HasEffectAuthority())
	{
		if (UPdAbilitySystemComponent* TargetAbilitySystem = AbilitySystem.Get())
		{
			TargetAbilitySystem->OnAbilitiesChangedNative.Broadcast();
		}
		const ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
		if (UAbilityStateComponent* AbilityState = Character ? Character->GetAbilityStateComponent() : nullptr)
		{
			AbilityState->ApplyMovementSpeedFromAttribute();
		}
	}
	OnEquipmentStatsChanged.Broadcast();
}
