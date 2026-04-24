#include "Ability/UnequipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Equipment/EquipmentComponent.h"
#include "Item/ItemDefinition.h"
#include "Mode/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UnequipAbility)

UUnequipAbility::UUnequipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** 장착해제중 태그 제거 */
void UUnequipAbility::ClearActiveUnequipEffect()
{
	if (ActiveUnequipEffectHandle.IsValid() && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(ActiveUnequipEffectHandle);
	}

	ActiveUnequipEffectHandle.Invalidate();
}

/** 몽타주 완료 시*/
void UUnequipAbility::HandleUnequipMontageCompleted()
{
	ClearActiveUnequipEffect();

	if (APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		Character->ResetAnimationToDefault();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

/** 몽타주 중단 시*/
void UUnequipAbility::HandleUnequipMontageInterrupted()
{
	ClearActiveUnequipEffect();

	if (APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		Character->ResetAnimationToDefault();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 몽타주 취소 시*/
void UUnequipAbility::HandleUnequipMontageCancelled()
{
	ClearActiveUnequipEffect();

	if (APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		Character->ResetAnimationToDefault();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 검 제거 노티파이 이벤트 수신 시점*/
void UUnequipAbility::HandleUnequipCommitEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (!HasAuthority(&CurrentActivationInfo) || !ActiveUnequipItemDefinition)
	{
		return;
	}

	FGameplayTagContainer GrantedTags;
	if (ActiveUnequipItemDefinition->IdTag.IsValid())
	{
		GrantedTags.AddTag(ActiveUnequipItemDefinition->IdTag);
	}

	if (!GrantedTags.IsEmpty())
	{
		RemoveGameplayEffectsWithGrantedTags(GrantedTags);
	}

	ActiveUnequipItemDefinition = nullptr;
}

/** 엔진API : 활성화 시점*/
void UUnequipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard

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

	FUnequipData UnequipVisualData;
	if (!EquipmentComponent->GetUnequipData(UnequipVisualData))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ActiveUnequipItemDefinition = UnequipVisualData.ItemDefinition;

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ensure(CommitUnequipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			CommitUnequipEventTag,
			nullptr,
			true,
			true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UUnequipAbility::HandleUnequipCommitEvent);
			CommitEventTask->ReadyForActivation();
		}
	}

	// =================================================================================================================
	// ### 몽타주 재생
	
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

	MontageTask->OnCompleted.AddDynamic(this, &UUnequipAbility::HandleUnequipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UUnequipAbility::HandleUnequipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UUnequipAbility::HandleUnequipMontageCancelled);
	MontageTask->ReadyForActivation();

	// =================================================================================================================
	// ### 효과 적용
	
	ActiveUnequipEffectHandle.Invalidate();
	if (UnequipVisualData.UnequipEffectClass)
	{
		ActiveUnequipEffectHandle = ApplyGameplayEffectHandle(UnequipVisualData.UnequipEffectClass, 1.f, 1);
	}
}
