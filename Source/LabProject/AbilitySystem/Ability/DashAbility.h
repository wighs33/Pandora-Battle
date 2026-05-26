#pragma once

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "DashAbility.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API UDashAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UDashAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UFUNCTION()
	void OnDashRootMotionFinished();

	FVector ResolveDashDirection(const FGameplayEventData* TriggerEventData) const;
	FVector GetFallbackDashDirection() const;
	float GetMaxSpeed() const;

	int32 GetMaxDashCharges() const;
	bool CommitDashCostAndMaybeCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	UPROPERTY(Transient)
	int32 DashChargesUsed = 0;
};
