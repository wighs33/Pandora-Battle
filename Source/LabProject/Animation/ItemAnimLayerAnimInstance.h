#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ItemAnimLayerAnimInstance.generated.h"

class APdPlayer;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemAnimLayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshCachedPlayer();

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Animation|References")
	TObjectPtr<APdPlayer> CachedPlayer = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Animation|State")
	bool bIsAiming = false;
};
