#include "Animation/AnimNotify_CommitCurrentUnequip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PlayerComponent/EquipmentComponent.h"

/** 애님 노티파이 시점에 현재 장비 해제와 이벤트 전송을 처리합니다. */
void UAnimNotify_CommitCurrentUnequip::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// =================================================================================================================
	// === 기본 유효성 검사

	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	// =================================================================================================================
	// === 장비 컴포넌트 조회 및 장착 해제 시도

	if (UEquipmentComponent* EquipmentComponent = OwnerActor->FindComponentByClass<UEquipmentComponent>())
	{
		// 장착 해제가 실제로 성공한 경우에만 후속 이벤트를 전송합니다.
		if (EquipmentComponent->UnequipCurrentItem() && EventTag.IsValid())
		{
			// =================================================================================================================
			// === 장착 해제 커밋 이벤트 전송

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
		}
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