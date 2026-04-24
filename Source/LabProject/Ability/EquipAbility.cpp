#include "Ability/EquipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Equipment/EquipmentComponent.h"
#include "Item/ItemDefinition.h"
#include "Mode/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipAbility)

UEquipAbility::UEquipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** 장착모션중 태그 제거 */
void UEquipAbility::ClearActiveEquipEffect()
{
	if (ActiveEquipEffectHandle.IsValid() && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(ActiveEquipEffectHandle);
	}

	ActiveEquipEffectHandle.Invalidate();
}

/** 몽타주 완료 시*/
void UEquipAbility::HandleEquipMontageCompleted()
{
	ClearActiveEquipEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

/** 몽타주 중단 시*/
void UEquipAbility::HandleEquipMontageInterrupted()
{
	ClearActiveEquipEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 몽타주 취소 시*/
void UEquipAbility::HandleEquipMontageCancelled()
{
	ClearActiveEquipEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 검 스폰 노티파이 이벤트 수신 시점*/
void UEquipAbility::HandleEquipCommitEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (!HasAuthority(&CurrentActivationInfo) || !EquippedItemEffectClass || !ActiveEquipItemDefinition)
	{
		return;
	}

	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipItemDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipItemDefinition->IdTag);
	}

	if (!DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);
	}
}

/** 엔진API : 활성화 시점*/
void UEquipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
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

	FEquipData EquipData;
	if (!ensure(EquipmentComponent->GetEquipData(EquipData)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	ActiveEquipItemDefinition = EquipData.ItemDefinition;

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (EquippedItemEffectClass && ensure(CommitEquipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			CommitEquipEventTag,
			nullptr,
			true,
			true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UEquipAbility::HandleEquipCommitEvent);
			CommitEventTask->ReadyForActivation();
		}
	}
	
	// =================================================================================================================
	// ### 몽타주 재생
	
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

	MontageTask->OnCompleted.AddDynamic(this, &UEquipAbility::HandleEquipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEquipAbility::HandleEquipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEquipAbility::HandleEquipMontageCancelled);
	MontageTask->ReadyForActivation();
	
	// =================================================================================================================
	// ### 애님 레이어 변경
	
	if (ensure(EquipData.EquipAnimLayer))
	{
		Character->SetCurrentAnimLayer(EquipData.EquipAnimLayer);
	}
	
	// =================================================================================================================
	// ### 효과, VFX 적용
	
	ActiveEquipEffectHandle.Invalidate();
	if (EquipData.EquipEffectClass)
	{
		ActiveEquipEffectHandle = ApplyGameplayEffectHandle(EquipData.EquipEffectClass, 1.f, 1);
	}

	if (ensure(EquipData.EquipCueTag.IsValid()))
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		CueParameters.SourceObject = const_cast<UItemDefinition*>(EquipData.ItemDefinition);
		K2_ExecuteGameplayCueWithParams(EquipData.EquipCueTag, CueParameters);
	}
}
