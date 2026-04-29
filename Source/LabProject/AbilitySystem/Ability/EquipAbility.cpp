#include "AbilitySystem/Ability/EquipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Item/ItemDefinition.h"
#include "Character/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipAbility)

UEquipAbility::UEquipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** 현재 활성화된 장착 중 이펙트를 제거합니다. */
void UEquipAbility::ClearActiveEquipEffect()
{
	// =================================================================================================================
	// === 서버 권한 보유 시 장착 진행 이펙트 제거
	
	if (EquipEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(EquipEffectClass);
	}
}

/** 몽타주 완료 시*/
void UEquipAbility::HandleEquipMontageCompleted()
{
	// =================================================================================================================
	// === 장착 진행 상태 정리 후 정상 종료
	
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
	// =================================================================================================================
	// === 실제 장착 반영 조건 검사

	if (!HasAuthority(&CurrentActivationInfo) || !EquippedItemEffectClass || !ActiveEquipItemDefinition)
	{
		return;
	}
	
	// =================================================================================================================
	// === 장착 아이템 태그를 동적 부여 태그로 구성

	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipItemDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipItemDefinition->IdTag);
	}
	
	// =================================================================================================================
	// === 장착 완료 이펙트 적용

	if (!DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);
	}
}

/** 장착 어빌리티를 활성화하고 몽타주, 이벤트, 이펙트를 시작합니다. */
void UEquipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// =================================================================================================================
	// === 캐릭터와 장비 컴포넌트 확인
	
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
	
	// =================================================================================================================
	// === 장착 데이터 확보

	FEquipData EquipData;
	if (!ensure(EquipmentComponent->GetEquipData(EquipData)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	// 현재 장착 처리 중인 아이템 정의를 저장합니다.
	ActiveEquipItemDefinition = EquipData.ItemDefinition;

	// =================================================================================================================
	// === 어빌리티 커밋
	
	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	// =================================================================================================================
	// === 장착 커밋 이벤트 태스크 등록

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
	// === 장착 몽타주 태스크 생성
	
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
	// === 장착 몽타주 종료 콜백 바인딩

	MontageTask->OnCompleted.AddDynamic(this, &UEquipAbility::HandleEquipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEquipAbility::HandleEquipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEquipAbility::HandleEquipMontageCancelled);
	MontageTask->ReadyForActivation();
	
	// =================================================================================================================
	// === 장착 전용 애님 레이어 적용
	
	if (ensure(EquipData.EquipAnimLayer))
	{
		Character->SetCurrentAnimLayer(EquipData.EquipAnimLayer);
	}
	
	// =================================================================================================================
	// === 장착 진행 이펙트 적용
	
	if (EquipEffectClass)
	{
		ApplyGameplayEffect(EquipEffectClass, 1.f, 1);
	}
	
	// =================================================================================================================
	// === 장착 연출용 GameplayCue 실행

	if (ensure(EquipCueTag.IsValid()))
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		CueParameters.SourceObject = const_cast<UItemDefinition*>(EquipData.ItemDefinition);
		K2_ExecuteGameplayCueWithParams(EquipCueTag, CueParameters);
	}
}
