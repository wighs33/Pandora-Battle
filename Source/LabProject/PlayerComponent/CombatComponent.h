#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "CombatComponent.generated.h"

class AActor;
class APdCharacterBase;
class APdPlayer;
class AWeaponBase;
class UAbilitySystemComponent;
class UAttackAbility;
class UGameplayEffect;
class UPdAbilitySystemComponent;
class UControllerUiComponent;
struct FGameplayAttribute;
struct FGameplayAbilitySpec;

/**
 * <전투 처리 컴포넌트>
 * - 공격 데미지 스탯을 조회합니다.
 * - OutgoingDamage / IncomingDamage GE를 통해 데미지를 처리합니다.
 * - 대상 HitReact Ability 활성화를 담당합니다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 전투 컴포넌트 기본 상태를 초기화합니다. */
	UCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartPrimaryAttack();
	void StopPrimaryAttack();
	void StartAim();
	void StopAim();
	void StopAutomaticFire();

	/** 현재 무기 데미지 기준 Attribute 값을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetWeaponDamageSourceMagnitude();

	/** 현재 무기 데미지를 대상에게 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Combat")
	bool ApplyWeaponDamageToTarget(AActor* TargetActor);

	void RefreshCachedReferences();

protected:
	// Timing hooks
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestAttackJumpSection(FName RequestedSectionName);

	APdPlayer* GetPlayerOwner() const;
	UControllerUiComponent* GetControllerUiComponent() const;
	AWeaponBase* GetCurrentWeaponActor() const;
	UAbilitySystemComponent* GetPlayerAbilitySystemComponent() const;
	FGameplayTag GetAttackAbilityTag() const;
	FGameplayTag GetRangedAttackAbilityTag() const;
	FGameplayTag GetWeaponDamageSourceTag() const;
	FGameplayTag GetHitReactAbilityTag() const;

	FGameplayTagContainer MakeAbilityTagContainer(const FGameplayTag& AbilityTag) const;
	const FGameplayAbilitySpec* FindActiveAbilitySpec(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	UAttackAbility* ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;

	void ProcessAttackInput();
	bool TryProcessWeaponPrimaryAttack(APdPlayer* PlayerCharacter, AWeaponBase* WeaponActor) const;
	bool ShouldUseRangedAttackAbility(const AWeaponBase* WeaponActor) const;
	FGameplayTag GetSelectedAttackAbilityTag(const AWeaponBase* WeaponActor) const;
	void RequestNextAttackSection(UAttackAbility* ActiveAttackAbility);
	void TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;
	bool TryStartAutomaticFire();
	void HandleAutomaticFireTick();

protected:
	// Query helpers
	/** 무기 데미지 소스 태그를 실제 Attribute로 해석합니다. */
	bool ResolveWeaponDamageAttribute(FGameplayAttribute& OutAttribute) const;

	/** 데미지 GE에 SetByCaller 데미지 값을 넣어 적용합니다. */
	bool ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<APdCharacterBase> CachedOwner;

	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> CachedASC;

	/** 공격자 OutgoingDamage에 적용할 GE입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> OutgoingDamageEffectClass;

	/** 피해자 IncomingDamage에 적용할 GE입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> IncomingDamageEffectClass;

	FTimerHandle AutomaticFireTimerHandle;
};
