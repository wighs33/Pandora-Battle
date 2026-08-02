#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/PdAbilityMovementRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilityPresentationRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilityResourceRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilitySourceRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"

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

UPdGameplayAbility::UPdGameplayAbility(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy =
		EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy =
		EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(
		LabGameplayTags::GameplayAbility_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Dead);
	CooldownRemovalPolicyTags.AddTag(
		LabGameplayTags::Effect_Policy_RemoveOnDeath);

	ResourceRuntime =
		ObjectInitializer.CreateDefaultSubobject<
			UPdAbilityResourceRuntime>(
			this,
			TEXT("ResourceRuntime"));
	SourceRuntime =
		ObjectInitializer.CreateDefaultSubobject<UPdAbilitySourceRuntime>(
			this,
			TEXT("SourceRuntime"));
	MovementRuntime =
		ObjectInitializer.CreateDefaultSubobject<
			UPdAbilityMovementRuntime>(
			this,
			TEXT("MovementRuntime"));
	PresentationRuntime =
		ObjectInitializer.CreateDefaultSubobject<
			UPdAbilityPresentationRuntime>(
			this,
			TEXT("PresentationRuntime"));
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
	FGameplayEffectSpecHandle& CooldownSpecHandle,
	const bool bPandoraCooldown) const
{
	if (ResourceRuntime)
	{
		ResourceRuntime->AppendCooldownRemovalPolicyTags(
			CooldownSpecHandle,
			CooldownRemovalPolicyTags,
			bPandoraCooldown);
	}
}

void UPdGameplayAbility::SuppressPendingCooldownForRuntimeReset() const
{
	if (ResourceRuntime)
	{
		ResourceRuntime->SuppressPendingCooldown();
	}
}

void UPdGameplayAbility::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
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
	const USkillDefinition* SkillDefinition = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			*this,
			AbilitySpec,
			ActorInfo)
		: nullptr;
	const bool bInputDrivenSkill = SkillDefinition
		&& (SkillDefinition->SkillType == EPdSkillType::Instant
			|| SkillDefinition->SkillType == EPdSkillType::Press
			|| SkillDefinition->SkillType == EPdSkillType::Duration);
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
	const FGameplayTagContainer* ParentCooldownTags =
		Super::GetCooldownTags();
	return ResourceRuntime
		? ResourceRuntime->BuildCooldownTags(
			*this,
			ParentCooldownTags)
		: ParentCooldownTags;
}

bool UPdGameplayAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	return !ResourceRuntime
		|| ResourceRuntime->CheckCost(
			*this,
			Handle,
			ActorInfo,
			OptionalRelevantTags);
}

void UPdGameplayAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	if (ResourceRuntime)
	{
		ResourceRuntime->ApplyCost(
			*this,
			Handle,
			ActorInfo,
			ActivationInfo);
	}
}

bool UPdGameplayAbility::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ResourceRuntime)
	{
		bool bHandled = false;
		const bool bConfiguredResult =
			ResourceRuntime->CheckConfiguredCooldown(
				*this,
				Handle,
				ActorInfo,
				Super::GetCooldownTags(),
				OptionalRelevantTags,
				bHandled);
		if (bHandled)
		{
			return bConfiguredResult;
		}
	}

	return Super::CheckCooldown(
		Handle,
		ActorInfo,
		OptionalRelevantTags);
}

void UPdGameplayAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (ResourceRuntime
		&& ResourceRuntime->ShouldDeferCooldown(
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
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(*this)
				: nullptr);
	const USkillDefinition* SkillDefinition = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			*this,
			AbilitySpec,
			ActorInfo)
		: nullptr;
	if (SkillDefinition)
	{
		StopAvatarMovementForSkillActivation();

		// Once a protected skill has paid its cost, unrelated abilities and
		// input-release events must not leave it in a cooldown-only state.
		// Death/runtime reset explicitly restores cancellability before cleanup,
		// while bCancelOnHit keeps the authored interruptible-skill behavior.
		if (!SkillDefinition->bCancelOnHit)
		{
			SetCanBeCanceled(false);
		}
	}

	StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
	return true;
}

void UPdGameplayAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
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
	const bool bCooldownWasPending = ResourceRuntime
		&& ResourceRuntime->ConsumePendingCooldown(bWasCancelled);
	const bool bShouldApplySkillCooldown =
		bCooldownWasPending
		&& !(AbilitySystemComponent
			&& AbilitySystemComponent->IsResettingAbilityRuntimeState());
	if (bShouldApplySkillCooldown)
	{
		ApplyCooldownImmediately(Handle, ActorInfo, ActivationInfo);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);

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
				// A skill or hit reaction may have interrupted an equipment
				// montage after it changed the linked animation layer. Reassert
				// the authoritative current weapon layer even when the replicated
				// weapon pointer itself did not change and no OnRep will run.
				EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
			}
		}

		if (Character)
		{
			Character->ReapplyCurrentRotationPolicy();
		}
	}
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
	// Committed skills are protected from external cancellation. A genuine
	// payload failure must still be able to terminate as cancelled so deferred
	// cooldown is discarded instead of charging for an execution that failed.
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
	const bool bHandled = ResourceRuntime
		&& ResourceRuntime->ApplyConfiguredCooldownImmediately(
			*this,
			Handle,
			ActorInfo,
			ActivationInfo,
			CooldownRemovalPolicyTags);
	if (!bHandled)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
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

USkillDefinition* UPdGameplayAbility::GetSourceSkillDataAsset() const
{
	return SourceRuntime
		? SourceRuntime->GetSourceSkillDataAsset(*this)
		: nullptr;
}

UPandoraSkillRuntimeContext*
UPdGameplayAbility::GetSourceSkillRuntimeContext() const
{
	return SourceRuntime
		? SourceRuntime->GetSourceSkillRuntimeContext(*this)
		: nullptr;
}

TArray<FProjectileImpactEffectAreaSpawnConfig>
UPdGameplayAbility::GetSourceProjectileImpactEffectAreas() const
{
	return SourceRuntime
		? SourceRuntime->GetSourceProjectileImpactEffectAreas(*this)
		: TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

UObject* UPdGameplayAbility::GetCurrentAbilitySpecSourceObject() const
{
	return SourceRuntime
		? SourceRuntime->GetCurrentAbilitySpecSourceObject(*this)
		: nullptr;
}

AWeaponBase* UPdGameplayAbility::GetCurrentWeaponActorFromAvatar() const
{
	return SourceRuntime
		? SourceRuntime->GetCurrentWeaponActorFromAvatar(*this)
		: nullptr;
}

bool UPdGameplayAbility::HasCurrentWeaponSkillTrail() const
{
	return SourceRuntime
		&& SourceRuntime->HasCurrentWeaponSkillTrail(*this);
}

bool UPdGameplayAbility::StartCurrentWeaponSkillTrail(
	UNiagaraSystem* TrailSystem) const
{
	return SourceRuntime
		&& SourceRuntime->StartCurrentWeaponSkillTrail(
			*this,
			TrailSystem);
}

void UPdGameplayAbility::StopCurrentWeaponSkillTrail() const
{
	if (SourceRuntime)
	{
		SourceRuntime->StopCurrentWeaponSkillTrail(*this);
	}
}

bool UPdGameplayAbility::TryCommitAdditionalActionStaminaCost() const
{
	return !ResourceRuntime
		|| ResourceRuntime->TryCommitAdditionalActionStaminaCost(*this);
}

float UPdGameplayAbility::CalculateBaseSkillDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	return SourceRuntime
		? SourceRuntime->CalculateBaseSkillDamageMagnitude(DamageConfig)
		: 0.0f;
}

float UPdGameplayAbility::ApplyIntelligenceToSkillDamage(
	const float DamageMagnitude) const
{
	return SourceRuntime
		? SourceRuntime->ApplyIntelligenceToSkillDamage(
			*this,
			DamageMagnitude)
		: FMath::Max(DamageMagnitude, 0.0f);
}

float UPdGameplayAbility::CalculateSkillDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	return SourceRuntime
		? SourceRuntime->CalculateSkillDamageMagnitude(
			*this,
			DamageConfig)
		: 0.0f;
}

FGameplayEffectSpecHandle
UPdGameplayAbility::MakeConfiguredDamageEffectSpec(
	const FSkillGameplayEffectConfig& DamageConfig,
	const float DamageMagnitude,
	UObject* SourceObject) const
{
	return SourceRuntime
		? SourceRuntime->MakeConfiguredDamageEffectSpec(
			*this,
			DamageConfig,
			DamageMagnitude,
			SourceObject)
		: FGameplayEffectSpecHandle();
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
	const TSubclassOf<UGameplayEffect> StatusEffectClass =
		StatusEffectDefinition && StatusEffectDefinition->StatusEffectClass
			? StatusEffectDefinition->StatusEffectClass
			: FallbackStatusEffectClass;
	if (!SourceAbilitySystemComponent || !StatusEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystemComponent->MakeEffectContext();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	EffectContext.AddInstigator(AvatarActor, AvatarActor);
	if (const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec())
	{
		if (UObject* SourceObject = AbilitySpec->SourceObject.Get())
		{
			EffectContext.AddSourceObject(SourceObject);
		}
	}

	const float StatusEffectLevel = StatusEffectDefinition && SkillDataAsset
		? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
		: FMath::Max(FallbackStatusEffectLevel, 1.0f);
	FGameplayEffectSpecHandle StatusEffectSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(
			StatusEffectClass,
			StatusEffectLevel,
			EffectContext);
	if (!StatusEffectSpecHandle.IsValid()
		|| !StatusEffectSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	if (!StatusEffectDefinition)
	{
		return StatusEffectSpecHandle;
	}

	const float StatusEffectDuration =
		FMath::Max(StatusEffectDefinition->StatusDuration, 0.0f);
	if (StatusEffectDuration > 0.0f)
	{
		StatusEffectSpecHandle = UAbilitySystemBlueprintLibrary::SetDuration(
			StatusEffectSpecHandle,
			StatusEffectDuration);
	}

	FSkillGameplayEffectConfig StatusDamageConfig;
	StatusDamageConfig.Magnitude =
		StatusEffectDefinition->ResolveDamageMagnitude();
	StatusEffectDefinition->SetDamageMagnitude(
		StatusEffectSpecHandle,
		SourceAbilitySystemComponent,
		CalculateSkillDamageMagnitude(StatusDamageConfig));
	StatusEffectDefinition->AppendRemovalPolicyTags(StatusEffectSpecHandle);

	if (StatusEffectDefinition->StatusEffectTag.IsValid())
	{
		StatusEffectSpecHandle.Data->DynamicGrantedTags.AddTag(
			StatusEffectDefinition->StatusEffectTag);
	}

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

	return SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
		*StatusEffectSpecHandle.Data.Get(),
		TargetAbilitySystemComponent);
}

void UPdGameplayAbility::StopAvatarMovementForSkillActivation()
{
	if (MovementRuntime)
	{
		MovementRuntime->StopAvatarMovementForSkillActivation(*this);
	}
}

void UPdGameplayAbility::LockAvatarMovementForAbility()
{
	if (MovementRuntime)
	{
		MovementRuntime->LockAvatarMovementForAbility(*this);
	}
}

void UPdGameplayAbility::RestoreAvatarMovementForAbility()
{
	if (MovementRuntime)
	{
		MovementRuntime->RestoreAvatarMovementForAbility(*this);
	}
}

void UPdGameplayAbility::StartDurationMovementLock()
{
	if (MovementRuntime)
	{
		MovementRuntime->StartDurationMovementLock(*this);
	}
}

void UPdGameplayAbility::StopDurationMovementLock()
{
	if (MovementRuntime)
	{
		MovementRuntime->StopDurationMovementLock(*this);
	}
}

void UPdGameplayAbility::StartMovementContactDamage()
{
	if (MovementRuntime)
	{
		MovementRuntime->StartMovementContactDamage(*this);
	}
}

void UPdGameplayAbility::StopMovementContactDamage()
{
	if (MovementRuntime)
	{
		MovementRuntime->StopMovementContactDamage(*this);
	}
}

void UPdGameplayAbility::StartConfiguredDefaultFX()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StartConfiguredDefaultFX(*this);
	}
}

void UPdGameplayAbility::StopConfiguredDefaultFX()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StopConfiguredDefaultFX(*this);
	}
}

void UPdGameplayAbility::StartConfiguredCharacterOverlay()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StartConfiguredCharacterOverlay(*this);
	}
}

void UPdGameplayAbility::StopConfiguredCharacterOverlay()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StopConfiguredCharacterOverlay(*this);
	}
}

void UPdGameplayAbility::StartConfiguredMissilePresentation(
	const FVector& TargetLocation)
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StartConfiguredMissilePresentation(
			*this,
			TargetLocation);
	}
}

void UPdGameplayAbility::UpdateConfiguredMissilePresentationTarget(
	const FVector& TargetLocation)
{
	if (PresentationRuntime)
	{
		PresentationRuntime
			->UpdateConfiguredMissilePresentationTarget(TargetLocation);
	}
}

void UPdGameplayAbility::StopConfiguredMissilePresentation()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StopConfiguredMissilePresentation(*this);
	}
}

void UPdGameplayAbility::CleanupConfiguredPresentation()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->CleanupConfiguredPresentation();
	}
}

void UPdGameplayAbility::StartConfiguredSelfBuff(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StartConfiguredSelfBuff(
			*this,
			Handle,
			ActorInfo,
			ActivationInfo);
	}
}

void UPdGameplayAbility::StopConfiguredSelfBuff()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->StopConfiguredSelfBuff(*this);
	}
}

void UPdGameplayAbility::SpawnConfiguredCharacterDecal()
{
	if (PresentationRuntime)
	{
		PresentationRuntime->SpawnConfiguredCharacterDecal(*this);
	}
}

FVector UPdGameplayAbility::ResolveConfiguredCharacterDecalLocation(
	const ACharacterBase* Character) const
{
	return PresentationRuntime
		? PresentationRuntime->ResolveConfiguredCharacterDecalLocation(
			Character)
		: FVector::ZeroVector;
}

float UPdGameplayAbility::ResolveConfiguredCharacterDecalDuration(
	const USkillDefinition* SkillDataAsset) const
{
	return PresentationRuntime
		? PresentationRuntime->ResolveConfiguredCharacterDecalDuration(
			SkillDataAsset)
		: 2.0f;
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
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*SpecHandle.Data.Get());
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
