#pragma once

#include "CoreMinimal.h"
#include "Character/EnemyBase.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayTagContainer.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "MonsterCharacter.generated.h"

class UAnimMontage;
struct FStreamableHandle;
class UPrimitiveComponent;
class URewardDefinition;
class APdPlayerState;

UCLASS(Blueprintable)
class LABPROJECT_API AMonsterCharacter : public AEnemyBase
{
	GENERATED_BODY()

public:
	AMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Attack() override;
	virtual bool IsAttackInProgress() const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit = false,
		bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr,
		AActor* DamageCauser = nullptr) override;
	virtual void HandleDeath_Implementation() override;
	virtual bool GetFallbackAttackData(FAttackData& OutAttackData) const override;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void HandleCharacterRuntimeInitialized() override;
	virtual void ApplyResolvedEnemyDefinition(
		const UEnemyBaseDefinition* ResolvedDefinition) override;
	virtual void ModifyResolvedEnemySettings(
		FEnemyCombatSettings& CombatSettings,
		FEnemyTrainingBotSettings& TrainingBotSettings) const override;

	UFUNCTION()
	void HandleAttackComponentBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPrimitiveComponent* ResolveDamageComponent() const;
	UPrimitiveComponent* ResolveAttackComponent() const;
	UPrimitiveComponent* ResolvePrimitiveComponentByName(FName ComponentName) const;
	bool IsValidMonsterDamageTarget(const AActor* OtherActor) const;
	void ApplyAttackDamageToCharacter(ACharacterBase* TargetCharacter);
	bool ApplyMonsterDamageToCharacter(ACharacterBase* TargetCharacter, float DamageMagnitude, float KnockbackStrength);
	void DeactivateDamageSphere();
	void ActivateAttackSphere();
	void DeactivateAttackSphere();
	void BeginMonsterDeath();
	void FinishMonsterDeath();
	void ApplyMonsterHealthDefaults();
	void BeginMonsterContentPreload();
	void HandleMonsterContentPreloadComplete();
	void ReleaseMonsterContentPreload();
	UAnimMontage* ResolveMonsterAttackMontage() const;
	UAnimMontage* ResolveMonsterHitReactMontage() const;
	bool TryPlayMonsterAttackMontage();
	void TryPlayMonsterHitReactMontage(float DamageAmount, bool bAllowHitReact);
	void RememberDamageSource(AActor* DamageInstigator, AActor* DamageCauser);
	virtual FVector GetDamageIndicatorWorldLocation() const override;
	APdPlayerState* ResolveRewardPlayerState() const;
	APdPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;
	const URewardDefinition* GetMonsterRewardDefinition() const;
	void GrantDefeatRewards();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterAttackMontage(UAnimMontage* AttackMontage, float PlayRate);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterHitReactMontage(UAnimMontage* HitReactMontage, float PlayRate);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayMonsterDeathPresentation(UAnimMontage* InDeathMontage);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Reward",
		meta = (ToolTip = "Required. The single source for this monster's defeat experience and Soul Dust rewards."))
	TSoftObjectPtr<URewardDefinition> MonsterRewardDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Death", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DeathDestroyDelay = 8.0f;

private:
	FTimerHandle AttackSphereTimerHandle;
	FTimerHandle DeathDestroyTimerHandle;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> AttackHitActorsThisSwing;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastDamageInstigator;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastDamageCauser;

	UPROPERTY(Transient)
	bool bDying = false;

	UPROPERTY(Transient)
	bool bDefeatRewardsGranted = false;

	TSharedPtr<FStreamableHandle> MonsterContentPreloadHandle;
	bool bMonsterContentPreloadStarted = false;
	bool bMonsterContentLoadPending = false;
	bool bAttackRequestedWhileContentLoading = false;
	bool bDefeatRewardGrantPending = false;
	bool bDestroyAfterContentLoad = false;
};
