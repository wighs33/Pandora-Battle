#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "FrozenGameplayCue.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS(Blueprintable)
class LABPROJECT_API AFrozenGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AFrozenGameplayCue();

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen")
	TObjectPtr<UNiagaraSystem> FrozenSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen")
	TObjectPtr<USoundBase> FrozenStartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen")
	FVector EffectScale = FVector(1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen")
	bool bApplyTimeDilation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen", meta = (EditCondition = "bApplyTimeDilation"))
	float FrozenTimeDilation = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Frozen", meta = (EditCondition = "bApplyTimeDilation"))
	float RestoredTimeDilation = 1.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!GameplayCue|Frozen")
	TObjectPtr<UNiagaraComponent> FrozenEffectComponent;

private:
	bool ApplyFrozenEffect(AActor* MyTarget, bool bPlaySound);
	void RemoveFrozenEffect(AActor* MyTarget);
	USceneComponent* ResolveAttachComponent(AActor* MyTarget) const;
};
