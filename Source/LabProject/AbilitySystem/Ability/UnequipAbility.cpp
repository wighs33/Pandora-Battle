#include "AbilitySystem/Ability/UnequipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Item/ItemDefinition.h"
#include "Character/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(UnequipAbility)

UUnequipAbility::UUnequipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** 장착해제중 태그 제거 */
void UUnequipAbility::ClearActiveUnequipEffect()
{
	// =================================================================================================================
	// === 서버 권한 보유 시 장착 해제 진행 이펙트 제거
	
	if (UnequipEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(UnequipEffectClass);
	}
}

/** 몽타주 완료 시*/
void UUnequipAbility::HandleUnequipMontageCompleted()
{
	// =================================================================================================================
	// === 장착 해제 진행 상태 정리
	
	ClearActiveUnequipEffect();

	// 애니메이션 레이어를 기본 상태로 복구합니다.
	if (APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		Character->ResetAnimationToDefault();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

/** 몽타주 중단 시*/
void UUnequipAbility::HandleUnequipMontageInterrupted()
{
	// =================================================================================================================
	// === 장착 해제 진행 상태 정리
	
	ClearActiveUnequipEffect();

	// 애니메이션 레이어를 기본 상태로 복구합니다.
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
	// =================================================================================================================
	// === 실제 장착 해제 반영 조건 검사

	if (!HasAuthority(&CurrentActivationInfo) || !ActiveUnequipItemDefinition)
	{
		return;
	}
	
	// =================================================================================================================
	// === 제거 대상 아이템 태그 구성

	FGameplayTagContainer GrantedTags;
	if (ActiveUnequipItemDefinition->IdTag.IsValid())
	{
		GrantedTags.AddTag(ActiveUnequipItemDefinition->IdTag);
	}
	
	// =================================================================================================================
	// === 장착 완료 효과 제거

	if (!GrantedTags.IsEmpty())
	{
		RemoveGameplayEffectsWithGrantedTags(GrantedTags);
	}

	// 장착 해제 대상 캐시를 비웁니다.
	ActiveUnequipItemDefinition = nullptr;
}

/** 장착 해제 어빌리티를 활성화하고 몽타주와 이벤트 태스크를 시작합니다. */
void UUnequipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드

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
	
	// =================================================================================================================
	// === 장착 해제 커밋 이벤트 태스크 등록

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
	// === 몽타주 재생
	
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
	// === 효과 적용
	
	if (UnequipEffectClass)
	{
		ApplyGameplayEffect(UnequipEffectClass, 1.f, 1);
	}
}
