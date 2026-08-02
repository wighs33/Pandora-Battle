#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillSummonActor.generated.h"

class UNiagaraComponent;

UCLASS(Blueprintable)
class LABPROJECT_API ASkillSummonActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillSummonActor();

	UFUNCTION(BlueprintCallable, Category = "!Skill|Summon")
	void ActivateSummonNiagara(FName ComponentName, bool bResetSystem, bool bActivateAllWhenNameNone);

	UFUNCTION(BlueprintCallable, Category = "!Skill|Summon")
	void DeactivateSummonNiagara(FName ComponentName, bool bActivateAllWhenNameNone);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_SummonNiagaraActive();

	void ApplySummonNiagaraActiveState();
	void FindSummonNiagaraComponents(FName ComponentName, bool bActivateAllWhenNameNone, TArray<UNiagaraComponent*>& OutComponents) const;

	UPROPERTY(ReplicatedUsing = OnRep_SummonNiagaraActive)
	bool bSummonNiagaraActive = false;

	UPROPERTY(Replicated)
	FName ReplicatedSummonNiagaraComponentName = NAME_None;

	UPROPERTY(Replicated)
	bool bReplicatedSummonNiagaraReset = true;

	UPROPERTY(Replicated)
	bool bReplicatedActivateAllWhenNameNone = true;
};
