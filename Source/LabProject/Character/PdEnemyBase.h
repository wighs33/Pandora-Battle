#pragma once

#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Character/PdCharacterBase.h"
#include "GameplayTagContainer.h"
#include "PdEnemyBase.generated.h"

class UPdAbilitySystemComponent;
class UItemDefinition;
class UUserWidget;

UCLASS(Blueprintable)
class LABPROJECT_API APdEnemyBase : public APdCharacterBase, public ITargetingInterface
{
	GENERATED_BODY()

public:
	APdEnemyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetEnemyAbilitySystemComponent() const { return AbilitySystemComponent.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!AI|Targeting")
	void SetAttackTarget(AActor* InAttackTarget);

	UFUNCTION(BlueprintPure, Category = "!AI|Targeting")
	AActor* GetCachedAttackTarget() const { return AttackTarget.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!AI|Combat")
	void Attack();

	virtual AActor* GetAttackTarget_Implementation() const override;

protected:
	void ConfigureEnemyAvatarWidget();
	void HandleInitialCombatDelayElapsed();
	void StartAttackTimer();
	bool EquipStartingWeapon();
	bool MoveToAttackTarget(AActor* CurrentAttackTarget);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPdAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UUserWidget> DefaultEnemyAvatarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!AI|Targeting")
	TObjectPtr<AActor> AttackTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!AI|Targeting")
	bool bUsePlayerCharacterFallbackTarget = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UItemDefinition> StartingWeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat")
	FGameplayTagContainer AttackAbilityTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat")
	bool bStartCombatOnPossess = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat")
	bool bAttackImmediatelyAfterStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float InitialCombatDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AttackInterval = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat")
	bool bMoveToTargetBeforeAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AttackRange = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Combat", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MoveAcceptanceRadius = 140.0f;

	FTimerHandle InitialCombatTimerHandle;
	FTimerHandle AttackTimerHandle;
};
