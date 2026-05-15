#include "Animation/AnimNotify_CommitCurrentUnequip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/PdCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PlayerComponent/EquipmentComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommitCurrentUnequipNotify, Log, All);

/** 애님 노티파이 시점에 현재 장비 해제와 이벤트 전송을 처리합니다. */
void UAnimNotify_CommitCurrentUnequip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// =================================================================================================================
	// === 기본 유효성 검사

	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	if (!IsValid(MeshComp))
	{
		UE_LOG(LogCommitCurrentUnequipNotify, Warning, TEXT("CommitCurrentUnequip notify skipped: MeshComp is invalid. eventTag=%s"),
			*EventTag.ToString());
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogCommitCurrentUnequipNotify, Warning, TEXT("CommitCurrentUnequip notify skipped: owner=%s authority=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			OwnerActor && OwnerActor->HasAuthority() ? TEXT("true") : TEXT("false"),
			*EventTag.ToString(),
			*GetNameSafe(Animation));
		return;
	}

	// =================================================================================================================
	// === 장비 컴포넌트 조회 및 장착 해제 시도

	APdCharacterBase* CharacterOwner = Cast<APdCharacterBase>(OwnerActor);
	UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent)
	{
		// 장착 해제가 실제로 성공한 경우에만 후속 이벤트를 전송합니다.
		const bool bUnequipped = EquipmentComponent->UnequipCurrentWeapon();
		UE_LOG(LogCommitCurrentUnequipNotify, Log, TEXT("CommitCurrentUnequip notify handled: owner=%s equipment=%s unequipped=%s eventTag=%s animation=%s"),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(EquipmentComponent),
			bUnequipped ? TEXT("true") : TEXT("false"),
			*EventTag.ToString(),
			*GetNameSafe(Animation));

		if (bUnequipped && EventTag.IsValid())
		{
			// =================================================================================================================
			// === 장착 해제 커밋 이벤트 전송

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
			UE_LOG(LogCommitCurrentUnequipNotify, Log, TEXT("CommitCurrentUnequip gameplay event sent: owner=%s eventTag=%s"),
				*GetNameSafe(OwnerActor),
				*EventTag.ToString());
		}
	}
	else
	{
		UE_LOG(LogCommitCurrentUnequipNotify, Warning, TEXT("CommitCurrentUnequip notify failed: owner=%s has no EquipmentComponent. eventTag=%s"),
			*GetNameSafe(OwnerActor),
			*EventTag.ToString());
	}
}

/** 에디터와 디버그에서 표시할 노티파이 이름을 반환합니다. */
FString UAnimNotify_CommitCurrentUnequip::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === 이벤트 태그 포함 노티파이 이름 구성

	return EventTag.IsValid()
		? FString::Printf(TEXT("CommitCurrentUnequip: %s"), *EventTag.ToString())
		: TEXT("CommitCurrentUnequip");
}
