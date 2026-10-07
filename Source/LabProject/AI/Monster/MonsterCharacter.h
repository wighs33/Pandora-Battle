#pragma once

#include "CoreMinimal.h"
#include "AI/Monster/AttackTelegraphActor.h"
#include "Character/EnemyBase.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "GameplayTagContainer.h"
#include "MonsterCharacter.generated.h"

class UAnimInstance;
class UAnimMontage;
class UPrimitiveComponent;
class URewardDefinition;
class APdPlayerState;
class FContentLease;
struct FGameplayEventData;
struct FOnAttributeChangeData;

/** 몬스터 공격 하나. 대상과의 거리가 범위 안일 때만 고른다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMonsterAttackPattern
{
	GENERATED_BODY()

	/** 공격 피해 구간(AnimNotifyState_AttackDamageWindow)이 있으면 그 구간에만 판정을 열고, 없으면 시작하자마자 연다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UAnimMontage> Montage;

	/** 이 공격을 고르는 대상과의 거리(두 캡슐 가장자리 사이). 최대가 0이면 멀어도 고른다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistance = 0.0f;

	/** 바닥에 미리 그리는 공격 범위. 피해 구간이 시작될 때 가득 찬다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation")
	FAttackTelegraphSettings Telegraph;

	/** Distance가 범위 밖으로 벗어난 정도. 범위 안이면 0이다. */
	float GetDistanceOutsideRange(float Distance) const;
};

/** 보스 페이즈 소환. 체력이 처음으로 HealthRatio 미만이 되면 한 번 포효하며 졸개를 한꺼번에 부른다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FMonsterPhaseSummon
{
	GENERATED_BODY()

	/** 최대 체력 대비 비율. 0이면 소환하지 않는다. */
	UPROPERTY(EditAnywhere, Category = "!AI|Monster|Summon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthRatio = 0.0f;

	UPROPERTY(EditAnywhere, Category = "!AI|Monster|Summon", meta = (AssetBundles = "Server"))
	TSoftClassPtr<AMonsterCharacter> MinionClass;

	/** 부를 졸개 수. 개발 빌드에서는 pd.Monster.PhaseSummonCountOverride로 바꿔 부하를 키울 수 있다. */
	UPROPERTY(EditAnywhere, Category = "!AI|Monster|Summon", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** 소환할 때 재생하는 포효. 재생하는 동안은 공격처럼 다른 행동을 멈춘다. */
	UPROPERTY(EditAnywhere, Category = "!AI|Monster|Summon", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<UAnimMontage> RoarMontage;
};

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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	AMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Attack() override;
	virtual bool IsAttackInProgress() const override;
	virtual bool GetFallbackAttackData(FAttackData& OutAttackData) const override;
	virtual AActor* GetAttackTarget_Implementation() const override;

	/** 서버가 고른 공격 대상을 원격 화면에 알린다. 원격 화면은 이 값으로 고개를 돌린다. */
	void SetReplicatedAttackTarget(AActor* Target);
	void StopMonsterAttack();
	bool IsMonsterReadyForAI() const;
	const UEnemyBaseDefinition* GetMonsterDefinition() const { return LoadedEnemyDefinition; }
	float GetSightRadiusOverride() const { return SightRadiusOverride; }

	/** 공격 패턴 중 가장 먼 사거리. 사거리 상한이 없는 패턴이 있으면 0이다. */
	float GetMaxAttackPatternDistance() const;

	/** 아직 쓰지 않은 페이즈 소환이 있는지. */
	bool HasPendingPhaseSummon() const { return PhaseSummon.HealthRatio > 0.0f && !bPhaseSummonDone && !bDying; }

	/** 체력과 관계없이 페이즈 소환을 지금 시작한다. 한 생에 한 번만 하며, 이미 했으면 false다. */
	bool TriggerPhaseSummon();

	/** 몬스터 BP 변수로 둔 추적 범위(leash)를 읽고 쓴다. BP에 두 변수가 모두 있을 때만 true다. */
	bool SetLeashDistances(double MaxDistanceFromSpawnPoint, double MinDistanceToResumeRoaming);
	bool GetLeashDistances(double& OutMaxDistanceFromSpawnPoint, double& OutMinDistanceToResumeRoaming) const;

private:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterAttackMontage(UAnimMontage* AttackMontage, float PlayRate, AActor* Target);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopMonsterAttack();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayMonsterHitReactMontage(UAnimMontage* HitReactMontage, float PlayRate);

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void HandleDamageTaken(float DamageAmount, bool bCriticalHit = false, bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr, AActor* DamageCauser = nullptr) override;
	virtual void HandleDeath_Implementation() override;
	virtual void MulticastHandleDamageTaken_Implementation(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation) override;

protected:
	virtual void HandleCharacterRuntimeInitialized() override;

private:
	void DeactivateAttackSphere();
	void FinishMonsterDeath();
	void HandleMonsterContentPreloadComplete();
	void HandleFrozenTagChanged(FGameplayTag Tag, int32 NewCount);
	void HandleAttackDamageWindowEvent(const FGameplayEventData* Payload);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void HandleAttackComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual bool IsAdditionalCharacterRuntimeContentReady() const override;
	virtual void ApplyResolvedEnemyDefinition(const UEnemyBaseDefinition* ResolvedDefinition) override;
	virtual void ModifyResolvedEnemySettings(FEnemyCombatSettings& CombatSettings, FEnemyTrainingBotSettings& TrainingBotSettings) const override;
	virtual FVector GetDamageIndicatorWorldLocation() const override;
	virtual bool ShouldShowHealthBarAtFullHealth() const override { return bShowHealthBarAtFullHealth; }
	virtual bool ShouldUseContinuousCharacterTick() const override;

private:
	void CacheCollisionComponents();
	void ApplyMeshOptimizations();
	bool IsValidMonsterDamageTarget(const AActor* OtherActor) const;
	void ApplyAttackDamageToCharacter(ACharacterBase* TargetCharacter);
	bool ApplyMonsterDamageToCharacter(ACharacterBase* TargetCharacter);
	void DeactivateDamageSphere();
	void ActivateAttackSphere();
	void ApplyMonsterHealthDefaults();
	void BeginMonsterContentPreload();
	bool TryPlayMonsterAttackMontage();
	bool FaceAttackTarget(AActor& Target);
	bool PlayMonsterAttackMontageLocal(UAnimMontage* AttackMontage, float PlayRate);
	void ShowAttackTelegraph(const UAnimMontage* AttackMontage, float PlayRate, AActor* Target);
	void HideAttackTelegraph();
	void RefreshHighlightStencil();
	void EnsureHighlightPostProcess() const;
	void EndHitFlash();
	UAnimMontage* PickAttackMontage(float TargetDistance) const;
	UAnimMontage* FindFirstLoadedAttackMontage() const;
	void BindAttackDamageWindowEvents();
	void UnbindAttackDamageWindowEvents();
	void TryPlayMonsterHitReactMontage(float DamageAmount, bool bAllowHitReact);
	void StartPhaseSummon();
	void SpawnPendingMinions();
	void SpawnMinion(int32 Index);
	APdPlayerState* ResolvePlayerStateFromActor(AActor* Actor) const;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> AttackComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> LegacyDamageComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AI|Monster|Collision")
	FName DamageComponentName = TEXT("DamageSphere");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AI|Monster|Collision")
	FName AttackComponentName = TEXT("AttackSphere");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (ClampMin = "0.0"))
	float AttackDamageMagnitude = 10.0f;

	/** 공격 몽타주에 공격 피해 구간 노티파이가 없을 때, 공격 시작부터 판정을 열어 두는 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AttackSphereActiveDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Damage", meta = (Categories = "Data"))
	FGameplayTag ContactDamageDataTag;

	/** 공격할 때마다 대상과의 거리에 맞는 공격 중 하나를 무작위로 고른다. 맞는 공격이 없으면 거리가 가장 가까운 공격을 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation")
	TArray<FMonsterAttackPattern> MonsterAttacks;

	/** 공격 예고 장판 데칼 머티리얼(M_AttackTelegraph). 비어 있으면 장판을 그리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Animation")
	TObjectPtr<UMaterialInterface> AttackTelegraphMaterial;

	/** 공격 전에 대상과의 각도 차이가 이 값 이하가 될 때까지 회전 속도대로 돈다. 0이면 공격할 때 바로 대상 쪽으로 돌려 세운다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "deg"))
	float AttackFacingTolerance = 0.0f;

	/** 피격 모션을 다시 재생하기까지의 최소 간격. 0이면 맞을 때마다 재생한다. 여럿이 계속 때리는 보스가 경직에서 못 벗어나는 일을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HitReactCooldown = 0.0f;

	/** 끄면 공격 중에 맞아도 공격을 끊지 않고 피격 모션도 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Animation")
	bool bHitReactInterruptsAttack = true;

	UPROPERTY(Transient)
	FMonsterPresentationSettings MonsterPresentationSettings;

	UPROPERTY(Transient)
	float ResolvedMonsterMaxHealth = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Reward")
	TSoftObjectPtr<URewardDefinition> MonsterRewardDefinition;

	/** 이 몬스터를 조종하는 AI가 플레이어를 알아채는 거리. 0이면 AI 컨트롤러의 기본 시야를 쓴다. 넓은 경기장의 보스처럼 멀리서 알아채야 할 때 늘린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Perception", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float SightRadiusOverride = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Death", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DeathDestroyDelay = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Summon")
	FMonsterPhaseSummon PhaseSummon;

	/**
	 * 전용 서버에서 애니메이션을 몇 프레임마다 갱신할지. 서버는 그림을 그리지 않고 피격 판정용 자세와 노티파이만 쓰므로 졸개는 늘려도 된다.
	 * 뼈에 붙인 공격 판정이 빠르게 움직이는 보스처럼 자세가 매 프레임 정확해야 하면 1로 둔다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Optimization", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ServerAnimationUpdateInterval = 1;

	/** 화면에 윤곽선을 그린다(보스). 벽 뒤에 가려져도 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Presentation")
	bool bDrawOutline = false;

	/** 맞았을 때 몸을 하얗게 번쩍이는 시간. 0이면 끈다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Presentation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HitFlashDuration = 0.0f;

	/** 윤곽선과 피격 플래시를 그리는 포스트 프로세스 머티리얼(PP_CombatHighlight). 메시의 CustomDepth 스텐실 비트를 읽는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Presentation")
	TObjectPtr<UMaterialInterface> HighlightPostProcessMaterial;

	/** 끄면 피해를 입기 전에는 체력바를 숨긴다. 수백 마리가 한꺼번에 나오는 졸개의 체력바가 화면과 UI 비용을 채우지 않게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Optimization")
	bool bShowHealthBarAtFullHealth = true;

private:
	FTimerHandle AttackSphereTimerHandle;
	FTimerHandle DeathDestroyTimerHandle;
	FTimerHandle MinionSpawnTimerHandle;
	FTimerHandle HitFlashTimerHandle;
	FDelegateHandle FrozenTagChangedHandle;
	FDelegateHandle DamageWindowOpenHandle;
	FDelegateHandle DamageWindowCloseHandle;
	FDelegateHandle HealthChangedHandle;
	TSet<TWeakObjectPtr<AActor>> AttackHitActorsThisSwing;
	TWeakObjectPtr<APdPlayerState> LastDamagingPlayerState;
	TWeakObjectPtr<UAnimInstance> AttackAnimInstance;
	TWeakObjectPtr<UAnimMontage> CurrentAttackMontage;
	TWeakObjectPtr<AActor> AttackTurnTarget;
	TWeakObjectPtr<AAttackTelegraphActor> ActiveTelegraph;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> ReplicatedAttackTarget;
	TSharedPtr<FContentLease> MonsterContentLease;
	bool bMonsterContentPreloadStarted = false;
	bool bMonsterContentReady = false;
	bool bMonsterAttackActive = false;
	bool bAttackWindowOpen = false;
	bool bDying = false;
	bool bPhaseSummonDone = false;
	bool bHitFlashActive = false;
	int32 SpawnedMinionCount = 0;
	int32 MinionCountToSpawn = 0;
	double NextHitReactTime = 0.0;
};
