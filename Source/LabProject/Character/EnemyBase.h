#pragma once

#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Character/CharacterBase.h"
#include "EnemyBase.generated.h"

class UPdAbilitySystemComponent;
class UEnemyBaseDefinition;
class UEnemyCombatComponent;
class UEnemyTrainingBotComponent;
class UItemDefinition;
class UUserWidget;
class UAnimMontage;
struct FAttackData;
struct FEnemyCombatSettings;
struct FEnemyTrainingBotSettings;

/**
 * Stable enemy facade.
 *
 * Targeting, combat bootstrapping, and attack state live in
 * UEnemyCombatComponent. Training-bot hit reaction, weapon swapping, and
 * respawn state live in UEnemyTrainingBotComponent. This actor keeps the
 * existing Blueprint, StateTree, GAS, and network surface intact.
 */
UCLASS(Blueprintable)
class LABPROJECT_API AEnemyBase
	: public ACharacterBase
	, public ITargetingInterface
{
	GENERATED_BODY()

public:
	AEnemyBase(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit = false,
		bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr,
		AActor* DamageCauser = nullptr) override;
	virtual void HandleDeath_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetEnemyAbilitySystemComponent() const
	{
		return AbilitySystemComponent.Get();
	}

	UFUNCTION(BlueprintPure, Category = "!AI|Definition")
	UEnemyBaseDefinition* GetEnemyDefinition() const;

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
	bool EquipEnemyWeaponDefinition(
		const UItemDefinition* WeaponDefinition);

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
	float GetAttackRange() const { return GetAttackStartDistance(); }

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

	virtual AActor* GetAttackTarget_Implementation() const override;

protected:
	virtual TSubclassOf<UUserWidget> ResolveHealthBarWidgetClass(
		const UWidgetClassDefinition* WidgetDefinition) const override;
	virtual bool ShouldApplyResolvedHealthBarWidgetClass(
		UClass* CurrentWidgetClass,
		TSubclassOf<UUserWidget> ResolvedWidgetClass) const override;
	virtual bool IsAdditionalCharacterRuntimeContentReady() const override;
	virtual void HandleCharacterRuntimeInitialized() override;

	virtual void ModifyResolvedEnemySettings(
		FEnemyCombatSettings& CombatSettings,
		FEnemyTrainingBotSettings& TrainingBotSettings) const;

	bool IsTrainingHitStunned() const;
	bool MoveToAttackTarget(AActor* CurrentAttackTarget);
	bool IsDefaultAttributeSetupComplete() const;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayTrainingHitReactMontage();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetTrainingBotRespawnVisuals(
		const FTransform& RespawnTransform);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayTrainingBotUnequipMontage(
		UAnimMontage* UnequipMontage,
		float PlayRate);

	void ApplyEnemyDefinition();
	void BeginEnemyDefinitionPreload();
	void HandleEnemyDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseEnemyDefinitionPreload();
	void InitializeEnemyRuntime();

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

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AI|Combat",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "!AI|Training Bot",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyTrainingBotComponent> EnemyTrainingBotComponent;

	/*
	 * Serialized Blueprint compatibility bridge.
	 *
	 * BP_Enemy, BP_TrainingBot, and BP_Bug currently store these original
	 * AEnemyBase properties. They are read once into the focused components
	 * during PreInitializeComponents so existing assets retain their exact
	 * behavior. New archetypes should use EnemyDefinition instead.
	 */
	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility",
		meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UItemDefinition> StartingWeaponDefinition;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility")
	bool bEnableTrainingBotHitReaction = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility")
	bool bStartCombatOnPossess = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility")
	bool bUseBehaviorTreeCombat = false;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility")
	bool bMoveToTargetBeforeAttack = true;

	UPROPERTY(
		EditDefaultsOnly,
		BlueprintReadOnly,
		Category = "!AI|Compatibility",
		meta = (
			ClampMin = "0.0",
			ForceUnits = "cm",
			DisplayName = "Attack Range"))
	float AttackStartDistance = 180.0f;

	TSharedPtr<FStreamableHandle> EnemyDefinitionLoadHandle;
	uint32 EnemyDefinitionLoadGeneration = 0;
	bool bEnemyDefinitionReady = false;
	bool bEnemyRuntimeInitialized = false;

private:
	friend class UEnemyCombatComponent;
	friend class UEnemyTrainingBotComponent;
};
