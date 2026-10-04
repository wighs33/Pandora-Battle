#include "AnimNotify_TriggerEvent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_TriggerEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);

	// =================================================================================================================
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

	UAbilitySystemComponent* AbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	// =================================================================================================================

	ACharacterBase* Character = Cast<ACharacterBase>(OwnerActor);
	const bool bIsAuthoritativeNotify = OwnerActor->HasAuthority();
	const bool bIsOwningClientPresentation = Character && Character->IsLocallyControlled() && !bIsAuthoritativeNotify;
	if (!bIsAuthoritativeNotify && !bIsOwningClientPresentation)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;

	if (bIsAuthoritativeNotify)
	{
		// 이 이벤트 태그로 활성화되도록 설정한 능력까지 포함한 GameplayEvent 전체 경로는
		// 서버만 실행할 수 있다.
		AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
		return;
	}

	// 소유 클라이언트에서는 이미 활성인 정확한 태그의 리스너에만 알린다(예측 표시·입력 준비용).
	// HandleGameplayEvent는 부르지 않는다. 새 능력을 활성화할 수 있어 클라이언트 몽타주 타이밍이
	// 게임플레이가 되어 버린다.
	if (FGameplayEventMulticastDelegate* EventDelegate =
		AbilitySystemComponent->GenericGameplayEventCallbacks.Find(EventTag))
	{
		FGameplayEventMulticastDelegate DelegateCopy = *EventDelegate;
		DelegateCopy.Broadcast(&Payload);
	}
}

FString UAnimNotify_TriggerEvent::GetNotifyName_Implementation() const
{
	// =================================================================================================================

	return EventTag.IsValid()
		? FString::Printf(TEXT("TriggerEvent: %s"), *EventTag.ToString())
		: Super::GetNotifyName_Implementation();
}
