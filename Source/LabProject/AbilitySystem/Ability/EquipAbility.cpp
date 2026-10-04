#include "AbilitySystem/Ability/EquipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipAbility)

UEquipAbility::UEquipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationBlockedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
}

const FGameplayTagContainer* UEquipAbility::GetCooldownTags() const
{
	EquipCooldownTags.Reset();
	EquipCooldownTags.AddTag(LabGameplayTags::Cooldown_EquipWeapon);
	return &EquipCooldownTags;
}

void UEquipAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	TSoftObjectPtr<UCharacterActionDefinition> ActionDefinition(UCharacterActionDefinition::GetDefaultDefinitionPath());
	const UCharacterActionDefinition* LoadedDefinition = ActionDefinition.LoadSynchronous();
	const float CooldownDuration = LoadedDefinition
		? static_cast<float>(FMath::Max(
			LoadedDefinition->GetCooldownDuration(
				ECharacterActionType::PandoraWeaponSwap),
			0.0))
		: 0.0f;

	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(LabGameplayTags::Cooldown_EquipWeapon);
	ApplySharedCooldownEffect(Handle, ActorInfo, ActivationInfo, CooldownDuration, CooldownTags);
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
			// 공유 장비 쿨다운 중에는 CommitAbility가 실패할 수 있다.
			// 취소 처리 경로가 그 쿨다운을 건너뛰게 두지 않는다.
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
}

// Ability flow
void UEquipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bEquipTransitionResolved = false;
	bEquipAbilityCommitted = false;

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!ensure(Character) || !ensure(EquipmentComponent))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FEquipData EquipData;
	if (!EquipmentComponent->GetEquipData(EquipData))
	{
		// 스킬이 끊으면서 요청한 무기를 비웠을 수 있다. 지금 무기의 애니메이션 레이어를 되돌리고, 지난 장착 요청은 확정하지 않고 끝낸다.
		EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
		EndAbility(Handle, ActorInfo, ActivationInfo, ActorInfo && ActorInfo->IsNetAuthority(), false);
		return;
	}
	bEquipCommitted = false;
	ActiveEquipWeaponDefinition = EquipData.ItemDefinition;
	PendingEquipAnimLayer = EquipData.EquipAnimLayer;

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bEquipAbilityCommitted = true;

	if (EquipmentComponent->ShouldEquipWeaponsWithoutAnimation())
	{
		CommitPendingEquipIfPossible();
		ExecuteEquipCue(*Character, EquipData.ItemDefinition);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!PlayEquipMontage(EquipData.EquipMontage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	if (EquipEffectClass)
	{
		ApplyGameplayEffect(EquipEffectClass, 1.f, 1);
	}
	ExecuteEquipCue(*Character, EquipData.ItemDefinition);
}

void UEquipAbility::ExecuteEquipCue(ACharacterBase& Character, const UItemDefinition* ItemDefinition)
{
	if (!EquipCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Location = Character.GetActorLocation();
	CueParameters.Instigator = &Character;
	CueParameters.EffectCauser = &Character;
	CueParameters.SourceObject = ItemDefinition;
	K2_ExecuteGameplayCueWithParams(EquipCueTag, CueParameters);
}

bool UEquipAbility::PlayEquipMontage(UAnimMontage* EquipMontage)
{
	if (ensure(CommitEquipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask = CreateWaitGameplayEventTask(CommitEquipEventTag, true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UEquipAbility::OnEquipCommitTiming);
			CommitEventTask->ReadyForActivation();
		}
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, EquipMontage, 1.f, NAME_None, false, 1.f, 0.f, false);
	if (!ensure(MontageTask))
	{
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UEquipAbility::OnEquipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEquipAbility::OnEquipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEquipAbility::OnEquipMontageCancelled);
	MontageTask->ReadyForActivation();
	return true;
}

void UEquipAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	ResolveEquipTransition();
}
