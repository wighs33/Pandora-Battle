#include "Component/AbilitySystem/Ability/AbilityCostAndCooldownManager.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Pandora/PandoraSkillSource.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityCostAndCooldownManager)

// 능력과 원거리 직접 공격이 사용할 동일한 비용 GameplayEffect 클래스를 조회한다.
TSubclassOf<UGameplayEffect> UAbilityCostAndCooldownManager::GetCostGameplayEffectClass(const UObject* WorldContextObject)
{
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(WorldContextObject);
	return Settings ? Settings->AbilityCostGameplayEffectClass : nullptr;
}

// 장착 무기의 공격 비용을 사용하고, 무기가 없으면 공통 행동 비용을 사용한다.
float UAbilityCostAndCooldownManager::GetWeaponAttackStaminaCost(const APawn* AvatarPawn)
{
	const ACharacterBase* Character = Cast<ACharacterBase>(AvatarPawn);
	const UEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	const UItemDefinition* WeaponDefinition = Equipment ? Equipment->GetCurrentWeaponDefinition() : nullptr;
	return WeaponDefinition ? WeaponDefinition->GetSafeAttackStaminaCost() : GetDefaultActionStaminaCost(AvatarPawn);
}

// 차감량을 음수 SetByCaller 값으로 기록한다. 효과 생성·출처·예측·실제 적용은 호출자가 담당한다.
bool UAbilityCostAndCooldownManager::SetCostEffectMagnitudes(
	FGameplayEffectSpecHandle& SpecHandle, const float ManaCost, const float StaminaCost)
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_ManaCost, -FMath::Max(ManaCost, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_StaminaCost, -FMath::Max(StaminaCost, 0.0f));
	return true;
}

// 설정 서브시스템이 기본 설정까지 선택하므로 여기서는 기본 객체를 다시 조회하지 않는다.
float UAbilityCostAndCooldownManager::GetDefaultActionStaminaCost(const UObject* WorldContextObject)
{
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(WorldContextObject);
	return Settings ? FMath::Max(Settings->ActionStaminaCost, 0.0f) : 0.0f;
}

// 플레이어의 스킬·주먹 공격은 공통 행동 비용을, 무기 공격은 장착 무기의 비용을 사용한다.
float UAbilityCostAndCooldownManager::CalculateAbilityStaminaCost(
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec* AbilitySpec, const USkillDefinition* SkillDataAsset)
{
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!AvatarPawn || !AvatarPawn->IsPlayerControlled())
	{
		return 0.0f;
	}
	const UGameplayAbility* GrantedAbility = AbilitySpec ? AbilitySpec->Ability.Get() : nullptr;
	if (SkillDataAsset || (GrantedAbility && GrantedAbility->GetAssetTags().HasTagExact(LabGameplayTags::Action_Punch)))
	{
		return GetDefaultActionStaminaCost(AvatarPawn);
	}
	if (GrantedAbility && GrantedAbility->GetAssetTags().HasTagExact(LabGameplayTags::Action_Attack))
	{
		return GetWeaponAttackStaminaCost(AvatarPawn);
	}
	return 0.0f;
}

// 비용 검사와 차감

// 시전 전에 마나·스태미나와 비용 효과 설정을 검사하고, 부족하면 GAS 실패 태그를 기록한다.
bool UAbilityCostAndCooldownManager::CheckCost(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	const float ManaCost = SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->ManaCost, 0.0)) : 0.0f;
	const float StaminaCost = CalculateAbilityStaminaCost(ActorInfo, AbilitySpec, SkillDataAsset);
	if (ManaCost <= 0.0f && StaminaCost <= 0.0f)
	{
		return true;
	}

	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent ? AbilitySystemComponent->GetSet<UBasicAttributeSet>() : nullptr;
	if (BasicAttributeSet && GetCostGameplayEffectClass(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		const bool bHasEnoughMana = BasicAttributeSet->GetMana() + UE_SMALL_NUMBER >= ManaCost;
		const bool bHasEnoughStamina = BasicAttributeSet->GetStamina() + UE_SMALL_NUMBER >= StaminaCost;
		if (bHasEnoughMana && bHasEnoughStamina)
		{
			return true;
		}
	}

	const FGameplayTag& FailCostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (OptionalRelevantTags && FailCostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(FailCostTag);
	}
	return false;
}

// 서버 또는 예측 가능한 시전자에게 마나·스태미나 차감을 적용한다. 현재 자원보다 많이 차감하지 않는다.
void UAbilityCostAndCooldownManager::ApplyCost(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent || !Ability.HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	const float ManaCost = SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->ManaCost, 0.0)) : 0.0f;
	const float StaminaCost = CalculateAbilityStaminaCost(ActorInfo, AbilitySpec, SkillDataAsset);
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	if ((ManaCost <= 0.0f && StaminaCost <= 0.0f) || !BasicAttributeSet)
	{
		return;
	}

	const float AppliedManaCost = FMath::Min(FMath::Max(BasicAttributeSet->GetMana(), 0.0f), ManaCost);
	const float AppliedStaminaCost = FMath::Min(FMath::Max(BasicAttributeSet->GetStamina(), 0.0f), StaminaCost);
	const TSubclassOf<UGameplayEffect> CostEffectClass = GetCostGameplayEffectClass(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!CostEffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle CostSpecHandle =
		Ability.MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CostEffectClass, 1.0f);
	if (!SetCostEffectMagnitudes(CostSpecHandle, AppliedManaCost, AppliedStaminaCost))
	{
		return;
	}

	Ability.ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpecHandle);
}

// 콤보 후속타의 스태미나를 검사한다. 클라이언트는 가능 여부만 판단하고 서버가 실제로 차감한다.
bool UAbilityCostAndCooldownManager::TryCommitAdditionalActionStaminaCost(const UPdGameplayAbility& Ability) const
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability.GetCurrentActorInfo();
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!AvatarPawn || !AvatarPawn->IsPlayerControlled())
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent ? AbilitySystemComponent->GetSet<UBasicAttributeSet>() : nullptr;
	if (!AbilitySystemComponent || !BasicAttributeSet)
	{
		return false;
	}

	const float ActionStaminaCost = GetWeaponAttackStaminaCost(AvatarPawn);
	if (ActionStaminaCost <= 0.0f)
	{
		return true;
	}

	if (BasicAttributeSet->GetStamina() + UE_SMALL_NUMBER < ActionStaminaCost)
	{
		return false;
	}

	// Autonomous proxies only validate replicated stamina; the server spends it.
	if (!ActorInfo->IsNetAuthority())
	{
		return true;
	}

	const TSubclassOf<UGameplayEffect> CostEffectClass = GetCostGameplayEffectClass(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!CostEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CostSpecHandle = Ability.MakeOutgoingGameplayEffectSpec(
		Ability.GetCurrentAbilitySpecHandle(), ActorInfo, Ability.GetCurrentActivationInfo(), CostEffectClass, 1.0f);
	if (!SetCostEffectMagnitudes(CostSpecHandle, 0.0f, ActionStaminaCost))
	{
		return false;
	}

	return Ability
		.ApplyGameplayEffectSpecToOwner(
			Ability.GetCurrentAbilitySpecHandle(), ActorInfo, Ability.GetCurrentActivationInfo(), CostSpecHandle)
		.WasSuccessfullyApplied();
}

// 스킬 출처별 쿨다운 검사·적용

// 스킬 정의가 있으면 출처별 쿨다운을 검사한다. bOutHandled가 false면 호출자가 부모 GAS 검사를 수행한다.
bool UAbilityCostAndCooldownManager::CheckConfiguredCooldown(const UPdGameplayAbility& Ability, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* ParentCooldownTags,
	FGameplayTagContainer* OptionalRelevantTags, bool& bOutHandled) const
{
	bOutHandled = false;

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!AbilitySystemComponent || !SkillDataAsset)
	{
		return true;
	}

	bOutHandled = true;
	const float BaseDuration = static_cast<float>(FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	if (BaseDuration <= 0.0f)
	{
		return true;
	}
	const FGameplayTag DefaultCooldownTag = LabGameplayTags::Cooldown;
	const UPandoraSkillSource* Source = AbilitySpec ? Cast<UPandoraSkillSource>(AbilitySpec->SourceObject.Get()) : nullptr;
	bool bOnCooldown = false;
	if (Source)
	{
		float Remaining = 0.0f;
		float Duration = 0.0f;
		Source->GetCooldownTimeRemainingAndDuration(Remaining, Duration);
		bOnCooldown = Remaining > 0.0f;
	}
	else
	{
		FGameplayTagContainer CooldownTags(DefaultCooldownTag);
		if (ParentCooldownTags)
		{
			CooldownTags.AppendTags(*ParentCooldownTags);
		}
		bOnCooldown = AbilitySystemComponent->HasAnyMatchingGameplayTags(CooldownTags);
	}
	if (!bOnCooldown)
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		const FGameplayTag& FailCooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		OptionalRelevantTags->AddTag(FailCooldownTag.IsValid() ? FailCooldownTag : DefaultCooldownTag);
	}

	return false;
}

// 어트리뷰트가 계산한 최종 쿨타임을 적용한다. 반환값은 적용 성공이 아닌 스킬 설정 처리 여부다.
bool UAbilityCostAndCooldownManager::ApplyConfiguredCooldown(const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!SkillDataAsset)
	{
		return false;
	}

	// 스킬 쿨다운은 서버에서만 적용하고, 클라이언트는 복제된 효과를 조회한다.
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return true;
	}

	const float BaseDuration = static_cast<float>(FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	if (BaseDuration <= 0.0f)
	{
		return true;
	}
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	const float EffectiveDuration = AttributeSet ? AttributeSet->CalculateCooldownDuration(BaseDuration) : BaseDuration;

	if (EffectiveDuration <= 0.0f)
	{
		return true;
	}

	const FGameplayTagContainer DynamicCooldownTags(LabGameplayTags::Cooldown);
	Ability.ApplySharedCooldownEffect(Handle, ActorInfo, ActivationInfo, EffectiveDuration, DynamicCooldownTags);
	return true;
}
