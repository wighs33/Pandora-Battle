#include "Animation/AnimNotifyState_AttackInputWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	void SendAttackGameplayEvent(USkeletalMeshComponent* MeshComp, const FGameplayTag& EventTag)
	{
		if (!IsValid(MeshComp) || !EventTag.IsValid())
		{
			return;
		}

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!IsValid(OwnerActor))
		{
			return;
		}

		if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor))
		{
			return;
		}

		FGameplayEventData Payload;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
	}
}

void UAnimNotifyState_AttackInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(TotalDuration);
	static_cast<void>(EventReference);
	SendAttackGameplayEvent(MeshComp, StartEventTag);
}

void UAnimNotifyState_AttackInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	static_cast<void>(Animation);
	static_cast<void>(EventReference);
	SendAttackGameplayEvent(MeshComp, EndEventTag);
}

FString UAnimNotifyState_AttackInputWindow::GetNotifyName_Implementation() const
{
	if (StartEventTag.IsValid() || EndEventTag.IsValid())
	{
		return FString::Printf(TEXT("AttackInputWindow: %s / %s"), *StartEventTag.ToString(), *EndEventTag.ToString());
	}

	return TEXT("AttackInputWindow");
}
