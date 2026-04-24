// Fill out your copyright notice in the Description page of Project Settings.

#include "PdAbilitySystemComponent.h"
#include "GameplayEffect.h"

namespace PdAbilitySystemComponent
{
	const FName OperationSetByCallerName(TEXT("Pd.StatUp.Operation"));
}

UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
{
	// Mixed mode keeps full data on owners while reducing replication for others.
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

bool UPdAbilitySystemComponent::ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EPdStatChangeOperation Operation, float Level)
{
	if (!GameplayEffectClass || !StatTag.IsValid())
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(StatTag, Magnitude);
	SpecHandle.Data->SetSetByCallerMagnitude(PdAbilitySystemComponent::OperationSetByCallerName, static_cast<float>(Operation));
	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}
