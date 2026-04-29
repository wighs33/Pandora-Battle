#include "Animation/AnimNotify_CommitRequestedEquip.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PlayerComponent/EquipmentComponent.h"

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
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	// =================================================================================================================
	// === 장비 컴포넌트 조회 및 요청 장착 시도

	if (UEquipmentComponent* EquipmentComponent = OwnerActor->FindComponentByClass<UEquipmentComponent>())
	{
		// 요청 장착이 실제로 성공한 경우에만 후속 이벤트를 전송합니다.
		if (EquipmentComponent->EquipRequestedItem() && EventTag.IsValid())
		{
			// =================================================================================================================
			// === 요청 장착 커밋 이벤트 전송

			FGameplayEventData Payload;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
		}
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