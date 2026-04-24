#include "Ability/AttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Equipment/EquipmentComponent.h"
#include "Mode/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AttackAbility)

UAttackAbility::UAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAttackAbility::ClearActiveAttackEffect()
{
	if (ActiveAttackEffectHandle.IsValid() && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(ActiveAttackEffectHandle);
	}

	ActiveAttackEffectHandle.Invalidate();
}

void UAttackAbility::HandleAttackMontageCompleted()
{
	ResetAttackInputState();
	ClearActiveAttackEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAttackAbility::HandleAttackMontageInterrupted()
{
	ResetAttackInputState();
	ClearActiveAttackEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::HandleAttackMontageCancelled()
{
	ResetAttackInputState();
	ClearActiveAttackEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::HandleJumpSectionEvent(FGameplayEventData Payload)
{
	const bool bIsJumpSectionRequest = Payload.EventMagnitude > 0.f;

	if (bIsJumpSectionRequest)
	{
		if (!bCanReceiveAttackInput)
		{
			return;
		}

		if (bReachedJumpSectionTiming)
		{
			TryJumpToNextSection();
			return;
		}

		bBufferedJumpSectionRequest = true;
		return;
	}

	bReachedJumpSectionTiming = true;
	if (bBufferedJumpSectionRequest)
	{
		TryJumpToNextSection();
	}
}

void UAttackAbility::HandleAttackInputWindowStartedEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	bCanReceiveAttackInput = true;
	bReachedJumpSectionTiming = false;
	bBufferedJumpSectionRequest = false;
}

void UAttackAbility::HandleAttackInputWindowEndedEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	ResetAttackInputState();
}

void UAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!ensure(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent();
	if (!ensure(EquipmentComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FAttackData AttackData;
	if (!ensure(EquipmentComponent->GetAttackData(AttackData)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResetAttackInputState();

	if (AttackInputWindowStartEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackInputWindowStartedEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			AttackInputWindowStartEventTag,
			nullptr,
			false,
			true);
		if (ensure(AttackInputWindowStartedEventTask))
		{
			AttackInputWindowStartedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::HandleAttackInputWindowStartedEvent);
			AttackInputWindowStartedEventTask->ReadyForActivation();
		}
	}

	if (AttackInputWindowEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackInputWindowEndedEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			AttackInputWindowEndEventTag,
			nullptr,
			false,
			true);
		if (ensure(AttackInputWindowEndedEventTask))
		{
			AttackInputWindowEndedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::HandleAttackInputWindowEndedEvent);
			AttackInputWindowEndedEventTask->ReadyForActivation();
		}
	}

	if (JumpSectionEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* JumpSectionEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			JumpSectionEventTag,
			nullptr,
			false,
			true);
		if (ensure(JumpSectionEventTask))
		{
			JumpSectionEventTask->EventReceived.AddDynamic(this, &UAttackAbility::HandleJumpSectionEvent);
			JumpSectionEventTask->ReadyForActivation();
		}
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData.AttackMontage,
		1.f,
		NAME_None,
		false,
		1.f,
		0.f,
		false);
	if (!ensure(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAttackAbility::HandleAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UAttackAbility::HandleAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAttackAbility::HandleAttackMontageCancelled);
	MontageTask->ReadyForActivation();

	ActiveAttackEffectHandle.Invalidate();
	if (AttackData.AttackEffectClass)
	{
		ActiveAttackEffectHandle = ApplyGameplayEffectHandle(AttackData.AttackEffectClass, 1.f, 1);
	}
}

FName UAttackAbility::GetCurrentAttackSectionName() const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	if (!CurrentAttackMontage)
	{
		return NAME_None;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return NAME_None;
	}

	return AnimInstance->Montage_GetCurrentSection(CurrentAttackMontage);
}

FName UAttackAbility::GetNextAttackSectionName() const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	if (!CurrentAttackMontage)
	{
		return NAME_None;
	}

	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (CurrentSectionName.IsNone())
	{
		return NAME_None;
	}

	const int32 CurrentSectionIndex = CurrentAttackMontage->GetSectionIndex(CurrentSectionName);
	const int32 NextSectionIndex = CurrentSectionIndex + 1;
	if (CurrentSectionIndex == INDEX_NONE || NextSectionIndex >= CurrentAttackMontage->GetNumSections())
	{
		return NAME_None;
	}

	return CurrentAttackMontage->GetSectionName(NextSectionIndex);
}

void UAttackAbility::ResetAttackInputState()
{
	bCanReceiveAttackInput = false;
	bReachedJumpSectionTiming = false;
	bBufferedJumpSectionRequest = false;
}

bool UAttackAbility::TryJumpToNextSection()
{
	const FName NextSectionName = GetNextAttackSectionName();
	if (NextSectionName.IsNone())
	{
		ResetAttackInputState();
		return false;
	}

	MontageJumpToSection(NextSectionName);
	ResetAttackInputState();
	return true;
}
