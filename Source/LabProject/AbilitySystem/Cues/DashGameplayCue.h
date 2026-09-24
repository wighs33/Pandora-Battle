#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "DashGameplayCue.generated.h"

class UParticleSystem;
class USoundBase;

UCLASS(Blueprintable)
class LABPROJECT_API UDashGameplayCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UDashGameplayCue();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static void SetCharacterMeshVisibility(AActor* Target, bool bVisible);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	TObjectPtr<UParticleSystem> StartEmitter = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	FVector StartEmitterScale = FVector(0.3f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	TObjectPtr<USoundBase> StartSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	TObjectPtr<UParticleSystem> RemovedEmitter = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	FVector RemovedEmitterScale = FVector(1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	TObjectPtr<USoundBase> RemovedSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Dash Cue")
	FVector RemovedSoundLocationOffset = FVector::ZeroVector;
};
