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
#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedAttackAbility)

namespace
{
float CalculateRangedWeaponAttackSpeedPlayRate(const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;

	const float AttackSpeedPercent = AttributeSet ? FMath::Max(AttributeSet->GetAttackSpeed(), 0.0f) : 0.0f;
	return FMath::Max(0.01f, 1.0f + AttackSpeedPercent * 0.01f);
}
}

URangedAttackAbility::URangedAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRetriggerInstancedAbility = true;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::Action_RangedAttack);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Movement_Airborne);

	AttackTraceStartEventTag = FGameplayTag::RequestGameplayTag(TEXT("Notifier.Attack.ComboInputOpen"), false);
	AttackTraceEndEventTag = FGameplayTag::RequestGameplayTag(TEXT("Notifier.Attack.ComboInputClose"), false);
}

bool URangedAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const ACharacterBase* Character = ActorInfo
		? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	if (MovementComponent && MovementComponent->IsFalling())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(LabGameplayTags::State_Movement_Airborne);
		}
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
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
void URangedAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanupAttackState();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
		UAbilityTask_WaitGameplayEvent* AttackTraceStartTask =
			CreateWaitGameplayEventTask(AttackTraceStartEventTag);
		if (ensure(AttackTraceStartTask))
		{
			AttackTraceStartTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceStart);
			AttackTraceStartTask->ReadyForActivation();
		}
	}

	if (AttackTraceEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackTraceEndTask =
			CreateWaitGameplayEventTask(AttackTraceEndEventTag);
		if (ensure(AttackTraceEndTask))
		{
			AttackTraceEndTask->EventReceived.AddDynamic(this, &URangedAttackAbility::OnAttackTraceEnd);
			AttackTraceEndTask->ReadyForActivation();
		}
	}

	const float AttackSpeedPlayRate = CalculateRangedWeaponAttackSpeedPlayRate(ActorInfo);
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData.AttackMontage,
		AttackSpeedPlayRate,
		NAME_None,
		false,
		1.f,
		0.f,
		false);
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
	return Character
		&& !Character->IsPlayerControlled()
		&& CurrentWeapon
		&& CurrentWeapon->SupportsAimInput()
		&& HasAuthority(&CurrentActivationInfo);
}

bool URangedAttackAbility::TryCacheAIPrimaryAttackTarget(ACharacterBase* Character)
{
	if (!Character)
	{
		return false;
	}

	AActor* AttackTarget = ResolveAttackTarget(Character);
	const FVector TargetLocation = ResolveAITargetAimLocation(AttackTarget);
	if (!IsValid(AttackTarget) || TargetLocation.IsNearlyZero())
	{

		return false;
	}

	CachedAIPrimaryAttackTargetActor = AttackTarget;
	CachedAIPrimaryAttackTargetLocation = TargetLocation;
	bHasCachedAIPrimaryAttackTargetLocation = true;
	FaceCharacterToTargetLocation(Character, CachedAIPrimaryAttackTargetLocation);


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
	FaceCharacterToTargetLocation(Character, CachedAIPrimaryAttackTargetLocation);

	const bool bFired = CurrentWeapon->HandleAIPrimaryAttackAtLocation(Character, AttackTarget, CachedAIPrimaryAttackTargetLocation);
	bAIPrimaryAttackExecuted = bFired;
	const float TargetLockDelay = GetAIRangedTargetLockDelay();



	return bFired;
}

FVector URangedAttackAbility::ResolveAITargetAimLocation(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	TargetActor->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);

	FVector AimLocation = TargetActor->GetActorLocation();
	AimLocation.Z += FMath::Max(TargetHalfHeight * 0.5f, 0.0f);
	return AimLocation;
}

void URangedAttackAbility::FaceCharacterToTargetLocation(ACharacterBase* Character, const FVector& TargetLocation) const
{
	if (!Character || TargetLocation.IsNearlyZero())
	{
		return;
	}
	if (Character->IsStatusFrozen())
	{

		return;
	}

	const FVector ToTarget = TargetLocation - Character->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	FRotator LookAtRotation = ToTarget.Rotation();
	LookAtRotation.Pitch = 0.0f;
	LookAtRotation.Roll = 0.0f;

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(LookAtRotation);
	}
	Character->SetActorRotation(LookAtRotation);
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
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleAIPrimaryAttackTimer);
	}
	else
	{
		World->GetTimerManager().SetTimer(
			AIPrimaryAttackTimerHandle,
			this,
			&ThisClass::HandleAIPrimaryAttackTimer,
			SafeDelay,
			false);
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
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->SetBeginOverlapEnabled(bEnabled);
}
