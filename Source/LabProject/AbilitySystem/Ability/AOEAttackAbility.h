#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/EngineTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "AOEAttackAbility.generated.h"

class AGameplayAbilityTargetActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputPress;
class UAbilityTask_WaitTargetData;
class UAnimMontage;
class UGameplayEffect;
class UMaterialInterface;

UCLASS(Blueprintable)
class LABPROJECT_API UAOEAttackAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UAOEAttackAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Ability|AOE")
	void StartTargeting();

	UFUNCTION(BlueprintCallable, Category = "!Ability|AOE")
	void LoopTargetingAnimation();

	UFUNCTION(BlueprintCallable, Category = "!Ability|AOE")
	void ConfirmStrike();

	UFUNCTION(BlueprintCallable, Category = "!Ability|AOE")
	void AOEDamage();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UPROPERTY(Transient)
	bool bIsWaitingTargetData = false;

	UPROPERTY(Transient)
	bool bStrikeTriggered = false;

	UPROPERTY(Transient)
	bool bWaitingLightningDamage = false;

	UPROPERTY(Transient)
	double CachedAOERadius = 0.0;

	UPROPERTY(Transient)
	FVector ConfirmedAOELocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> HitActors;

private:
	void WaitCancelInput();
	void StartWaitTargetData();
	void StartWaitMontageTrigger();
	void StartLightningDamageDelay();
	void ConfigureSpawnedTargetActor(AGameplayAbilityTargetActor* SpawnedActor);
	void ApplyEffectToHitActor(AActor* HitActor);
	void ApplyDirectAOECamera(bool bEnabled) const;
	void RemovePersistentGameplayCues();
	FGameplayAbilityTargetingLocationInfo MakeTargetStartLocation();
	bool GetTargetGroundLocation(AActor* AttackTarget, FVector& OutGroundLocation) const;
	FVector ResolveConfirmedAOELocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint) const;
	FGameplayEffectSpecHandle MakeDamageEffectSpec() const;
	UAnimMontage* GetConfiguredTargetingMontage() const;
	UAnimMontage* GetConfiguredTriggerMontage() const;
	TSubclassOf<UGameplayEffect> GetConfiguredDamageEffectClass() const;
	TArray<TEnumAsByte<EObjectTypeQuery>> GetConfiguredDamageObjectTypes() const;
	TSubclassOf<AGameplayAbilityTargetActor> GetConfiguredTargetActorClass() const;
	UMaterialInterface* GetConfiguredTargetingDecal() const;
	FLinearColor GetConfiguredTargetingDecalColor() const;
	FName GetConfiguredTargetingTraceProfileName() const;
	float GetConfiguredTargetingMaxRange() const;
	float GetConfiguredTargetingCollisionRadius() const;
	float GetConfiguredTargetingCollisionHeight() const;
	bool GetConfiguredTargetingTraceAffectsAimPitch() const;
	bool GetConfiguredDebugTargeting() const;
	bool GetConfiguredDrawDebugDamageRadius() const;
	float GetConfiguredDebugDamageRadiusDrawTime() const;
	FName GetConfiguredTargetingSocketName() const;
	TEnumAsByte<ETraceTypeQuery> GetConfiguredTargetGroundTraceChannel() const;
	float GetConfiguredTargetGroundTraceDepth() const;
	FGameplayTag GetConfiguredDamageDataTag() const;
	FGameplayTag GetConfiguredMontageTriggerEventTag() const;
	FGameplayTag GetConfiguredAOEIndicatorCueTag() const;
	FGameplayTag GetConfiguredLightningBoltCueTag() const;
	float GetConfiguredLightningDamageDelay() const;
	FWeaponAimCameraSettings GetConfiguredAOECameraSettings() const;
	bool HasPlayerController() const;
	double CalculateAOERadiusFromSkillData() const;
	float CalculateDamageMagnitude() const;
	bool ShouldDrawDebugDamageRadius() const;
	void DrawDebugDamageRadius(const TCHAR* Context, const FColor& CircleColor, const FColor& SphereColor) const;

	UFUNCTION()
	void HandleCancelInputPressed(float TimeWaited);

	UFUNCTION()
	void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetingMontageBlendOut();

	UFUNCTION()
	void HandleTargetingMontageInterrupted();

	UFUNCTION()
	void HandleTriggerMontageFinished();

	UFUNCTION()
	void HandleTriggerMontageInterrupted();

	UFUNCTION()
	void HandleMontageTriggerEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleLightningDamageDelayFinished();

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitInputPress> WaitCancelInputTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> TargetingMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitTargetData> WaitTargetDataTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> TriggerMontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitMontageTriggerTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitDelay> LightningDamageDelayTask;
};
