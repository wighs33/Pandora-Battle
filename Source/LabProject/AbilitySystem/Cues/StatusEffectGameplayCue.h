#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "StatusEffectGameplayCue.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS(Blueprintable)
class LABPROJECT_API AStatusEffectGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStatusEffectGameplayCue();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool ApplyEffect(AActor* Target, bool bPlayStartSound);
	void RemoveEffect();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|StatusEffect")
	TObjectPtr<UNiagaraSystem> EffectSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|StatusEffect")
	TObjectPtr<USoundBase> StartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|StatusEffect")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|StatusEffect")
	FVector EffectScale = FVector::OneVector;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> EffectComponent;

	bool bHasPlayedStartSound = false;
};
