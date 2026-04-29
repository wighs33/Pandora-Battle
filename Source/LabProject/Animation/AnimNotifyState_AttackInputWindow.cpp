#include "Animation/AnimNotifyState_AttackInputWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	/** 지정한 태그의 공격 관련 게임플레이 이벤트를 액터에 전송합니다. */
	void SendAttackGameplayEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag)
	{
		// =================================================================================================================
		// === 기본 유효성 검사

		if (!IsValid(MeshComp) || !EventTag.IsValid())
		{
			return;
		}

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!IsValid(OwnerActor))
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
}

/** 노티파이 시작 시점에 공격 입력 가능 시작 이벤트를 전송합니다. */
void UAnimNotifyState_AttackInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	// 사용하지 않는 인자 경고를 방지합니다.
	static_cast<void>(Animation);
	static_cast<void>(TotalDuration);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === 시작 이벤트 전송

	SendAttackGameplayEvent(MeshComp, StartEventTag);
}

/** 노티파이 종료 시점에 공격 입력 가능 종료 이벤트를 전송합니다. */
void UAnimNotifyState_AttackInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// 사용하지 않는 인자 경고를 방지합니다.
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
	// === 종료 이벤트 전송

	SendAttackGameplayEvent(MeshComp, EndEventTag);
}

/** 에디터와 디버그에서 표시할 노티파이 이름을 반환합니다. */
FString UAnimNotifyState_AttackInputWindow::GetNotifyName_Implementation() const
{
	// =================================================================================================================
	// === 시작/종료 태그 포함 노티파이 이름 구성

	return TEXT("AttackInputWindow");
}