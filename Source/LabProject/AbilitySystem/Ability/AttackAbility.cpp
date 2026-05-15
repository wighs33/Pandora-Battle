#include "AbilitySystem/Ability/AttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdCharacterBase.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AttackAbility)

UAttackAbility::UAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// State helpers
void UAttackAbility::CleanupAttackState()
{
	SetCurrentWeaponBeginOverlapEnabled(false);
	ResetAttackInputState();

	if (AttackingEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(AttackingEffectClass);
	}
}

// Timing callbacks
void UAttackAbility::OnAttackMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAttackAbility::OnAttackMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::OnAttackMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::OnJumpSectionTiming(FGameplayEventData Payload)
{
	if (Payload.EventMagnitude > 0.f)
	{
		RequestJumpToSection(GetNextAttackSectionName());
		return;
	}

	bReachedJumpSectionTiming = true;
	if (!BufferedJumpSectionName.IsNone())
	{
		TryJumpToSection(BufferedJumpSectionName);
	}
}

void UAttackAbility::OnAttackInputWindowOpened(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	SetCurrentWeaponBeginOverlapEnabled(true);
	bCanReceiveAttackInput = true;
	bReachedJumpSectionTiming = false;
	BufferedJumpSectionName = NAME_None;
}

void UAttackAbility::OnAttackInputWindowClosed(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	SetCurrentWeaponBeginOverlapEnabled(false);
	ResetAttackInputState();
}

// Ability flow
void UAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanupAttackState();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

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
			AttackInputWindowStartedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnAttackInputWindowOpened);
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
			AttackInputWindowEndedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnAttackInputWindowClosed);
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
			JumpSectionEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnJumpSectionTiming);
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

	MontageTask->OnCompleted.AddDynamic(this, &UAttackAbility::OnAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UAttackAbility::OnAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAttackAbility::OnAttackMontageCancelled);

	if (AttackingEffectClass && !HasActiveGameplayEffect(AttackingEffectClass))
	{
		ApplyGameplayEffect(AttackingEffectClass, 1.f, 1);
	}

	MontageTask->ReadyForActivation();
}

// Query helpers
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
	const int32 NumSections = CurrentAttackMontage->GetNumSections();
	const int32 NextSectionIndex = CurrentSectionIndex + 1;
	if (CurrentSectionIndex == INDEX_NONE || NumSections <= 0)
	{
		return NAME_None;
	}

	if (NextSectionIndex >= NumSections)
	{
		return CurrentAttackMontage->GetSectionName(0);
	}

	return CurrentAttackMontage->GetSectionName(NextSectionIndex);
}

// Public requests
bool UAttackAbility::RequestJumpToSection(FName RequestedSectionName)
{
	if (!bCanReceiveAttackInput || RequestedSectionName.IsNone() || !IsAttackSectionNameValid(RequestedSectionName))
	{
		return false;
	}

	if (bReachedJumpSectionTiming)
	{
		return TryJumpToSection(RequestedSectionName);
	}

	BufferedJumpSectionName = RequestedSectionName;
	return true;
}

// State helpers
void UAttackAbility::ResetAttackInputState()
{
	bCanReceiveAttackInput = false;
	bReachedJumpSectionTiming = false;
	BufferedJumpSectionName = NAME_None;
}

AWeaponBase* UAttackAbility::GetCurrentWeaponActor() const
{
	const APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

// Action helpers
void UAttackAbility::SetCurrentWeaponBeginOverlapEnabled(bool bEnabled) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (!CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->SetBeginOverlapEnabled(bEnabled);
}

// Query helpers
bool UAttackAbility::IsAttackSectionNameValid(FName SectionName) const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	return CurrentAttackMontage
		&& !SectionName.IsNone()
		&& CurrentAttackMontage->GetSectionIndex(SectionName) != INDEX_NONE;
}

// Action helpers
bool UAttackAbility::TryJumpToSection(FName SectionName)
{
	if (!IsAttackSectionNameValid(SectionName))
	{
		ResetAttackInputState();
		return false;
	}

	MontageJumpToSection(SectionName);
	ResetAttackInputState();
	return true;
}

bool UAttackAbility::TryJumpToNextSection()
{
	return TryJumpToSection(GetNextAttackSectionName());
}
