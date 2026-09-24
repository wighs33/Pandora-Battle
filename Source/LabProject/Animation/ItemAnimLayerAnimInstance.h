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

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "!Animation|Item")
	APdPlayer* GetCachedPlayer() const { return CachedPlayer.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Animation|Item")
	bool IsAiming() const { return bIsAiming; }

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshCachedPlayer();
	void PushValuesToLegacyBlueprintVariables();

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Animation|References", meta = (DisplayName = "Cached Player"))
	TObjectPtr<APdPlayer> CachedPlayer = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Animation|State", meta = (DisplayName = "IsAiming?"))
	bool bIsAiming = false;
};
