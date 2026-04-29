#include "AnimNotify_TriggerEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

/** 애님 노티파이 시점에 지정한 게임플레이 이벤트를 전송합니다. */
void UAnimNotify_TriggerEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
	if (!IsValid(OwnerActor) || !EventTag.IsValid())
	{
		return;
	}

	// =================================================================================================================
	// === AbilitySystemComponent 존재 여부 확인

	if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
	{
		return;
	}

	// =================================================================================================================
	// === 게임플레이 이벤트 전송

	FGameplayEventData Payload;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}

/** 에디터와 디버그에서 표시할 노티파이 이름을 반환합니다. */
FString UAnimNotify_TriggerEvent::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === 이벤트 태그 포함 노티파이 이름 구성

	return EventTag.IsValid()
		? FString::Printf(TEXT("TriggerEvent: %s"), *EventTag.ToString())
		: Super::GetNotifyName_Implementation();
}