#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponEvent.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotify_WeaponEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_WeaponEvent();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	static bool DispatchWeaponEvent(USkeletalMeshComponent* MeshComp, FName EventName);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Weapon")
	FName WeaponEventName = NAME_None;
};
