#include "Animation/AnimNotify_CommitRequestedEquip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/PdCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PlayerComponent/EquipmentComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommitRequestedEquipNotify, Log, All);

/** 애님 노티파이 시점에 요청 장착과 이벤트 전송을 처리합니다. */
void UAnimNotify_CommitRequestedEquip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// 사용하지 않는 인자 경고를 방지합니다.
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === 기본 유효성 검사

	if (!IsValid(MeshComp))
	{
		UE_LOG(LogCommitRequestedEquipNotify, Warning, TEXT("CommitRequestedEquip notify skipped: MeshComp is invalid. eventTag=%s"),
			*EventTag.ToString());
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogCommitRequestedEquipNotify, Warning, TEXT("CommitRequestedEquip notify skipped: owner=%s authority=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			OwnerActor && OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
			*EventTag.ToString(),
			*GetNameSafe(Animation));
		return;
	}

	// =================================================================================================================
	// === 장비 컴포넌트 조회 및 요청 장착 시도

	APdCharacterBase* CharacterOwner = Cast<APdCharacterBase>(OwnerActor);
	UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent)
	{
		// 요청 장착이 실제로 성공한 경우에만 후속 이벤트를 전송합니다.
		const bool bEquipped = EquipmentComponent->EquipWeapon();
		UE_LOG(LogCommitRequestedEquipNotify, Log, TEXT("CommitRequestedEquip notify handled: owner=%s equipment=%s equipped=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(EquipmentComponent),
			bEquipped ? TEXT("true") : TEXT("false"),
			*EventTag.ToString(),
			*GetNameSafe(Animation));

		if (bEquipped && EventTag.IsValid())
		{
			// =================================================================================================================
			// === 요청 장착 커밋 이벤트 전송

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
			UE_LOG(LogCommitRequestedEquipNotify, Log, TEXT("CommitRequestedEquip gameplay event sent: owner=%s eventTag=%s"),
				*GetNameSafe(OwnerActor),
				*EventTag.ToString());
		}
	}
	else
	{
		UE_LOG(LogCommitRequestedEquipNotify, Warning, TEXT("CommitRequestedEquip notify failed: owner=%s has no EquipmentComponent. eventTag=%s"),
			*GetNameSafe(OwnerActor),
			*EventTag.ToString());
	}
}

/** 에디터와 디버그에서 표시할 노티파이 이름을 반환합니다. */
FString UAnimNotify_CommitRequestedEquip::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === 이벤트 태그 포함 노티파이 이름 구성

	return EventTag.IsValid()
		? FString::Printf(TEXT("CommitRequestedEquip: %s"), *EventTag.ToString())
		: TEXT("CommitRequestedEquip");
}
