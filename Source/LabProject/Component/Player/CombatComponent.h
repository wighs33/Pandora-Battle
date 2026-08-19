#pragma once

#include "CoreMinimal.h"
#include "Definition/Player/PlayerPawnDefinition.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "CombatComponent.generated.h"

class AActor;
class APdHUD;
class ACharacterBase;
class APdPlayer;
class AWeaponBase;
class UAbilitySystemComponent;
class UAttackAbility;
class UAnimMontage;
class UBasicAttributeSet;
class UGameplayEffect;
class UNiagaraSystem;
class UPdAbilitySystemComponent;
class UPlayerPawnDefinition;
struct FGameplayAbilitySpec;
struct FAttackData;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartPrimaryAttack();
	void StopPrimaryAttack();
	void StartAim();
	void StopAim();
	void StopAutomaticFire();

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetWeaponDamageSourceMagnitude();

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetStrengthAdjustedWeaponDamageMagnitude(float SourceStrength);

	UFUNCTION(BlueprintCallable, Category = "!Combat")
	bool ApplyWeaponDamageToTarget(AActor* TargetActor);

	bool CanAffordRangedWeaponAttackStamina() const;
	bool TryCommitRangedWeaponAttackStamina();

	void SetActiveComboDamageMultiplier(float DamageMultiplier);

	void SetTemporaryWeaponDamageBonus(UObject* SourceObject, float DamageBonus);
	void ClearTemporaryWeaponDamageBonus(UObject* SourceObject);
	float GetTemporaryWeaponDamageBonus() const;

	bool GetUnarmedAttackData(FAttackData& OutAttackData) const;
	void PlayUnarmedComboWindowStartEffect() const;
	void SetUnarmedAttackTraceEnabledForSection(bool bEnabled, FName AttackSectionName);
	void ResetUnarmedAttackHitTracking();

	void ApplyDefinition(const UPlayerPawnDefinition* Definition);
	void RefreshCachedReferences();

protected:
	// Timing hooks
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestAttackJumpSection(FName ClientExpectedSectionName, FGameplayTag AbilityTag);

	APdPlayer* GetPlayerOwner() const;
	APdHUD* GetPdHUD() const;
	AWeaponBase* GetCurrentWeaponActor() const;
	UAbilitySystemComponent* GetPlayerAbilitySystemComponent() const;
	FGameplayTag GetAttackAbilityTag() const;
	FGameplayTag GetPunchAbilityTag() const;
	FGameplayTag GetRangedAttackAbilityTag() const;
	FGameplayTag GetWeaponDamageSourceTag() const;

	FGameplayTagContainer MakeAbilityTagContainer(const FGameplayTag& AbilityTag) const;
	UAttackAbility* ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	UAnimMontage* GetCachedUnarmedAttackMontage() const;
	void BeginUnarmedAttackMontagePreload();
	void HandleUnarmedAttackMontagePreloadComplete();
	void ReleaseUnarmedAttackMontagePreload();

	void ProcessAttackInput();
	bool IsPrimaryAttackBlockedByAbilityTags() const;
	bool TryProcessWeaponPrimaryAttack(APdPlayer* PlayerCharacter, AWeaponBase* WeaponActor) const;
	bool ShouldUseRangedAttackAbility(const AWeaponBase* WeaponActor) const;
	FGameplayTag GetSelectedAttackAbilityTag(const AWeaponBase* WeaponActor) const;
	void RequestNextAttackSection(UAttackAbility* ActiveAttackAbility, const FGameplayTag& AbilityTag);
	bool TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	bool TryStartAutomaticFire();
	void HandleAutomaticFireTick();
	void StartUnarmedAttackTrace(bool bResetHitActors = true);
	void StopUnarmedAttackTrace();
	void PerformUnarmedAttackTrace();
	bool ApplyUnarmedDamageToTarget(AActor* TargetActor);
	float GetUnarmedDamageSourceMagnitude() const;
	float CalculateStrengthAdjustedWeaponDamage(float WeaponDamage, float SourceStrength) const;
	float GetActionStaminaCost() const;
	bool HasCombatAuthority() const;
	void CompactTemporaryWeaponDamageBonuses();

protected:
	bool ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject,
		AActor* InstigatorActor = nullptr, AActor* EffectCauserActor = nullptr) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> CachedOwner;

	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> CachedASC;

	UPROPERTY(Transient)
	FUnarmedCombatSettings UnarmedCombatSettings;

	TSet<TObjectPtr<AActor>> HitActorsInCurrentUnarmedAttack;

	UPROPERTY(Transient)
	FName TrackedUnarmedAttackSectionName = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedUnarmedAttackMontage;

	TSharedPtr<FStreamableHandle> UnarmedAttackMontagePreloadHandle;
	TArray<TEnumAsByte<EObjectTypeQuery>> CachedUnarmedAttackObjectTypes;
	TArray<AActor*> UnarmedAttackActorsToIgnore;
	TArray<FHitResult> UnarmedAttackHitResults;
	TArray<FVector> PreviousUnarmedAttackTraceStartLocations;
	TArray<FVector> PreviousUnarmedAttackTraceEndLocations;
	TArray<uint8> PreviousUnarmedAttackTraceValid;
	TMap<FObjectKey, float> TemporaryWeaponDamageBonuses;

	UPROPERTY(Transient)
	float ActiveComboDamageMultiplier = 1.0f;

	FTimerHandle UnarmedAttackTraceTimerHandle;
	FTimerHandle AutomaticFireTimerHandle;
};
