#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_AttackDamageWindow.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotifyState_AttackDamageWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_AttackDamageWindow();

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag StartEventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Tag")
	FGameplayTag EndEventTag;
};
