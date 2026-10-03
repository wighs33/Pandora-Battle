#pragma once

#include "Interface/TargetingInterface.h"
#include "Character/CharacterBase.h"
#include "EnemyBase.generated.h"

class UPdAbilitySystemComponent;
class UBasicAttributeSet;
class UEnemyBaseDefinition;
class UEnemyCombatComponent;
class UEnemyTrainingBotComponent;
class UCombatComponent;
class UItemDefinition;
class UUserWidget;
class UAnimMontage;
class FContentLease;
struct FAttackData;
struct FEnemyCombatSettings;
struct FEnemyTrainingBotSettings;

/**
 * 적 캐릭터의 공통 진입점을 제공한다.
 *
 * 대상 지정·전투 초기화·공격 상태는 UEnemyCombatComponent가 담당한다.
 * 훈련 봇의 피격 반응·무기 교체·리스폰 상태는 UEnemyTrainingBotComponent가 담당한다.
 * 기존 블루프린트·StateTree·GAS·네트워크 연동은 이 액터에서 유지한다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API AEnemyBase
	: public ACharacterBase
	, public ITargetingInterface
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	// Interface Implementations ---------------------------------------------------------------------------------------
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual AActor* GetAttackTarget_Implementation() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	AEnemyBase(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetEnemyAbilitySystemComponent() const
	{
		return AbilitySystemComponent.Get();
	}

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	UEnemyCombatComponent* GetEnemyCombatComponent() const
	{
		return EnemyCombatComponent;
	}

	UFUNCTION(BlueprintPure, Category = "!AI|Training Bot")
	UEnemyTrainingBotComponent* GetEnemyTrainingBotComponent() const
	{
		return EnemyTrainingBotComponent;
	}

	UFUNCTION(BlueprintCallable, Category = "!AI|Targeting")
	void SetAttackTarget(AActor* InAttackTarget);

	UFUNCTION(BlueprintPure, Category = "!AI|Targeting")
	AActor* GetCachedAttackTarget() const;

	UFUNCTION(BlueprintCallable, Category = "!AI|Combat")
	virtual void Attack();

	UFUNCTION(BlueprintCallable, Category = "!AI|Combat")
	void SetAttackEnabled(bool bInAttackEnabled);

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	bool IsAttackEnabled() const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	bool IsAttackAbilityActive() const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	virtual bool IsAttackInProgress() const;

	UFUNCTION(BlueprintCallable, Category = "!AI|Combat")
	bool RequestMoveToAttackTarget(AActor* InAttackTarget);

	UFUNCTION(BlueprintCallable, Category = "!AI|Combat")
	void InitializeBehaviorTreeCombat();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AI|Combat")
	bool RequestTrainingBotWeaponChange(
		const UItemDefinition* WeaponDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AI|Combat")
	bool RequestTrainingBotUnarmed();

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	const UItemDefinition*
	GetCurrentOrStartingEnemyWeaponDefinition() const;

	virtual bool GetFallbackAttackData(FAttackData& OutAttackData) const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	float GetAttackStartDistance() const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	bool IsUsingRangedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	bool IsUsingGunWeapon() const;

	UFUNCTION(BlueprintPure, Category = "!AI|Combat")
	float GetAttackDistanceToActor(const AActor* InActor) const;

	UFUNCTION(BlueprintPure, Category = "!AI|Targeting")
	bool IsActorValidAttackTarget(AActor* InActor) const;

	UFUNCTION(BlueprintCallable, Category = "!AI|Targeting")
	void SetUseNearestPlayerWhenTargetUnset(bool bInUseNearestPlayer);

protected:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayTrainingHitReactMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetTrainingBotRespawnVisuals(
		const FTransform& RespawnTransform);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayTrainingBotUnequipMontage(
		UAnimMontage* UnequipMontage,
		float PlayRate);

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit = false,
		bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr,
		AActor* DamageCauser = nullptr) override;
	virtual void HandleDeath_Implementation() override;

protected:
	virtual void HandleCharacterRuntimeInitialized() override;
	void HandleEnemyDefinitionPreloaded();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual TSubclassOf<UUserWidget> ResolveHealthBarWidgetClass(
		const UWidgetClassDefinition* WidgetDefinition) const override;
	virtual bool ShouldApplyResolvedHealthBarWidgetClass(
		UClass* CurrentWidgetClass,
		TSubclassOf<UUserWidget> ResolvedWidgetClass) const override;
	virtual bool IsAdditionalCharacterRuntimeContentReady() const override;

	virtual void ModifyResolvedEnemySettings(
		FEnemyCombatSettings& CombatSettings,
		FEnemyTrainingBotSettings& TrainingBotSettings) const;
	virtual void ApplyResolvedEnemyDefinition(
		const UEnemyBaseDefinition* ResolvedDefinition);

	bool IsTrainingHitStunned() const;
	bool MoveToAttackTarget(AActor* CurrentAttackTarget);
	bool IsDefaultAttributeSetupComplete() const;

	void ApplyEnemyDefinition();
	void BeginEnemyDefinitionPreload();
	void InitializeEnemyRuntime();

protected:
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Definition")
	TSoftObjectPtr<UEnemyBaseDefinition> EnemyDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyBaseDefinition> LoadedEnemyDefinition;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AbilitySystem",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	// 플레이어의 PlayerState처럼 ASC 소유 액터의 기본 서브오브젝트로 두어, 클라이언트에서도 ASC 초기화 시점에 속성이 함께 준비된다.
	UPROPERTY(VisibleAnywhere, Category = "!AbilitySystem")
	TObjectPtr<UBasicAttributeSet> BasicAttributeSet;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AI|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AI|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AI|Training Bot",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyTrainingBotComponent> EnemyTrainingBotComponent;

	TSharedPtr<FContentLease> EnemyDefinitionLease;
	bool bEnemyDefinitionReady = false;
	bool bEnemyRuntimeInitialized = false;

private:
	friend class UEnemyCombatComponent;
	friend class UEnemyTrainingBotComponent;
};
