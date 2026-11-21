#include "Animation/AnimNotifyState_LooseGameplayTag.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_LooseGameplayTag)

namespace
{
	UAbilitySystemComponent* FindAbilitySystem(const USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(MeshComp->GetOwner()) : nullptr;
	}
}

void UAnimNotifyState_LooseGameplayTag::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UAbilitySystemComponent* ASC = FindAbilitySystem(MeshComp); ASC && Tag.IsValid())
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UAnimNotifyState_LooseGameplayTag::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UAbilitySystemComponent* ASC = FindAbilitySystem(MeshComp); ASC && Tag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_LooseGameplayTag::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? Tag.ToString() : TEXT("Gameplay Tag While Active");
}
