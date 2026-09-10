#pragma once

#include "CoreMinimal.h"
#include "Definition/Common/CombatSettings.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayPrediction.h"
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
class UGameplayEffect;
class UPdAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FAttackData;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE(FOnCombatDamageBonusChanged);

/**
 * 캐릭터의 기본 공격 입력과 실제 타격을 연결한다.
 *
 * 장착 정보는 EquipmentComponent에서 읽고, 능력의 콤보 타이밍에 맞춰 맨손 판정과
 * 서버 피해를 적용한다. 피해 GE 설정은 플레이어·AI가 공통으로 전달한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------
	void StartPrimaryAttack();
	void StopPrimaryAttack();
	void StartAim();
	void StopAim();
	void StopAutomaticFire();

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetWeaponDamageSourceMagnitude() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetStrengthAdjustedWeaponDamageMagnitude(float SourceStrength) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Combat")
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

	void ApplySettings(const FCombatDamageSettings& DamageSettings, const FUnarmedCombatSettings& UnarmedSettings);
	FOnCombatDamageBonusChanged OnDamageBonusChanged;
	void RefreshCachedReferences();

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestNextComboInput(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey ActivationKey, FName ClientExpectedSectionName);

	APdPlayer* GetPlayerOwner() const;
	APdHUD* GetPdHUD() const;
	AWeaponBase* GetCurrentWeaponActor() const;
	UAbilitySystemComponent* GetPlayerAbilitySystemComponent() const;
	FGameplayTag GetAttackAbilityTag() const;
	FGameplayTag GetPunchAbilityTag() const;
	FGameplayTag GetRangedAttackAbilityTag() const;
	FGameplayTag GetWeaponDamageSourceTag() const;

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
	void RequestNextAttackSection(UAttackAbility* ActiveAttackAbility);
	bool TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	bool TryStartAutomaticFire();
	void HandleAutomaticFireTick();
	void HandleAttackSpeedChanged(const FOnAttributeChangeData& Data);
	void StartUnarmedAttackTrace();
	void StopUnarmedAttackTrace();
	void PerformUnarmedAttackTrace();
	bool ApplyUnarmedDamageToTarget(AActor* TargetActor);
	float GetUnarmedDamageSourceMagnitude() const;
	float CalculateStrengthAdjustedWeaponDamage(float WeaponDamage, float SourceStrength) const;
	bool HasCombatAuthority() const;
	void RefreshTemporaryWeaponDamageBonus();

	UFUNCTION()
	void OnRep_TemporaryWeaponDamageBonus();

	bool ApplyAttackDamageToTarget(AActor* TargetActor, float RawDamage, UObject* SourceObject, AActor* DamageCauser, bool bAllowHitReact);

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
	FCombatDamageSettings CombatDamageSettings;

	UPROPERTY(Transient)
	FUnarmedCombatSettings UnarmedCombatSettings;

	TSet<TWeakObjectPtr<AActor>> HitActorsInCurrentUnarmedAttack;

	UPROPERTY(Transient)
	FName TrackedUnarmedAttackSectionName = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedUnarmedAttackMontage;

	TSharedPtr<FStreamableHandle> UnarmedAttackMontagePreloadHandle;
	TArray<TEnumAsByte<EObjectTypeQuery>> CachedUnarmedAttackObjectTypes;
	TArray<AActor*> UnarmedAttackActorsToIgnore;
	TArray<FHitResult> UnarmedAttackHitResults;
	TArray<FHitResult> InterpolatedUnarmedHitResults;
	TArray<FVector> PreviousUnarmedAttackTraceStartLocations;
	TArray<FVector> PreviousUnarmedAttackTraceEndLocations;
	TArray<uint8> PreviousUnarmedAttackTraceValid;
	TMap<FObjectKey, float> TemporaryWeaponDamageBonuses;

	UPROPERTY(Transient)
	float ActiveComboDamageMultiplier = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_TemporaryWeaponDamageBonus)
	float ReplicatedTemporaryWeaponDamageBonus = 0.0f;

	bool bPrimaryAttackHeld = false;
	bool bEndingPlay = false;
	bool bUnarmedAttackTraceActive = false;
	uint32 UnarmedAttackTraceGeneration = 0;
	double LastPrimaryAttackRequestTime = 0.0;
	FDelegateHandle AttackSpeedChangedDelegateHandle;
	FTimerHandle UnarmedAttackTraceTimerHandle;
	FTimerHandle AutomaticFireTimerHandle;
};
