#include "AbilitySystem/Summons/SkillSummonActor.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillSummonActor)

ASkillSummonActor::ASkillSummonActor()
{
	bReplicates = true;
	SetReplicateMovement(true);
}

void ASkillSummonActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillSummonActor, bSummonNiagaraActive, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillSummonActor, ReplicatedSummonNiagaraComponentName, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillSummonActor, bReplicatedSummonNiagaraReset, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillSummonActor, bReplicatedActivateAllWhenNameNone, Params);
}

void ASkillSummonActor::ActivateSummonNiagara(
	const FName ComponentName,
	const bool bResetSystem,
	const bool bActivateAllWhenNameNone)
{
	ReplicatedSummonNiagaraComponentName = ComponentName;
	bReplicatedSummonNiagaraReset = bResetSystem;
	bReplicatedActivateAllWhenNameNone = bActivateAllWhenNameNone;
	bSummonNiagaraActive = true;
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, ReplicatedSummonNiagaraComponentName, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, bReplicatedSummonNiagaraReset, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, bReplicatedActivateAllWhenNameNone, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, bSummonNiagaraActive, this);
	}
	ApplySummonNiagaraActiveState();
}

void ASkillSummonActor::DeactivateSummonNiagara(
	const FName ComponentName,
	const bool bActivateAllWhenNameNone)
{
	ReplicatedSummonNiagaraComponentName = ComponentName;
	bReplicatedActivateAllWhenNameNone = bActivateAllWhenNameNone;
	bSummonNiagaraActive = false;
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, ReplicatedSummonNiagaraComponentName, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, bReplicatedActivateAllWhenNameNone, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(ASkillSummonActor, bSummonNiagaraActive, this);
	}
	ApplySummonNiagaraActiveState();
}

void ASkillSummonActor::OnRep_SummonNiagaraActive()
{
	ApplySummonNiagaraActiveState();
}

void ASkillSummonActor::ApplySummonNiagaraActiveState()
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	FindSummonNiagaraComponents(
		ReplicatedSummonNiagaraComponentName,
		bReplicatedActivateAllWhenNameNone,
		NiagaraComponents);

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		if (bSummonNiagaraActive)
		{
			NiagaraComponent->Activate(bReplicatedSummonNiagaraReset);
		}
		else
		{
			NiagaraComponent->Deactivate();
		}
	}
}

void ASkillSummonActor::FindSummonNiagaraComponents(
	const FName ComponentName,
	const bool bActivateAllWhenNameNone,
	TArray<UNiagaraComponent*>& OutComponents) const
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);

	if (ComponentName.IsNone())
	{
		if (bActivateAllWhenNameNone)
		{
			OutComponents = MoveTemp(NiagaraComponents);
		}
		return;
	}

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		if (NiagaraComponent->GetFName() == ComponentName || NiagaraComponent->ComponentHasTag(ComponentName))
		{
			OutComponents.Add(NiagaraComponent);
		}
	}
}
