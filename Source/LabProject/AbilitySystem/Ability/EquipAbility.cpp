#include "AbilitySystem/Ability/EquipAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimInstance.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Item/ItemDefinition.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipAbility)

DEFINE_LOG_CATEGORY_STATIC(LogEquipAbility, Log, All);

UEquipAbility::UEquipAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationBlockedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
}

/** 현재 활성화된 장착 중 이펙트를 제거합니다. */
// State helpers
void UEquipAbility::ClearActiveEquipEffect()
{
	// =================================================================================================================
	// === 서버 권한 보유 시 장착 진행 이펙트 제거
	
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
}

void UEquipAbility::FinalizeEquipCommit()
{
	if (bEquipCommitted || !ActiveEquipWeaponDefinition)
	{
		return;
	}

	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!HasAuthority(&CurrentActivationInfo))
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("FinalizeEquipCommit skipped: authority=false character=%s definition=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(ActiveEquipWeaponDefinition.Get()));
		return;
	}

	bEquipCommitted = true;

	UE_LOG(LogEquipAbility, Log, TEXT("FinalizeEquipCommit: character=%s definition=%s idTag=%s"),
		*GetNameSafe(Character),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()),
		*ActiveEquipWeaponDefinition->IdTag.ToString());

	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipWeaponDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipWeaponDefinition->IdTag);
	}

	if (EquippedItemEffectClass && !DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);
		UE_LOG(LogEquipAbility, Log, TEXT("Equipped item effect applied: effect=%s tags=%s"),
			*GetNameSafe(EquippedItemEffectClass.Get()),
			*DynamicGrantedTags.ToStringSimple());
	}
	else
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Equipped item effect skipped: effect=%s tags=%s"),
			*GetNameSafe(EquippedItemEffectClass.Get()),
			*DynamicGrantedTags.ToStringSimple());
	}

	if (Character && PendingEquipAnimLayer)
	{
		Character->SetCurrentAnimLayer(PendingEquipAnimLayer);
		UE_LOG(LogEquipAbility, Log, TEXT("Equip anim layer applied: character=%s animLayer=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(PendingEquipAnimLayer.Get()));
	}
}

bool UEquipAbility::CommitPendingEquipIfPossible()
{
	if (bEquipCommitted || !ActiveEquipWeaponDefinition || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	if (!EquipmentComponent)
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("CommitPendingEquipIfPossible failed: equipment component is null. character=%s"),
			*GetNameSafe(Character));
		return false;
	}

	if (!EquipmentComponent->EquipWeapon())
	{
		return false;
	}

	FinalizeEquipCommit();
	return true;
}

/** 몽타주 완료 시*/
// Timing callbacks
void UEquipAbility::OnEquipMontageCompleted()
{
	UE_LOG(LogEquipAbility, Log, TEXT("Equip montage completed: ability=%s activeDefinition=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()));

	// =================================================================================================================
	// === 장착 진행 상태 정리 후 정상 종료
	
	CommitPendingEquipIfPossible();
	ClearActiveEquipEffect();
	ClearPendingEquipState();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

/** 몽타주 중단 시*/
void UEquipAbility::OnEquipMontageInterrupted()
{
	UE_LOG(LogEquipAbility, Warning, TEXT("Equip montage interrupted: ability=%s activeDefinition=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()));

	CommitPendingEquipIfPossible();
	ClearActiveEquipEffect();
	ClearPendingEquipState();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 몽타주 취소 시*/
void UEquipAbility::OnEquipMontageCancelled()
{
	UE_LOG(LogEquipAbility, Warning, TEXT("Equip montage cancelled: ability=%s activeDefinition=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()));

	CommitPendingEquipIfPossible();
	ClearActiveEquipEffect();
	ClearPendingEquipState();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

/** 검 스폰 노티파이 이벤트 수신 시점*/
void UEquipAbility::OnEquipCommitTiming(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	FinalizeEquipCommit();

	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (bEquipCommitted)
	{
		return;
	}

	// =================================================================================================================
	// === 실제 장착 반영 조건 검사

	if (!HasAuthority(&CurrentActivationInfo) || !ActiveEquipWeaponDefinition)
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("OnEquipCommitTiming skipped: authority=%s activeDefinition=%s character=%s"),
			HasAuthority(&CurrentActivationInfo) ? TEXT("true") : TEXT("false"),
			*GetNameSafe(ActiveEquipWeaponDefinition.Get()),
			*GetNameSafe(Character));
		return;
	}
	UE_LOG(LogEquipAbility, Log, TEXT("OnEquipCommitTiming received: character=%s definition=%s idTag=%s"),
		*GetNameSafe(Character),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()),
		*ActiveEquipWeaponDefinition->IdTag.ToString());
	
	// =================================================================================================================
	// === 장착 아이템 태그를 동적 부여 태그로 구성

	FGameplayTagContainer DynamicGrantedTags;
	if (ActiveEquipWeaponDefinition->IdTag.IsValid())
	{
		DynamicGrantedTags.AddTag(ActiveEquipWeaponDefinition->IdTag);
	}
	
	// =================================================================================================================
	// === 장착 완료 이펙트 적용

	if (EquippedItemEffectClass && !DynamicGrantedTags.IsEmpty())
	{
		ApplyGameplayEffectHandle(EquippedItemEffectClass, DynamicGrantedTags, 1.f, 1);
		UE_LOG(LogEquipAbility, Log, TEXT("Equipped item effect applied: effect=%s tags=%s"),
			*GetNameSafe(EquippedItemEffectClass.Get()),
			*DynamicGrantedTags.ToStringSimple());
	}
	else
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Equipped item effect skipped: effect=%s tags=%s"),
			*GetNameSafe(EquippedItemEffectClass.Get()),
			*DynamicGrantedTags.ToStringSimple());
	}

	if (Character && PendingEquipAnimLayer)
	{
		Character->SetCurrentAnimLayer(PendingEquipAnimLayer);
		UE_LOG(LogEquipAbility, Log, TEXT("Equip anim layer applied: character=%s animLayer=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(PendingEquipAnimLayer.Get()));
	}
}

/** 장착 어빌리티를 활성화하고 몽타주, 이벤트, 이펙트를 시작합니다. */
// Ability flow
void UEquipAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogEquipAbility, Log, TEXT("Activate EquipAbility: ability=%s avatar=%s owner=%s authority=%s"),
		*GetNameSafe(this),
		ActorInfo && ActorInfo->AvatarActor.IsValid() ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"),
		ActorInfo && ActorInfo->OwnerActor.IsValid() ? *GetNameSafe(ActorInfo->OwnerActor.Get()) : TEXT("None"),
		HasAuthority(&ActivationInfo) ? TEXT("true") : TEXT("false"));

	// =================================================================================================================
	// === 캐릭터와 장비 컴포넌트 확인
	
	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!ensure(Character))
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed: character is null."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent();
	if (!ensure(EquipmentComponent))
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed: equipment component is null. character=%s"),
			*GetNameSafe(Character));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	// =================================================================================================================
	// === 장착 데이터 확보

	FEquipData EquipData;
	if (!ensure(EquipmentComponent->GetEquipData(EquipData)))
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed: GetEquipData returned false. character=%s equipment=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(EquipmentComponent));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	// 현재 장착 처리 중인 아이템 정의를 저장합니다.
	bEquipCommitted = false;
	ActiveEquipWeaponDefinition = EquipData.ItemDefinition;
	PendingEquipAnimLayer = EquipData.EquipAnimLayer;
	UE_LOG(LogEquipAbility, Log, TEXT("Equip data resolved: definition=%s montage=%s animLayer=%s"),
		*GetNameSafe(EquipData.ItemDefinition),
		*GetNameSafe(EquipData.EquipMontage),
		*GetNameSafe(PendingEquipAnimLayer.Get()));

	// =================================================================================================================
	// === 어빌리티 커밋
	
	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed: CommitAbility returned false. definition=%s"),
			*GetNameSafe(ActiveEquipWeaponDefinition.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UE_LOG(LogEquipAbility, Log, TEXT("EquipAbility committed: definition=%s"),
		*GetNameSafe(ActiveEquipWeaponDefinition.Get()));
	
	// =================================================================================================================
	// === 장착 커밋 이벤트 태스크 등록

	if (ensure(CommitEquipEventTag.IsValid()))
	{
		UAbilityTask_WaitGameplayEvent* CommitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			CommitEquipEventTag,
			nullptr,
			true,
			true);
		if (ensure(CommitEventTask))
		{
			CommitEventTask->EventReceived.AddDynamic(this, &UEquipAbility::OnEquipCommitTiming);
			CommitEventTask->ReadyForActivation();
			UE_LOG(LogEquipAbility, Log, TEXT("Equip commit event task registered: tag=%s"),
				*CommitEquipEventTag.ToString());
		}
		else
		{
			UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed to create commit event task: tag=%s"),
				*CommitEquipEventTag.ToString());
		}
	}
	else
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility: CommitEquipEventTag is invalid."));
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
		UE_LOG(LogEquipAbility, Warning, TEXT("Activate EquipAbility failed: montage task is null. montage=%s"),
			*GetNameSafe(EquipData.EquipMontage));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	// =================================================================================================================
	// === 장착 몽타주 종료 콜백 바인딩

	MontageTask->OnCompleted.AddDynamic(this, &UEquipAbility::OnEquipMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UEquipAbility::OnEquipMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UEquipAbility::OnEquipMontageCancelled);
	MontageTask->ReadyForActivation();
	UE_LOG(LogEquipAbility, Log, TEXT("Equip montage started: montage=%s definition=%s"),
		*GetNameSafe(EquipData.EquipMontage),
		*GetNameSafe(EquipData.ItemDefinition));
	
	// =================================================================================================================
	// === 장착 전용 애님 레이어 적용
	
	
	// =================================================================================================================
	// === 장착 진행 이펙트 적용
	
	if (EquipEffectClass)
	{
		ApplyGameplayEffect(EquipEffectClass, 1.f, 1);
		UE_LOG(LogEquipAbility, Log, TEXT("Equip progress effect applied: effect=%s"),
			*GetNameSafe(EquipEffectClass.Get()));
	}
	else
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Equip progress effect skipped: EquipEffectClass is null."));
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
		UE_LOG(LogEquipAbility, Log, TEXT("Equip cue executed: tag=%s definition=%s"),
			*EquipCueTag.ToString(),
			*GetNameSafe(EquipData.ItemDefinition));
	}
	else
	{
		UE_LOG(LogEquipAbility, Warning, TEXT("Equip cue skipped: EquipCueTag is invalid."));
	}
}
