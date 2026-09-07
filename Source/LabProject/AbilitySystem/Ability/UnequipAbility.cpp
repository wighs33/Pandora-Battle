#include "AbilitySystem/Ability/UnequipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UnequipAbility)

UUnequipAbility::UUnequipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationBlockedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
}

// State helpers
void UUnequipAbility::ClearActiveUnequipEffect()
{
	// =================================================================================================================

	if (UnequipEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(UnequipEffectClass);
	}
}

void UUnequipAbility::FinalizeUnequipCommit()
{
	if (bUnequipCommitted || !ActiveUnequipWeaponDefinition)
	{
		return;
	}

	if (!HasAuthority(&CurrentActivationInfo))
	{
		return;
	}

	bUnequipCommitted = true;

	FGameplayTagContainer GrantedTags;
	if (ActiveUnequipWeaponDefinition->IdTag.IsValid())
	{
		GrantedTags.AddTag(ActiveUnequipWeaponDefinition->IdTag);
	}

	if (!GrantedTags.IsEmpty())
	{
		RemoveGameplayEffectsWithGrantedTags(GrantedTags);
	}

	ActiveUnequipWeaponDefinition = nullptr;
}

bool UUnequipAbility::CommitPendingUnequipIfPossible()
{
	if (bUnequipCommitted || !ActiveUnequipWeaponDefinition || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{
		return false;
	}

	if (!EquipmentComponent->UnequipCurrentWeapon())
	{
		return false;
	}

	FinalizeUnequipCommit();
	return true;
}

bool UUnequipAbility::ShouldActivateRequestedEquip() const
{
	if (!PostUnequipEquipAbilityTag.IsValid())
	{
		return false;
	}

	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent && EquipmentComponent->GetRequestedWeaponDefinition() != nullptr;
}

bool UUnequipAbility::ActivateRequestedEquipIfNeeded(const bool bShouldActivate)
{
	if (!bShouldActivate)
	{
		return false;
	}

	FGameplayTagContainer EquipAbilityTags;
	EquipAbilityTags.AddTag(PostUnequipEquipAbilityTag);
	return TryActivateAbilitiesByTags(EquipAbilityTags, true);
}

void UUnequipAbility::OnUnequipMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UUnequipAbility::OnUnequipMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UUnequipAbility::OnUnequipMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UUnequipAbility::OnUnequipCommitTiming(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	FinalizeUnequipCommit();

	// =================================================================================================================
	if (bUnequipCommitted || !HasAuthority(&CurrentActivationInfo) || !ActiveUnequipWeaponDefinition)
	{
		return;
	}

	// =================================================================================================================

	FGameplayTagContainer GrantedTags;
	if (ActiveUnequipWeaponDefinition->IdTag.IsValid())
	{
		GrantedTags.AddTag(ActiveUnequipWeaponDefinition->IdTag);
	}

	// =================================================================================================================

	if (!GrantedTags.IsEmpty())
	{
		RemoveGameplayEffectsWithGrantedTags(GrantedTags);
	}

	ActiveUnequipWeaponDefinition = nullptr;
}

// Ability flow
void UUnequipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bUnequipTransitionResolved = false;

	// =================================================================================================================
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
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

	const bool bWeaponReplacementRequested =
		EquipmentComponent->GetRequestedWeaponDefinition() != nullptr;
	FUnequipData UnequipVisualData;
	if (bWeaponReplacementRequested)
	{
		UnequipVisualData.ItemDefinition =
			EquipmentComponent->GetCurrentWeaponDefinition();
	}
	else if (!EquipmentComponent->GetUnequipData(UnequipVisualData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!UnequipVisualData.ItemDefinition)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	bUnequipCommitted = false;
	ActiveUnequipWeaponDefinition = UnequipVisualData.ItemDefinition;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bWeaponReplacementRequested)
	{
		CommitPendingUnequipIfPossible();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// =================================================================================================================

	if (ensure(CommitUnequipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask =
			CreateWaitGameplayEventTask(CommitUnequipEventTag, true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UUnequipAbility::OnUnequipCommitTiming);
			CommitEventTask->ReadyForActivation();
		}
	}

	// =================================================================================================================

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		UnequipVisualData.UnequipMontage,
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

	MontageTask->OnCompleted.AddDynamic(this, &UUnequipAbility::OnUnequipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UUnequipAbility::OnUnequipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UUnequipAbility::OnUnequipMontageCancelled);
	MontageTask->ReadyForActivation();

	// =================================================================================================================

	if (UnequipEffectClass)
	{
		ApplyGameplayEffect(UnequipEffectClass, 1.f, 1);
	}
}

void UUnequipAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();

	if (!bUnequipTransitionResolved)
	{
		bUnequipTransitionResolved = true;
		CommitPendingUnequipIfPossible();
		ClearActiveUnequipEffect();

		if (Character)
		{
			Character->ResetAnimationToDefault();
		}

		ActiveUnequipWeaponDefinition = nullptr;
		bUnequipCommitted = false;
	}
}

// 장착 해제 능력이 실제로 끝난 뒤에만 대기 중인 새 무기의 장착을 이어 간다.
void UUnequipAbility::OnAbilityEnded(const bool bWasCancelled)
{
	Super::OnAbilityEnded(bWasCancelled);
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!ShouldActivateRequestedEquip() || !EquipmentComponent)
	{
		return;
	}
	if (bWasCancelled || !ActivateRequestedEquipIfNeeded(true))
	{
		EquipmentComponent->CompletePendingWeaponSelectionWithoutAnimation();
	}
}
