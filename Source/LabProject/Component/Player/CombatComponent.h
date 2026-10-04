#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "Component/Player/UnarmedAttackSweep.h"
#include "Definition/Common/CombatSettings.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayPrediction.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UObject/ObjectKey.h"
#include "CombatComponent.generated.h"

class AActor;
class ACharacterBase;
class APdPlayer;
class AWeaponBase;
class FContentLease;
class UAbilitySystemComponent;
class UGameplayAbility;
class UAnimMontage;
class UGameplayEffect;
class UPdAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FAttackData;

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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual void BeginPlay() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void StartPrimaryAttack();
	void StopPrimaryAttack();
	void StartAim();
	void StopAim();

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetWeaponDamageSourceMagnitude() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetStrengthAdjustedWeaponDamageMagnitude(float SourceStrength) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Combat")
	bool ApplyWeaponDamageToTarget(AActor* TargetActor);

	/** 장착 중인 무기로 지금 낼 피해를 정한다. 화살처럼 나중에 맞는 공격은 발사할 때 이 값을 담아 둔다. */
	bool BuildWeaponDamage(const AWeaponBase& Weapon, PdDamageRules::FOutgoingDamage& OutDamage) const;
	/** 미리 정한 피해를 대상에게 적용한다. 대상·팀·권한은 적용하는 순간에 확인한다. */
	bool ApplyOutgoingDamageToTarget(AActor* TargetActor, const PdDamageRules::FOutgoingDamage& Damage,
		UObject* SourceObject, AActor* DamageCauser);

	bool CanAffordRangedWeaponAttackStamina() const;
	bool TryCommitRangedWeaponAttackStamina();

	void SetActiveComboDamageMultiplier(float DamageMultiplier);

	void SetTemporaryWeaponDamageBonus(UObject* SourceObject, float DamageBonus);
	void ClearTemporaryWeaponDamageBonus(UObject* SourceObject);

	bool GetUnarmedAttackData(FAttackData& OutAttackData) const;
	void PlayUnarmedComboWindowStartEffect() const;
	void SetUnarmedAttackTraceEnabledForSection(bool bEnabled, FName AttackSectionName);
	void ResetUnarmedAttackHitTracking();

	void ApplySettings(const FCombatDamageSettings& DamageSettings, const FUnarmedCombatSettings& UnarmedSettings);

private:
	float GetTemporaryWeaponDamageBonus() const;
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerRequestNextComboInput(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey ActivationKey, FName ClientExpectedSectionName);

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem);
	void HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem);
	void HandleUnarmedAttackMontagePreloadComplete();
	void HandleAutomaticFireTick();
	void HandleAttackSpeedChanged(const FOnAttributeChangeData& Data);
	void PerformUnarmedAttackTrace();

	UFUNCTION()
	void OnRep_TemporaryWeaponDamageBonus();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* GetCharacter() const;
	void BindAbilitySystem(UPdAbilitySystemComponent* AbilitySystem);
	APdPlayer* GetPlayerOwner() const;
	AWeaponBase* GetCurrentWeaponActor() const;
	UAbilitySystemComponent* GetPlayerAbilitySystemComponent() const;

	UGameplayAbility* ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	UAnimMontage* GetCachedUnarmedAttackMontage() const;
	void BeginUnarmedAttackMontagePreload();
	void ReleaseUnarmedAttackMontagePreload();

	void ProcessAttackInput();
	bool IsPrimaryAttackBlockedByAbilityTags() const;
	bool TryProcessWeaponPrimaryAttack(APdPlayer* PlayerCharacter, AWeaponBase* WeaponActor) const;
	bool ShouldUseRangedAttackAbility(const AWeaponBase* WeaponActor) const;
	FGameplayTag GetSelectedAttackAbilityTag(const AWeaponBase* WeaponActor) const;
	void RequestNextAttackSection(UGameplayAbility* ActiveAttackAbility);
	bool TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	bool TryStartAutomaticFire();
	void StartUnarmedAttackTrace();
	void StopUnarmedAttackTrace();
	bool ApplyUnarmedDamageToTarget(AActor* TargetActor);
	float GetUnarmedDamageSourceMagnitude() const;
	float CalculateStrengthAdjustedWeaponDamage(float WeaponDamage, float SourceStrength) const;
	bool HasCombatAuthority() const;
	void RefreshTemporaryWeaponDamageBonus();

	bool ApplyAttackDamageToTarget(AActor* TargetActor, float RawDamage, UObject* SourceObject, AActor* DamageCauser, bool bAllowHitReact);
	bool BuildOutgoingDamage(float RawDamage, bool bAllowHitReact, PdDamageRules::FOutgoingDamage& OutDamage) const;

protected:
	// DamageSpecTags는 Spec의 동적 애셋 태그로 붙어 대상 AttributeSet이 치명타·피격 반응 여부를 읽는다.
	bool ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject,
		AActor* InstigatorActor, AActor* EffectCauserActor, const FGameplayTagContainer& DamageSpecTags) const;

public:
	FOnCombatDamageBonusChanged OnDamageBonusChanged;

protected:
	UPROPERTY(Transient)
	FCombatDamageSettings CombatDamageSettings;

	UPROPERTY(Transient)
	FUnarmedCombatSettings UnarmedCombatSettings;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedUnarmedAttackMontage;

	TSharedPtr<FContentLease> UnarmedAttackMontageLease;
	FUnarmedAttackSweep UnarmedAttackSweep;
	TMap<FObjectKey, float> TemporaryWeaponDamageBonuses;

	UPROPERTY(Transient)
	float ActiveComboDamageMultiplier = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_TemporaryWeaponDamageBonus)
	float ReplicatedTemporaryWeaponDamageBonus = 0.0f;

	bool bPrimaryAttackHeld = false;
	bool bEndingPlay = false;
	double LastPrimaryAttackRequestTime = 0.0;
	FAbilitySystemReadySubscription AbilitySystemSubscription;
	TWeakObjectPtr<UPdAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle AttackSpeedChangedDelegateHandle;
	FTimerHandle UnarmedAttackTraceTimerHandle;
	FTimerHandle AutomaticFireTimerHandle;
};
