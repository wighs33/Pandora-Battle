#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Definition/AbilitySystem/SkillAreaSettings.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "SkillTargetedAreaAction.generated.h"

class AGameplayAbilityTargetActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputPress;
class UAbilityTask_WaitTargetData;
class UAbilitySystemComponent;
class UAnimMontage;
class UGameplayEffect;
class UMaterialInterface;

/** 지면을 조준하고 몽타주 이벤트에 맞춰 범위 피해를 적용한다. */
UCLASS(meta = (DisplayName = "Targeted Area"))
class LABPROJECT_API USkillTargetedAreaAction : public USkillAction
{
	GENERATED_BODY()

public:

	/** 지면을 조준하고 몽타주 이벤트에 맞춰 범위 피해를 적용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ShowOnlyInnerProperties))
	FSkillAreaSettings Settings;

	UFUNCTION(BlueprintCallable, Category = "Skill|AOE")
	void StartTargeting();

	UFUNCTION(BlueprintCallable, Category = "Skill|AOE")
	void LoopTargetingAnimation();

	UFUNCTION(BlueprintCallable, Category = "Skill|AOE")
	void ConfirmStrike();

	UFUNCTION(BlueprintCallable, Category = "Skill|AOE")
	void AOEDamage();

protected:
	virtual void OnStart() override;

	virtual void OnStop() override;

	UPROPERTY(Transient)
	bool bIsWaitingTargetData = false;

	UPROPERTY(Transient)
	bool bStrikeTriggered = false;

	UPROPERTY(Transient)
	bool bStrikeConfirmed = false;

	UPROPERTY(Transient)
	bool bWaitingLightningDamage = false;

	UPROPERTY(Transient)
	double CachedAOERadius = 0.0;

	UPROPERTY(Transient)
	FVector ConfirmedAOELocation = FVector::ZeroVector;

	TSet<FObjectKey> HitActorKeys;

	TArray<FOverlapResult> AOEOverlapResults;

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
	bool ResolveFallbackAOELocation(FVector& OutGroundLocation) const;
	FVector ResolveConfirmedAOELocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint) const;
	bool TryValidateServerAOELocation(
		const FHitResult& ClientHitResult,
		const FVector& TargetDataEndPoint,
		FVector& OutValidatedLocation);
	FGameplayEffectSpecHandle MakeDamageEffectSpec() const;

	// 몽타주·데칼의 기본값 선택과 디버그 표시 규칙
	UAnimMontage* GetConfiguredTargetingMontage() const;
	UAnimMontage* GetConfiguredTriggerMontage() const;
	TSubclassOf<AGameplayAbilityTargetActor> GetConfiguredTargetActorClass() const;
	UMaterialInterface* GetConfiguredTargetingDecal() const;
	double GetConfiguredTargetingDecalSize() const;
	bool GetConfiguredDebugTargeting() const;
	FGameplayTag GetConfiguredMontageTriggerEventTag() const;
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
