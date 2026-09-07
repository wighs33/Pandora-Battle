#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/AbilityMovementRuntime.h"
#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"
#include "Component/AbilitySystem/Ability/AbilityResourceRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

namespace
{
bool IsDeadCharacter(const AActor* Actor)
{
	const ACharacterBase* Character = Cast<ACharacterBase>(Actor);
	const UAbilitySystemComponent* AbilitySystemComponent =
		Character ? Character->GetAbilitySystemComponent() : nullptr;
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead);
}
}

UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(
		LabGameplayTags::GameplayAbility_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Dead);
	CooldownRemovalPolicyTags.AddTag(
		LabGameplayTags::Effect_Policy_RemoveOnDeath);

	ResourceRuntime = ObjectInitializer.CreateDefaultSubobject<UAbilityResourceRuntime>(this, TEXT("ResourceRuntime"));
	MovementRuntime = ObjectInitializer.CreateDefaultSubobject<UAbilityMovementRuntime>(this, TEXT("MovementRuntime"));
	PresentationRuntime = ObjectInitializer.CreateDefaultSubobject<UAbilityPresentationRuntime>(this, TEXT("PresentationRuntime"));
	// 이 세 객체는 모든 능력이 소유하는 필수 구성이다.
	check(ResourceRuntime && MovementRuntime && PresentationRuntime);
}

ACharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
}

APdPlayerState* UPdGameplayAbility::GetPdPlayerStateFromActorInfo() const
{
	if (const ACharacterBase* Character =
		GetPdCharacterFromActorInfo())
	{
		return Character->GetPlayerState<APdPlayerState>();
	}

	return Cast<APdPlayerState>(GetOwningActorFromActorInfo());
}

UPdAbilitySystemComponent*
UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(
		GetAbilitySystemComponentFromActorInfo());
}

void UPdGameplayAbility::AppendCooldownRemovalPolicyTags(
	FGameplayEffectSpecHandle& CooldownSpecHandle) const
{
	ResourceRuntime->AppendCooldownRemovalPolicyTags(CooldownSpecHandle, CooldownRemovalPolicyTags);
}

void UPdGameplayAbility::SuppressPendingCooldownForRuntimeReset() const
{
	ResourceRuntime->SuppressPendingCooldown();
}

void UPdGameplayAbility::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// 새 시전 직전에만 방향을 갱신한다. 교체 도중 실행 중인 시전의 출처는 변경하지 않는다.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		const APdPlayerState* PlayerState = Cast<APdPlayerState>(ActorInfo->OwnerActor.Get());
		const UPandoraComponent* Pandora = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
		const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		UPandoraSkillRuntimeContext* Source = Spec ? Cast<UPandoraSkillRuntimeContext>(Spec->SourceObject.Get()) : nullptr;
		if (Source && Pandora && Source->GetPandoraDefinition() == Pandora->GetCurrentPandoraDefinition())
		{
			Source->Initialize(Source->GetPandoraDefinition(), Source->GetSkillDataAsset(), Source->GetSkillIndex(),
				Source->GetPandoraLevel(), Pandora->GetCurrentPandoraLoadoutDirection());
		}
	}
	Super::PreActivate(
		Handle,
		ActorInfo,
		ActivationInfo,
		OnGameplayAbilityEndedDelegate,
		TriggerEventData);
	CleanupConfiguredPresentation();

	UPdAbilitySystemComponent* AbilitySystemComponent = ActorInfo
		? Cast<UPdAbilitySystemComponent>(
			ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: nullptr;
	const USkillDefinition* SkillDefinition = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	const bool bInputDrivenSkill = SkillDefinition
		&& (SkillDefinition->SkillType == ESkillType::Instant
			|| SkillDefinition->SkillType == ESkillType::Press
			|| SkillDefinition->SkillType == ESkillType::Duration);
	if (!AbilitySystemComponent || !bInputDrivenSkill)
	{
		return;
	}

	FGameplayTagContainer EquipmentTransitionTags;
	EquipmentTransitionTags.AddTag(LabGameplayTags::Action_Equip);
	EquipmentTransitionTags.AddTag(LabGameplayTags::Action_Unequip);
	if (AbilitySystemComponent->HasActiveAbilityWithTags(
		EquipmentTransitionTags))
	{
		AbilitySystemComponent->CancelAbilities(
			&EquipmentTransitionTags,
			nullptr,
			this);
	}
}

const FGameplayTagContainer* UPdGameplayAbility::GetCooldownTags() const
{
	return ResourceRuntime->BuildCooldownTags(*this, Super::GetCooldownTags());
}

bool UPdGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)
		&& ResourceRuntime->CheckCost(*this, Handle, ActorInfo, OptionalRelevantTags);
}

void UPdGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	ResourceRuntime->ApplyCost(*this, Handle, ActorInfo, ActivationInfo);
}

bool UPdGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	bool bHandled = false;
	const bool bAvailable = ResourceRuntime->CheckConfiguredCooldown(
		*this, Handle, ActorInfo, Super::GetCooldownTags(), OptionalRelevantTags, bHandled);
	return bHandled ? bAvailable : Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

void UPdGameplayAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (ResourceRuntime->ShouldDeferCooldown(
			*this,
			Handle,
			ActorInfo))
	{
		ResourceRuntime->MarkCooldownForAbilityEnd();
		return;
	}

	ApplyCooldownImmediately(Handle, ActorInfo, ActivationInfo);
}

bool UPdGameplayAbility::CommitAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		OptionalRelevantTags))
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: GetCurrentAbilitySpec();
	const USkillDefinition* SkillDefinition = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (SkillDefinition)
	{
		StopAvatarMovementForSkillActivation();

		// 비용을 지불한 보호 스킬은 일반 취소로 끊지 않는다. 사망·리셋은 별도로 취소 가능 상태를 복구한다.
		if (!SkillDefinition->bCancelOnHit)
		{
			SetCanBeCanceled(false);
		}
	}

	StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
	return true;
}

// 종료 검사·지연·공통 정리는 여기서 한 번만 수행하고, 파생 능력에는 정리 시점만 제공한다.
void UPdGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (bIsEndingAbilityRuntime || !IsEndAbilityValid(Handle, ActorInfo))
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
	bIsEndingAbilityRuntime = true;
	OnAbilityEnding();

	const bool bEquipmentTransitionAbility =
		GetAssetTags().HasTagExact(LabGameplayTags::Action_Equip)
		|| GetAssetTags().HasTagExact(LabGameplayTags::Action_Unequip);

	CleanupConfiguredPresentation();
	StopConfiguredSelfBuff();
	StopMovementContactDamage();
	StopDurationMovementLock();
	RestoreAvatarMovementForAbility();

	const UPdAbilitySystemComponent* AbilitySystemComponent = ActorInfo
		? Cast<UPdAbilitySystemComponent>(
			ActorInfo->AbilitySystemComponent.Get())
		: nullptr;
	const bool bCooldownWasPending = ResourceRuntime->ConsumePendingCooldown(bWasCancelled);
	const bool bShouldApplySkillCooldown =
		bCooldownWasPending
		&& !(AbilitySystemComponent
			&& AbilitySystemComponent->IsResettingAbilityRuntimeState());
	if (bShouldApplySkillCooldown)
	{
		ApplyCooldownImmediately(Handle, ActorInfo, ActivationInfo);
	}

	// 이제부터는 GAS의 bIsAbilityEnding이 재진입을 막는다.
	bIsEndingAbilityRuntime = false;
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);

	// 종료 알림에서 같은 인스턴스가 다시 활성화되었다면 이전 시전의 후처리를 실행하지 않는다.
	if (IsActive())
	{
		return;
	}
	OnAbilityEnded(bWasCancelled);

	if (!bEquipmentTransitionAbility)
	{
		ACharacterBase* Character = ActorInfo
			? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get())
			: nullptr;
		if (UEquipmentComponent* EquipmentComponent =
			Character ? Character->GetEquipmentComponent() : nullptr)
		{
			const bool bEquipmentTransitionResumed =
				EquipmentComponent->TryResumePendingWeaponSelection();
			if (!bEquipmentTransitionResumed)
			{
				// 장착 몽타주가 중단되어도 현재 무기의 애니메이션 레이어로 복구한다.
				EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
			}
		}

		if (Character)
		{
			Character->ReapplyCurrentRotationPolicy();
		}
	}
}

void UPdGameplayAbility::OnAbilityEnding()
{
}

void UPdGameplayAbility::OnAbilityEnded(bool bWasCancelled)
{
}

void UPdGameplayAbility::FinishAbilityFromDuration()
{
	if (!IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		return;
	}

	const bool bReplicateEndAbility =
		CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		bReplicateEndAbility,
		false);
}

bool UPdGameplayAbility::CanExecuteSkillPayload() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || IsDeadCharacter(AvatarActor))
	{
		return false;
	}

	const UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	if (BasicAttributeSet && BasicAttributeSet->GetHealth() <= 0.0f)
	{
		return false;
	}

	return !AbilitySystemComponent
		|| !AbilitySystemComponent->IsResettingAbilityRuntimeState();
}

void UPdGameplayAbility::CancelAbilityForSkillExecutionFailure()
{
	// 실제 스킬 실행 실패는 보호 상태라도 취소로 종료해, 예약된 종료 쿨다운을 부과하지 않는다.
	if (!CanBeCanceled())
	{
		SetCanBeCanceled(true);
	}

	K2_CancelAbility();
}

void UPdGameplayAbility::ApplyCooldownImmediately(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const bool bHandled = ResourceRuntime->ApplyConfiguredCooldownImmediately(
			*this,
			Handle,
			ActorInfo,
			ActivationInfo);
	if (!bHandled)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

bool UPdGameplayAbility::ApplySharedCooldownEffect(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const float CooldownDuration,
	const FGameplayTagContainer& CooldownTags) const
{
	if (CooldownDuration <= 0.0f || CooldownTags.IsEmpty())
	{
		return true;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> CooldownEffectClass =
		SettingDefinition
			? SettingDefinition->AbilityCooldownGameplayEffectClass
			: nullptr;
	if (!CooldownEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CooldownSpec = MakeOutgoingGameplayEffectSpec(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownEffectClass,
		GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpec.IsValid() || !CooldownSpec.Data.IsValid())
	{
		return false;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_Cooldown,
		CooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpec.Data->AppendDynamicAssetTags(CooldownTags);

	AppendCooldownRemovalPolicyTags(CooldownSpec);
	return ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownSpec).WasSuccessfullyApplied();
}

bool UPdGameplayAbility::TryActivateAbilitiesByTags(
	FGameplayTagContainer InAbilityTags,
	const bool bAllowRemoteActivation) const
{
	UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	return AbilitySystemComponent
		&& !InAbilityTags.IsEmpty()
		&& AbilitySystemComponent->TryActivateAbilitiesByTag(
			InAbilityTags,
			bAllowRemoteActivation);
}

bool UPdGameplayAbility::HasPlayerController() const
{
	const APawn* AvatarPawn =
		Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller =
		AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

AActor* UPdGameplayAbility::GetAttackTargetFromAvatar() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)
		|| !AvatarActor->GetClass()->ImplementsInterface(
			UTargetingInterface::StaticClass()))
	{
		return nullptr;
	}

	AActor* AttackTarget =
		ITargetingInterface::Execute_GetAttackTarget(AvatarActor);
	return IsDeadCharacter(AttackTarget) ? nullptr : AttackTarget;
}

// 선택 중인 판도라를 추정하지 않고, 부여된 능력의 출처만 사용한다.
const USkillDefinition* UPdGameplayAbility::ResolveSourceSkillDataAsset(UObject* SourceObject)
{
	if (USkillDefinition* SkillDefinition = Cast<USkillDefinition>(SourceObject))
	{
		return SkillDefinition;
	}
	const UPandoraSkillRuntimeContext* Source = Cast<UPandoraSkillRuntimeContext>(SourceObject);
	return Source ? Source->GetSkillDataAsset() : nullptr;
}

USkillDefinition* UPdGameplayAbility::GetSourceSkillDataAsset() const
{
	return const_cast<USkillDefinition*>(ResolveSourceSkillDataAsset(GetCurrentSourceObject()));
}

UPandoraSkillRuntimeContext* UPdGameplayAbility::GetSourceSkillRuntimeContext() const
{
	return Cast<UPandoraSkillRuntimeContext>(GetCurrentSourceObject());
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPdGameplayAbility::GetSourceProjectileImpactEffectAreas() const
{
	const UPandoraSkillRuntimeContext* Source = GetSourceSkillRuntimeContext();
	return Source ? Source->GetProjectileImpactEffectAreas() : TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

AWeaponBase* UPdGameplayAbility::GetCurrentWeaponActorFromAvatar() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent =
		Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponActor()
		: nullptr;
}

bool UPdGameplayAbility::HasCurrentWeaponSkillTrail() const
{
	const AWeaponBase* CurrentWeapon =
		GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon && CurrentWeapon->HasSkillWeaponTrailComponent();
}

bool UPdGameplayAbility::StartCurrentWeaponSkillTrail(
	UNiagaraSystem* TrailSystem) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon
		? CurrentWeapon->StartSkillWeaponTrail(TrailSystem)
		: false;
}

void UPdGameplayAbility::StopCurrentWeaponSkillTrail() const
{
	if (AWeaponBase* CurrentWeapon =
		GetCurrentWeaponActorFromAvatar())
	{
		CurrentWeapon->StopSkillWeaponTrail();
	}
}

bool UPdGameplayAbility::TryCommitAdditionalActionStaminaCost() const
{
	return ResourceRuntime->TryCommitAdditionalActionStaminaCost(*this);
}

float UPdGameplayAbility::CalculateBaseSkillDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	return static_cast<float>(FMath::Max(DamageConfig.Magnitude, 0.0));
}

float UPdGameplayAbility::ApplyIntelligenceToSkillDamage(const float DamageMagnitude) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* Attributes = ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;
	float DamageBonusPercent = Attributes ? FMath::Max(Attributes->GetIntelligence(), 0.0f) : 0.0f;
	const UPandoraSkillRuntimeContext* Source = GetSourceSkillRuntimeContext();
	if (Attributes && Source)
	{
		switch (Source->GetLoadoutDirection())
		{
		case EEnum_Direction::Left: DamageBonusPercent += FMath::Max(Attributes->GetFirstPandora(), 0.0f); break;
		case EEnum_Direction::Up: DamageBonusPercent += FMath::Max(Attributes->GetSecondPandora(), 0.0f); break;
		case EEnum_Direction::Right: DamageBonusPercent += FMath::Max(Attributes->GetThirdPandora(), 0.0f); break;
		default: break;
		}
	}
	return static_cast<float>(FMath::Max(DamageMagnitude, 0.0f) * (1.0 + static_cast<double>(DamageBonusPercent) * 0.01));
}

float UPdGameplayAbility::CalculateSkillDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	return ApplyIntelligenceToSkillDamage(
		CalculateBaseSkillDamageMagnitude(DamageConfig));
}

FGameplayEffectSpecHandle
UPdGameplayAbility::MakeConfiguredDamageEffectSpec(
	const FSkillGameplayEffectConfig& DamageConfig,
	const float DamageMagnitude,
	UObject* SourceObject) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !DamageConfig.GameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddInstigator(
		GetAvatarActorFromActorInfo(),
		GetAvatarActorFromActorInfo());
	if (SourceObject)
	{
		EffectContext.AddSourceObject(SourceObject);
	}
	else
	{
		EffectContext.AddSourceObject(GetCurrentSourceObject());
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(
			DamageConfig.GameplayEffectClass,
			FMath::Max(GetAbilityLevel(), 1),
			EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag DamageDataTag = DamageConfig.MagnitudeDataTag;
	if (!DamageDataTag.IsValid())
	{
		SourceAbilitySystemComponent->ResolveDamageMagnitudeSetByCallerTag(
			DamageDataTag);
	}

	if (DamageDataTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(
			DamageDataTag,
			DamageMagnitude);
	}

	return DamageSpecHandle;
}

FGameplayEffectSpecHandle
UPdGameplayAbility::MakeConfiguredStatusEffectSpec(
	const USkillDefinition* SkillDataAsset,
	const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass,
	const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	const UStatusEffectDefinition* StatusEffectDefinition =
		SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition)
	{
		StatusEffectDefinition->SynchronizeDebuffGameplayEffectStackLimit();
	}
	const TSubclassOf<UGameplayEffect> DebuffGameplayEffectClass =
		StatusEffectDefinition
			? StatusEffectDefinition->DebuffGameplayEffectClass
			: FallbackStatusEffectClass;
	if (!SourceAbilitySystemComponent || !DebuffGameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystemComponent->MakeEffectContext();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	EffectContext.AddInstigator(AvatarActor, AvatarActor);
	EffectContext.AddSourceObject(GetCurrentSourceObject());

	const float StatusEffectLevel = StatusEffectDefinition && SkillDataAsset
		? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
		: FMath::Max(FallbackStatusEffectLevel, 1.0f);
	FGameplayEffectSpecHandle StatusEffectSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(
			DebuffGameplayEffectClass,
			StatusEffectLevel,
			EffectContext);
	if (!StatusEffectSpecHandle.IsValid()
		|| !StatusEffectSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}
	if (SkillDataAsset)
	{
		StatusEffectSpecHandle.Data->SetStackCount(
			FMath::Max(SkillDataAsset->StackCount, 1));
	}

	if (!StatusEffectDefinition)
	{
		return StatusEffectSpecHandle;
	}
	StatusEffectSpecHandle.Data->SetDuration(
		StatusEffectTiming::FullStackLifetimeSeconds,
		true);

	return StatusEffectSpecHandle;
}

FActiveGameplayEffectHandle
UPdGameplayAbility::ApplyConfiguredStatusEffectToTarget(
	const USkillDefinition* SkillDataAsset,
	UAbilitySystemComponent* TargetAbilitySystemComponent,
	const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass,
	const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return FActiveGameplayEffectHandle();
	}
	const UStatusEffectDefinition* StatusEffectDefinition =
		SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition
		&& !StatusEffectDefinition->CanAccumulateDebuffOn(
			TargetAbilitySystemComponent))
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle StatusEffectSpecHandle =
		MakeConfiguredStatusEffectSpec(
			SkillDataAsset,
			FallbackStatusEffectClass,
			FallbackStatusEffectLevel);
	if (!StatusEffectSpecHandle.IsValid()
		|| !StatusEffectSpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
		*StatusEffectSpecHandle.Data.Get(),
		TargetAbilitySystemComponent);
	AActor* TargetActor = TargetAbilitySystemComponent->GetAvatarActor();
	if (AppliedHandle.WasSuccessfullyApplied()
		&& StatusEffectDefinition
		&& TargetActor)
	{
		if (UStatusEffectReplicationComponent* ReplicationComponent =
			TargetActor->FindComponentByClass<UStatusEffectReplicationComponent>())
		{
			ReplicationComponent->TrackAppliedStatusEffect(
				StatusEffectDefinition,
				AppliedHandle);
		}
	}

	return AppliedHandle;
}

void UPdGameplayAbility::StopAvatarMovementForSkillActivation()
{
	MovementRuntime->StopAvatarMovementForSkillActivation(*this);
}

void UPdGameplayAbility::LockAvatarMovementForAbility()
{
	MovementRuntime->LockAvatarMovementForAbility(*this);
}

void UPdGameplayAbility::RestoreAvatarMovementForAbility()
{
	MovementRuntime->RestoreAvatarMovementForAbility(*this);
}

void UPdGameplayAbility::StartDurationMovementLock()
{
	MovementRuntime->StartDurationMovementLock(*this);
}

void UPdGameplayAbility::StopDurationMovementLock()
{
	MovementRuntime->StopDurationMovementLock(*this);
}

void UPdGameplayAbility::StartMovementContactDamage()
{
	MovementRuntime->StartMovementContactDamage(*this);
}

void UPdGameplayAbility::StopMovementContactDamage()
{
	MovementRuntime->StopMovementContactDamage(*this);
}

// 파생 능력이 이 인스턴스의 연출 객체에 시작·갱신·중단을 요청할 수 있게 한다.
UAbilityPresentationRuntime& UPdGameplayAbility::GetPresentationRuntime()
{
	return *PresentationRuntime;
}

// 위치와 지속시간 계산처럼 상태를 바꾸지 않는 연출 조회에 사용한다.
const UAbilityPresentationRuntime& UPdGameplayAbility::GetPresentationRuntime() const
{
	return *PresentationRuntime;
}

// 재시전·종료·ASC 리셋에서 이 능력이 남긴 연출 액터를 공통으로 정리한다.
void UPdGameplayAbility::CleanupConfiguredPresentation()
{
	PresentationRuntime->CleanupConfiguredPresentation();
}

// 자기 버프의 게임 규칙은 능력이 적용한다. 시각효과 객체에는 메시 확대 연출만 맡긴다.
void UPdGameplayAbility::StartConfiguredSelfBuff(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	StopConfiguredSelfBuff();
	const USkillDefinition* SkillDefinition = GetSourceSkillDataAsset();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDefinition || !SkillDefinition->SelfBuff.bEnabled || !ASC || !GetAvatarActorFromActorInfo())
	{
		return;
	}
	const FSkillSelfBuffSettings& Settings = SkillDefinition->SelfBuff;
	PresentationRuntime->ApplySelfBuffCharacterScale(*this, Settings);
	if (Settings.WeaponTraceEndZMultiplier > 1.0)
	{
		if (AWeaponBase* Weapon = GetCurrentWeaponActorFromAvatar())
		{
			Weapon->SetTemporaryAttackTraceEndZMultiplier(this, static_cast<float>(Settings.WeaponTraceEndZMultiplier));
			SelfBuffTraceEndZWeapon = Weapon;
		}
	}
	// 버프 수치는 서버가 확정한다. 외형과 무기 검사 범위의 로컬 처리는 기존 방식대로 유지한다.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
		Combat && Settings.WeaponDamageBonus > 0.0)
	{
		Combat->SetTemporaryWeaponDamageBonus(this, static_cast<float>(Settings.WeaponDamageBonus));
		SelfBuffCombatComponent = Combat;
	}
	if (!Settings.GameplayEffectClass)
	{
		return;
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddSourceObject(SkillDefinition);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Settings.GameplayEffectClass, FMath::Max(GetAbilityLevel(Handle, ActorInfo), 1), Context);
	if (!Spec.IsValid())
	{
		return;
	}
	if (Settings.MagnitudeDataTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Settings.MagnitudeDataTag, static_cast<float>(Settings.Magnitude));
	}
	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	if (AppliedHandle.IsValid() && Settings.bRemoveOnAbilityEnd)
	{
		ActiveSelfBuffEffectHandle = AppliedHandle;
		SelfBuffAbilitySystemComponent = ASC;
	}
}

// 현재 Avatar를 다시 찾지 않고 실제 버프를 적용했던 대상에서 자신의 기여만 제거한다.
void UPdGameplayAbility::StopConfiguredSelfBuff()
{
	PresentationRuntime->RestoreSelfBuffCharacterScale(*this);
	if (AWeaponBase* Weapon = SelfBuffTraceEndZWeapon.Get())
	{
		Weapon->ClearTemporaryAttackTraceEndZMultiplier(this);
	}
	SelfBuffTraceEndZWeapon.Reset();
	if (UCombatComponent* Combat = SelfBuffCombatComponent.Get())
	{
		Combat->ClearTemporaryWeaponDamageBonus(this);
	}
	SelfBuffCombatComponent.Reset();
	if (UAbilitySystemComponent* ASC = SelfBuffAbilitySystemComponent.Get(); ASC && ASC->IsOwnerActorAuthoritative())
	{
		ASC->RemoveActiveGameplayEffect(ActiveSelfBuffEffectHandle);
	}
	ActiveSelfBuffEffectHandle.Invalidate();
	SelfBuffAbilitySystemComponent.Reset();
}

UAbilityTask_PlayMontageAndWait*
UPdGameplayAbility::CreateDefaultMontageAndWaitTask(
	UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay)
	{
		return nullptr;
	}

	return UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			NAME_None,
			MontageToPlay,
			1.0f,
			NAME_None,
			true,
			1.0f,
			0.0f,
			true);
}

UAbilityTask_WaitGameplayEvent*
UPdGameplayAbility::CreateWaitGameplayEventTask(
	const FGameplayTag& EventTag,
	const bool bOnlyTriggerOnce,
	const bool bOnlyMatchExact)
{
	if (!EventTag.IsValid())
	{
		return nullptr;
	}

	return UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		EventTag,
		nullptr,
		bOnlyTriggerOnce,
		bOnlyMatchExact);
}

AGameplayAbilityTargetActor*
UPdGameplayAbility::BeginSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask,
	const TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass)
{
	if (!TargetDataTask || !TargetActorClass)
	{
		return nullptr;
	}

	AGameplayAbilityTargetActor* SpawnedActor = nullptr;
	return TargetDataTask->BeginSpawningActor(
		this,
		TargetActorClass,
		SpawnedActor)
		? SpawnedActor
		: nullptr;
}

void UPdGameplayAbility::FinishSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask,
	AGameplayAbilityTargetActor* SpawnedActor)
{
	if (TargetDataTask && SpawnedActor)
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedActor);
	}
}

int32 UPdGameplayAbility::GrantAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const int32 AbilityLevel)
{
	UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent
		|| AbilityClasses.IsEmpty()
		|| !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return 0;
	}

	return AbilitySystemComponent->GrantAbilities(
		AbilityClasses,
		AbilityLevel,
		GetAvatarActorFromActorInfo()).Num();
}

bool UPdGameplayAbility::ApplyGameplayEffect(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const float EffectLevel,
	const int32 StackCount)
{
	return ApplyGameplayEffectHandle(
		GameplayEffectClass,
		EffectLevel,
		StackCount).WasSuccessfullyApplied();
}

FActiveGameplayEffectHandle
UPdGameplayAbility::ApplyGameplayEffectHandle(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const float EffectLevel,
	const int32 StackCount)
{
	const FGameplayTagContainer DynamicGrantedTags;
	return ApplyGameplayEffectHandle(
		GameplayEffectClass,
		DynamicGrantedTags,
		EffectLevel,
		StackCount);
}

FActiveGameplayEffectHandle
UPdGameplayAbility::ApplyGameplayEffectHandle(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTagContainer& DynamicGrantedTags,
	const float EffectLevel,
	const int32 StackCount)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent
		|| !GameplayEffectClass
		|| !ActorInfo
		|| !HasAuthorityOrPredictionKey(
			ActorInfo,
			&CurrentActivationInfo))
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingGameplayEffectSpec(
			GameplayEffectClass,
			FMath::Max(EffectLevel, 1.0f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	SpecHandle.Data->DynamicGrantedTags.AppendTags(DynamicGrantedTags);
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, SpecHandle);
}

bool UPdGameplayAbility::HasActiveGameplayEffect(
	TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass)
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return !AbilitySystemComponent->GetActiveEffects(Query).IsEmpty();
}

bool UPdGameplayAbility::RemoveGameplayEffect(
	TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent
		|| !GameplayEffectClass
		|| !ActorInfo
		|| !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return AbilitySystemComponent->RemoveActiveEffects(Query) > 0;
}

int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(
	const FGameplayTagContainer& GrantedTags)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent
		|| GrantedTags.IsEmpty()
		|| !ActorInfo
		|| !HasAuthority(&CurrentActivationInfo))
	{
		return 0;
	}

	return AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(
		GrantedTags);
}

// 숨겨진 판도라는 새 시전을 시작할 수 없다. 이미 실행 중인 인스턴스의 지속 처리는 이 검사와 무관하다.
bool UPdGameplayAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		const UPandoraSkillRuntimeContext* Source = Cast<UPandoraSkillRuntimeContext>(Spec->SourceObject.Get());
		if (!Source || !Source->IsSourceReady() || !Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Pandora_Selected))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

float UPdGameplayAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	float Remaining = 0.0f;
	float Duration = 0.0f;
	GetCooldownTimeRemainingAndDuration(GetCurrentAbilitySpecHandle(), ActorInfo, Remaining, Duration);
	return Remaining;
}

void UPdGameplayAbility::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPandoraSkillRuntimeContext* Source = Spec ? Cast<UPandoraSkillRuntimeContext>(Spec->SourceObject.Get()) : nullptr;
	if (Source)
	{
		UAbilityResourceRuntime::GetPandoraCooldown(*ASC, *Source, TimeRemaining, CooldownDuration);
		return;
	}
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		TimeRemaining = 0.0f;
		CooldownDuration = 0.0f;
		return;
	}
	Super::GetCooldownTimeRemainingAndDuration(Handle, ActorInfo, TimeRemaining, CooldownDuration);
}
