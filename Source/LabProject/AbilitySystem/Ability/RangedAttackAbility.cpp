#include "AbilitySystem/Ability/RangedAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Definition/Item/ItemDefinition.h"
#include "Component/Player/EquipmentComponent.h"
#include "Interface/TargetingInterface.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/RangedWeaponBase.h"
#include "Weapon/MeleeWeapon.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedAttackAbility)

URangedAttackAbility::URangedAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRetriggerInstancedAbility = true;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::Action_RangedAttack);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Movement_Airborne);
	bRequiresGroundedAvatar = true;

	AttackTraceStartEventTag = LabGameplayTags::Notifier_Attack_ComboInputOpen;
	AttackTraceEndEventTag = LabGameplayTags::Notifier_Attack_ComboInputClose;
}

// State helpers
void URangedAttackAbility::CleanupAttackState()
{
	ClearAIPrimaryAttackTimer();
	bAIPrimaryAttackExecuted = false;
	SetCurrentWeaponTraceEnabled(false);

	if (AttackingEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(AttackingEffectClass);
	}
}

// Timing callbacks
void URangedAttackAbility::OnAttackMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void URangedAttackAbility::OnAttackMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URangedAttackAbility::OnAttackMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void URangedAttackAbility::OnAttackTraceStart(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (ShouldUseAIWeaponFire(Character, CurrentWeapon))
	{
		return;
	}

	SetCurrentWeaponTraceEnabled(true);
}

void URangedAttackAbility::OnAttackTraceEnd(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	SetCurrentWeaponTraceEnabled(false);
}

// Ability flow
void URangedAttackAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	CleanupAttackState();
}

void URangedAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);
	bAIPrimaryAttackExecuted = false;
	ClearAIPrimaryAttackTimer();

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FAttackData AttackData;
	if (!EquipmentComponent->GetAttackData(AttackData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (AttackTraceStartEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackTraceStartTask = CreateWaitGameplayEventTask(AttackTraceStartEventTag);
		if (ensure(AttackTraceStartTask))
		{
			AttackTraceStartTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceStart);
			AttackTraceStartTask->ReadyForActivation();
		}
	}

	if (AttackTraceEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackTraceEndTask = CreateWaitGameplayEventTask(AttackTraceEndEventTag);
		if (ensure(AttackTraceEndTask))
		{
			AttackTraceEndTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceEnd);
			AttackTraceEndTask->ReadyForActivation();
		}
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = CreateWeaponAttackMontageTask(AttackData.AttackMontage);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &URangedAttackAbility::OnAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &URangedAttackAbility::OnAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &URangedAttackAbility::OnAttackMontageCancelled);

	if (AttackingEffectClass && !HasActiveGameplayEffect(AttackingEffectClass))
	{
		ApplyGameplayEffect(AttackingEffectClass, 1.f, 1);
	}

	MontageTask->ReadyForActivation();
	ScheduleAIPrimaryAttack();
}

AWeaponBase* URangedAttackAbility::GetCurrentWeaponActor() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

AActor* URangedAttackAbility::ResolveAttackTarget(ACharacterBase* Character) const
{
	if (!Character || !Character->GetClass()->ImplementsInterface(UTargetingInterface::StaticClass()))
	{
		return nullptr;
	}

	return ITargetingInterface::Execute_GetAttackTarget(Character);
}

bool URangedAttackAbility::ShouldUseAIWeaponFire(ACharacterBase* Character, AWeaponBase* CurrentWeapon) const
{
	const ARangedWeaponBase* RangedWeapon = Cast<ARangedWeaponBase>(CurrentWeapon);
	return Character && !Character->IsPlayerControlled() && RangedWeapon && RangedWeapon->SupportsAimInput()
		&& HasAuthority(&CurrentActivationInfo);
}

bool URangedAttackAbility::TryCacheAIPrimaryAttackTarget(ACharacterBase* Character)
{
	if (!Character)
	{
		return false;
	}

	AActor* AttackTarget = ResolveAttackTarget(Character);
	const FVector TargetLocation = ARangedWeaponBase::GetAITargetAimLocation(AttackTarget);
	if (!IsValid(AttackTarget) || TargetLocation.IsNearlyZero())
	{
		return false;
	}

	CachedAIPrimaryAttackTargetActor = AttackTarget;
	CachedAIPrimaryAttackTargetLocation = TargetLocation;
	bHasCachedAIPrimaryAttackTargetLocation = true;
	FaceCharacterToward(Character, CachedAIPrimaryAttackTargetLocation);

	return true;
}

bool URangedAttackAbility::TryExecuteScheduledAIWeaponFire()
{
	if (bAIPrimaryAttackExecuted)
	{
		return true;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (!ShouldUseAIWeaponFire(Character, CurrentWeapon))
	{
		return false;
	}

	if (!bHasCachedAIPrimaryAttackTargetLocation || CachedAIPrimaryAttackTargetLocation.IsNearlyZero())
	{
		return false;
	}

	AActor* AttackTarget = CachedAIPrimaryAttackTargetActor.Get();
	FaceCharacterToward(Character, CachedAIPrimaryAttackTargetLocation);

	const bool bFired = CurrentWeapon->HandleAIPrimaryAttackAtLocation(Character, AttackTarget, CachedAIPrimaryAttackTargetLocation);
	bAIPrimaryAttackExecuted = bFired;

	return bFired;
}

float URangedAttackAbility::GetAIRangedTargetLockDelay() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	const UItemDefinition* ItemDefinition = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
	if (!ItemDefinition)
	{
		return FWeaponAIDefinitionData().RangedTargetLockDelay;
	}

	return ItemDefinition->WeaponData.AI.RangedTargetLockDelay;
}

void URangedAttackAbility::ScheduleAIPrimaryAttack()
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (!ShouldUseAIWeaponFire(Character, CurrentWeapon))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!TryCacheAIPrimaryAttackTarget(Character))
	{
		return;
	}

	const float SafeDelay = FMath::Max(GetAIRangedTargetLockDelay(), 0.0f);
	if (SafeDelay <= 0.0f)
	{
		// 지연이 없을 때의 다음 틱 예약도 같은 핸들에 담아, 능력이 먼저 끝나면 ClearAIPrimaryAttackTimer가 함께 취소한다.
		AIPrimaryAttackTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleAIPrimaryAttackTimer);
	}
	else
	{
		World->GetTimerManager().SetTimer(AIPrimaryAttackTimerHandle, this, &ThisClass::HandleAIPrimaryAttackTimer,
			SafeDelay, false);
	}
}

void URangedAttackAbility::ClearAIPrimaryAttackTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AIPrimaryAttackTimerHandle);
	}

	bHasCachedAIPrimaryAttackTargetLocation = false;
	CachedAIPrimaryAttackTargetLocation = FVector::ZeroVector;
	CachedAIPrimaryAttackTargetActor.Reset();
}

void URangedAttackAbility::HandleAIPrimaryAttackTimer()
{
	TryExecuteScheduledAIWeaponFire();
}

void URangedAttackAbility::SetCurrentWeaponTraceEnabled(bool bEnabled) const
{
	AMeleeWeapon* CurrentWeapon = Cast<AMeleeWeapon>(GetCurrentWeaponActor());
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->SetAttackTraceEnabled(bEnabled);
}
