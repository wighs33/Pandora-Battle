#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_WeaponEvent.generated.h"

UCLASS()
class LABPROJECT_API UAnimNotify_WeaponEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UAnimNotify_WeaponEvent();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static bool DispatchWeaponEvent(USkeletalMeshComponent* MeshComp, FName EventName);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Weapon")
	FName WeaponEventName = NAME_None;
};
