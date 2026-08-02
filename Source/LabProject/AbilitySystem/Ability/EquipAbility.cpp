#include "AbilitySystem/Ability/EquipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipAbility)

UEquipAbility::UEquipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationBlockedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
}

// State helpers
void UEquipAbility::ClearActiveEquipEffect()
{
	// =================================================================================================================

	if (EquipEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(EquipEffectClass);
	}
}

void UEquipAbility::ClearPendingEquipState()
{
	ActiveEquipWeaponDefinition = nullptr;
	PendingEquipAnimLayer = nullptr;
	bEquipCommitted = false;
	bEquipAbilityCommitted = false;
}

void UEquipAbility::ResolveEquipTransition()
{
	if (bEquipTransitionResolved)
	{
		return;
	}

	bEquipTransitionResolved = true;
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr)
	{
		if (bEquipAbilityCommitted)
		{
			CommitPendingEquipIfPossible();
			EquipmentComponent->CompletePendingWeaponSelectionWithoutAnimation();
		}
		else
		{
			// CommitAbility can fail because GE_EquipWeapon_Cooldown is active.
			// Never let the cancellation fallback bypass that cooldown.
			EquipmentComponent->ClearRequestedWeaponInstance();
		}
	}

	ClearActiveEquipEffect();
	ClearPendingEquipState();
}

void UEquipAbility::FinalizeEquipCommit()
{
	if (bEquipCommitted || !ActiveEquipWeaponDefinition)
	{
		return;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!HasAuthority(&CurrentActivationInfo))
	{

		return;
	}

	bEquipCommitted = true;



	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipWeaponDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipWeaponDefinition->IdTag);
	}

	if (EquippedItemEffectClass && !DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);

	}

	if (Character && PendingEquipAnimLayer)
	{
		Character->SetCurrentAnimLayer(PendingEquipAnimLayer);

	}
}

bool UEquipAbility::CommitPendingEquipIfPossible()
{
	if (bEquipCommitted || !ActiveEquipWeaponDefinition || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{

		return false;
	}

	if (!EquipmentComponent->EquipWeapon())
	{
		return false;
	}

	FinalizeEquipCommit();
	return true;
}

void UEquipAbility::OnEquipMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEquipAbility::OnEquipMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UEquipAbility::OnEquipMontageCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UEquipAbility::OnEquipCommitTiming(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	FinalizeEquipCommit();

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (bEquipCommitted)
	{
		return;
	}

	// =================================================================================================================
	if (!HasAuthority(&CurrentActivationInfo) || !ActiveEquipWeaponDefinition)
	{

		return;
	}


	// =================================================================================================================

	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipWeaponDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipWeaponDefinition->IdTag);
	}

	// =================================================================================================================

	if (EquippedItemEffectClass && !DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);

	}

	if (Character && PendingEquipAnimLayer)
	{
		Character->SetCurrentAnimLayer(PendingEquipAnimLayer);

	}
}

// Ability flow
void UEquipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEquipTransitionResolved = false;
	bEquipAbilityCommitted = false;


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

	// =================================================================================================================

	FEquipData EquipData;
	if (!EquipmentComponent->GetEquipData(EquipData))
	{
		// ServerInitiated activation can reach the owning client after its
		// transient requested-weapon state was cleared by a skill interrupt.
		// The replicated current weapon remains the safe presentation source.
		EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
		EndAbility(
			Handle,
			ActorInfo,
			ActivationInfo,
			ActorInfo && ActorInfo->IsNetAuthority(),
			false);
		return;
	}
	bEquipCommitted = false;
	ActiveEquipWeaponDefinition = EquipData.ItemDefinition;
	PendingEquipAnimLayer = EquipData.EquipAnimLayer;


	// =================================================================================================================

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bEquipAbilityCommitted = true;

	const auto ExecuteEquipCue =
		[this, Character, ItemDefinition = EquipData.ItemDefinition]()
		{
			if (!EquipCueTag.IsValid())
			{
				return;
			}

			FGameplayCueParameters CueParameters;
			CueParameters.Location = Character->GetActorLocation();
			CueParameters.Instigator = Character;
			CueParameters.EffectCauser = Character;
			CueParameters.SourceObject = const_cast<UItemDefinition*>(ItemDefinition);
			K2_ExecuteGameplayCueWithParams(EquipCueTag, CueParameters);
		};

	if (EquipmentComponent->ShouldEquipWeaponsWithoutAnimation())
	{
		CommitPendingEquipIfPossible();
		ExecuteEquipCue();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}


	// =================================================================================================================

	if (ensure(CommitEquipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask =
			CreateWaitGameplayEventTask(CommitEquipEventTag, true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UEquipAbility::OnEquipCommitTiming);
			CommitEventTask->ReadyForActivation();

		}
	}

	// =================================================================================================================

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		EquipData.EquipMontage,
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

	// =================================================================================================================
	MontageTask->OnCompleted.AddDynamic(this, &UEquipAbility::OnEquipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEquipAbility::OnEquipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEquipAbility::OnEquipMontageCancelled);
	MontageTask->ReadyForActivation();


	// =================================================================================================================


	// =================================================================================================================

	if (EquipEffectClass)
	{
		ApplyGameplayEffect(EquipEffectClass, 1.f, 1);

	}

	// =================================================================================================================

	ExecuteEquipCue();
}

void UEquipAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	ResolveEquipTransition();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
