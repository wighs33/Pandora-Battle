#pragma once

#include "CoreMinimal.h"
#include "Character/EnemyBase.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayTagContainer.h"
#include "MonsterCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;
class UPrimitiveComponent;
class URewardDefinition;
class APdPlayerState;
struct FStreamableHandle;

/**
 * 몬스터의 공격 판정과 피격·사망 처리를 담당한다.
 *
 * 행동 판단은 AIController와 StateTree에 맡기고, 사망 시 확정한 보상은 플레이어 보상 컴포넌트에 전달한다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API AMonsterCharacter : public AEnemyBase
{
	GENERATED_BODY()

public:
	AMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Attack() override;
	virtual bool IsAttackInProgress() const override;
	virtual bool GetFallbackAttackData(FAttackData& OutAttackData) const override;
	void StopMonsterAttack();
	bool IsMonsterReadyForAI() const;
	const UEnemyBaseDefinition* GetMonsterDefinition() const { return LoadedEnemyDefinition; }

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDamageTaken(float DamageAmount, bool bCriticalHit = false, bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr, AActor* DamageCauser = nullptr) override;
	virtual void HandleDeath_Implementation() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual bool IsAdditionalCharacterRuntimeContentReady() const override;
	virtual void HandleCharacterRuntimeInitialized() override;
	virtual void ApplyResolvedEnemyDefinition(const UEnemyBaseDefinition* ResolvedDefinition) override;
	virtual void ModifyResolvedEnemySettings(FEnemyCombatSettings& CombatSettings, FEnemyTrainingBotSettings& TrainingBotSettings) const override;
	virtual FVector GetDamageIndicatorWorldLocation() const override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> AttackComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> LegacyDamageComponent;

	//------------------------------------------------------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AI|Monster|Collision")
	FName DamageComponentName = TEXT("DamageSphere");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AI|Monster|Collision")
	FName AttackComponentName = TEXT("AttackSphere");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (ClampMin = "0.0"))
	float AttackDamageMagnitude = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AttackSphereActiveDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (Categories = "Data"))
	FGameplayTag ContactDamageDataTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UAnimMontage> MonsterAttackMontage;

	UPROPERTY(Transient)
	FMonsterPresentationSettings MonsterPresentationSettings;

	UPROPERTY(Transient)
	float ResolvedMonsterMaxHealth = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Reward")
	TSoftObjectPtr<URewardDefinition> MonsterRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Death", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DeathDestroyDelay = 8.0f;

private:
	void CacheCollisionComponents();
	bool IsValidMonsterDamageTarget(const AActor* OtherActor) const;
	void ApplyAttackDamageToCharacter(ACharacterBase* TargetCharacter);
	bool ApplyMonsterDamageToCharacter(ACharacterBase* TargetCharacter);
	void DeactivateDamageSphere();
	void ActivateAttackSphere();
	void DeactivateAttackSphere();
	void FinishMonsterDeath();
	void ApplyMonsterHealthDefaults();
	void BeginMonsterContentPreload();
	void HandleMonsterContentPreloadComplete();
	bool TryPlayMonsterAttackMontage();
	bool PlayMonsterAttackMontageLocal(UAnimMontage* AttackMontage, float PlayRate);
	void TryPlayMonsterHitReactMontage(float DamageAmount, bool bAllowHitReact);
	void HandleFrozenTagChanged(FGameplayTag Tag, int32 NewCount);
	APdPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;

	UFUNCTION()
	void HandleAttackComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterAttackMontage(UAnimMontage* AttackMontage, float PlayRate);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopMonsterAttack();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterHitReactMontage(UAnimMontage* HitReactMontage, float PlayRate);

	FTimerHandle AttackSphereTimerHandle;
	FTimerHandle DeathDestroyTimerHandle;
	FDelegateHandle FrozenTagChangedHandle;
	TSet<TWeakObjectPtr<AActor>> AttackHitActorsThisSwing;
	TWeakObjectPtr<APdPlayerState> LastDamagingPlayerState;
	TWeakObjectPtr<UAnimInstance> AttackAnimInstance;
	TSharedPtr<FStreamableHandle> MonsterContentPreloadHandle;
	bool bMonsterContentPreloadStarted = false;
	bool bMonsterContentReady = false;
	bool bMonsterAttackActive = false;
	bool bAttackWindowOpen = false;
	bool bDying = false;
};
