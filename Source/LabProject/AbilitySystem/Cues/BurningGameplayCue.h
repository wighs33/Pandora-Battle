#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "BurningGameplayCue.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS(Blueprintable)
class LABPROJECT_API ABurningGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ABurningGameplayCue();

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Burning")
	TObjectPtr<UNiagaraSystem> BurningSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Burning")
	TObjectPtr<USoundBase> BurningStartSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Burning")
	FName AttachSocketName = TEXT("spine_01");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Burning")
	FVector EffectScale = FVector(1.5f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!GameplayCue|Burning")
	TObjectPtr<UNiagaraComponent> BurningEffectComponent;

private:
	bool ApplyBurningEffect(AActor* MyTarget, bool bPlaySound);
	void RemoveBurningEffect();
	USceneComponent* ResolveAttachComponent(AActor* MyTarget) const;
};
