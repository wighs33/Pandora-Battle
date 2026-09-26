#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/AbilitySystem/SkillGameplayEffectConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Interface/TargetingInterface.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Dead);
	CooldownRemovalPolicyTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnDeath);

}

void UPdGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (bIsCleaningUpAbility || !IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(
			this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	// 파생 능력의 정리 중 들어오는 종료 알림도 같은 종료를 다시 실행하지 못하게 한다.
	bIsCleaningUpAbility = true;
	OnAbilityEnding();

	const bool bEquipmentTransitionAbility =
		GetAssetTags().HasTagExact(LabGameplayTags::Action_Equip) || GetAssetTags().HasTagExact(LabGameplayTags::Action_Unequip);


	// 사망 정리 중 정상 종료 알림이 들어와도 새 쿨다운을 적용하지 않는다.
	if (!bWasCancelled)
	{
		ApplyCooldownOnEnd(Handle, ActorInfo, ActivationInfo);
	}

	// 이제부터는 GAS의 bIsAbilityEnding이 재진입을 막는다.
	bIsCleaningUpAbility = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 종료 알림에서 같은 인스턴스가 다시 활성화되었다면 이전 시전의 후처리를 실행하지 않는다.
	if (IsActive())
	{
		return;
	}
	OnAbilityEnded(bWasCancelled);

	if (bEquipmentTransitionAbility)
	{
		return;
	}

	ACharacterBase* Character = ActorInfo ? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return;
	}

	if (UEquipmentComponent* Equipment = Character->GetEquipmentComponent())
	{
		if (!Equipment->TryResumePendingWeaponSelection())
		{
			// 장착 몽타주가 중단되어도 현재 무기의 애니메이션 레이어로 복구한다.
			Equipment->RefreshCurrentWeaponAnimationLayer();
		}
	}
	Character->ReapplyCurrentRotationPolicy();
}

void UPdGameplayAbility::OnAbilityEnding() {}

void UPdGameplayAbility::OnAbilityEnded(bool bWasCancelled) {}

void UPdGameplayAbility::ApplyCooldownOnEnd(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) {}

bool UPdGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	float ManaCost = 0.0f;
	float StaminaCost = 0.0f;
	GetResourceCosts(Handle, ActorInfo, ManaCost, StaminaCost);
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

void UPdGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent || !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return;
	}

	float ManaCost = 0.0f;
	float StaminaCost = 0.0f;
	GetResourceCosts(Handle, ActorInfo, ManaCost, StaminaCost);
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
		MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CostEffectClass, 1.0f);
	if (!SetCostEffectMagnitudes(CostSpecHandle, AppliedManaCost, AppliedStaminaCost))
	{
		return;
	}

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CostSpecHandle);
}

void UPdGameplayAbility::GetResourceCosts(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, float& ManaCost, float& StaminaCost) const
{
	ManaCost = 0.0f;
	StaminaCost = 0.0f;
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!AvatarPawn || !AvatarPawn->IsPlayerControlled()) return;

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC && Handle.IsValid() ? ASC->FindAbilitySpecFromHandle(Handle) : GetCurrentAbilitySpec();
	const UGameplayAbility* GrantedAbility = Spec ? Spec->Ability.Get() : nullptr;
	if (GrantedAbility && GrantedAbility->GetAssetTags().HasTagExact(LabGameplayTags::Action_Punch))
	{
		StaminaCost = GetDefaultActionStaminaCost(AvatarPawn);
	}
	else if (GrantedAbility && GrantedAbility->GetAssetTags().HasTagExact(LabGameplayTags::Action_Attack))
	{
		StaminaCost = GetWeaponAttackStaminaCost(AvatarPawn);
	}
}

TSubclassOf<UGameplayEffect> UPdGameplayAbility::GetCostGameplayEffectClass(const UObject* WorldContextObject)
{
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(WorldContextObject);
	return Settings ? Settings->AbilityCostGameplayEffectClass : nullptr;
}

float UPdGameplayAbility::GetWeaponAttackStaminaCost(const APawn* AvatarPawn)
{
	const ACharacterBase* Character = Cast<ACharacterBase>(AvatarPawn);
	const UEquipmentComponent* Equipment = Character ? Character->GetEquipmentComponent() : nullptr;
	const UItemDefinition* WeaponDefinition = Equipment ? Equipment->GetCurrentWeaponDefinition() : nullptr;
	return WeaponDefinition ? WeaponDefinition->GetSafeAttackStaminaCost() : GetDefaultActionStaminaCost(AvatarPawn);
}

bool UPdGameplayAbility::SetCostEffectMagnitudes(
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

float UPdGameplayAbility::GetDefaultActionStaminaCost(const UObject* WorldContextObject)
{
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(WorldContextObject);
	return Settings ? FMath::Max(Settings->ActionStaminaCost, 0.0f) : 0.0f;
}

bool UPdGameplayAbility::TryCommitAdditionalActionStaminaCost() const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
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

	FGameplayEffectSpecHandle CostSpecHandle = MakeOutgoingGameplayEffectSpec(
		GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), CostEffectClass, 1.0f);
	if (!SetCostEffectMagnitudes(CostSpecHandle, 0.0f, ActionStaminaCost))
	{
		return false;
	}

	return ApplyGameplayEffectSpecToOwner(
			GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), CostSpecHandle)
		.WasSuccessfullyApplied();
}

bool UPdGameplayAbility::ApplySharedCooldownEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const float CooldownDuration, const FGameplayTagContainer& CooldownTags) const
{
	if (CooldownDuration <= 0.0f || CooldownTags.IsEmpty())
	{
		return true;
	}

	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> CooldownEffectClass =
		SettingDefinition ? SettingDefinition->AbilityCooldownGameplayEffectClass : nullptr;
	if (!CooldownEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CooldownSpec =
		MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpec.IsValid())
	{
		return false;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Cooldown, CooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpec.Data->AppendDynamicAssetTags(CooldownTags);

	CooldownSpec.Data->AppendDynamicAssetTags(CooldownRemovalPolicyTags);
	return ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec).WasSuccessfullyApplied();
}

ACharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
}

UPdAbilitySystemComponent* UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

bool UPdGameplayAbility::UsesInputRelease(const FGameplayAbilitySpec& Spec) const
{
	return false;
}

bool UPdGameplayAbility::ShouldConfirmTargetingOnInputRelease() const
{
	return false;
}

bool UPdGameplayAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

AActor* UPdGameplayAbility::GetAttackTargetFromAvatar() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !AvatarActor->GetClass()->ImplementsInterface(UTargetingInterface::StaticClass()))
	{
		return nullptr;
	}

	AActor* AttackTarget = ITargetingInterface::Execute_GetAttackTarget(AvatarActor);
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(AttackTarget);
	return TargetCharacter && TargetCharacter->IsDead() ? nullptr : AttackTarget;
}

float UPdGameplayAbility::GetDamageBonusPercent() const
{
	const UAbilitySystemComponent* ASC =
		GetAbilitySystemComponentFromActorInfo();

	const UBasicAttributeSet* Attributes =
		ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;

	return Attributes
		? FMath::Max(Attributes->GetIntelligence(), 0.0f)
		: 0.0f;
}

float UPdGameplayAbility::CalculateDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	const float BaseDamage =
		static_cast<float>(FMath::Max(DamageConfig.Magnitude, 0.0));

	const float DamageBonusPercent =
		GetDamageBonusPercent();

	return static_cast<float>(
		BaseDamage
		* (1.0
			+ static_cast<double>(DamageBonusPercent) * 0.01));
}

bool UPdGameplayAbility::ApplyGameplayEffect(
	TSubclassOf<UGameplayEffect> GameplayEffectClass, const float EffectLevel, const int32 StackCount)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass || !ActorInfo || !HasAuthorityOrPredictionKey(ActorInfo, &CurrentActivationInfo))
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GameplayEffectClass, FMath::Max(EffectLevel, 1.0f));
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, SpecHandle).WasSuccessfullyApplied();
}

bool UPdGameplayAbility::HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass)
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return !AbilitySystemComponent->GetActiveEffects(Query).IsEmpty();
}

bool UPdGameplayAbility::RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass || !ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return AbilitySystemComponent->RemoveActiveEffects(Query) > 0;
}

int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || GrantedTags.IsEmpty() || !ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		return 0;
	}

	return AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

UAbilityTask_PlayMontageAndWait* UPdGameplayAbility::CreateDefaultMontageAndWaitTask(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay)
	{
		return nullptr;
	}

	return UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f, true);
}

UAbilityTask_WaitGameplayEvent* UPdGameplayAbility::CreateWaitGameplayEventTask(
	const FGameplayTag& EventTag, const bool bOnlyTriggerOnce, const bool bOnlyMatchExact)
{
	if (!EventTag.IsValid())
	{
		return nullptr;
	}

	return UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag, nullptr, bOnlyTriggerOnce, bOnlyMatchExact);
}

AGameplayAbilityTargetActor* UPdGameplayAbility::BeginSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask, const TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass)
{
	if (!TargetDataTask || !TargetActorClass)
	{
		return nullptr;
	}

	AGameplayAbilityTargetActor* SpawnedActor = nullptr;
	return TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedActor) ? SpawnedActor : nullptr;
}

void UPdGameplayAbility::FinishSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask, AGameplayAbilityTargetActor* SpawnedActor)
{
	if (TargetDataTask && SpawnedActor)
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedActor);
	}
}
