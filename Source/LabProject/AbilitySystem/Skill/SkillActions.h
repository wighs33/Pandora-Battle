#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skill/SkillAction.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "SkillActions.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class AProjectileBase;

UCLASS(meta = (DisplayName = "Wait"))
class LABPROJECT_API USkillWaitAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0", Units = "s"))
	float Seconds = 0.0f;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	void Elapsed();
	FTimerHandle Timer;
};

/** 애니메이션 알림 등의 GAS 이벤트를 기다리고 다음 단계에 대상 정보를 전달한다. */
UCLASS(meta = (DisplayName = "Wait Gameplay Event"))
class LABPROJECT_API USkillWaitEventAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (Categories = "Event"))
	FGameplayTag EventTag;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0.01", Units = "s"))
	float Timeout = 10.0f;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	UFUNCTION()
	void Received(FGameplayEventData Payload);
	void TimedOut();
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> Task;
	FTimerHandle Timer;
};

UCLASS(meta = (DisplayName = "Play Montage"))
class LABPROJECT_API USkillMontageAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill")
	TObjectPtr<UAnimMontage> Montage;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	UFUNCTION()
	void Completed();
	UFUNCTION()
	void Interrupted();
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> Task;
};

/** 기존 돌진과 조합형 스킬이 같은 루트 모션 생성 경로를 사용한다. */
UCLASS(meta = (DisplayName = "Dash"))
class LABPROJECT_API USkillDashAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0", Units = "cm/s"))
	float Speed = 1000.0f;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Skill")
	bool bEnableGravity = false;
	/** 대시 GameplayCue가 실행되는 동안 캐릭터를 숨긴다. */
	UPROPERTY(EditAnywhere, Category = "Skill")
	bool bHideCharacter = true;
	static UAbilityTask_ApplyRootMotionConstantForce* CreateTask(UGameplayAbility* Ability,
		const FVector& Direction, float Speed, float Duration, float FinishSpeed, bool bEnableGravity);
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	UFUNCTION()
	void Completed();
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> Task;
};

/** 명시한 효과를 현재 대상 또는 시전자에게 적용한다. 회복과 버프에도 사용한다. */
UCLASS(meta = (DisplayName = "Apply Gameplay Effect"))
class LABPROJECT_API USkillGameplayEffectAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill")
	FSkillGameplayEffectConfig Effect;
	UPROPERTY(EditAnywhere, Category = "Skill")
	bool bApplyToSelf = false;
protected:
	virtual void OnStart() override;
};

UCLASS(meta = (DisplayName = "Area Damage"))
class LABPROJECT_API USkillAreaDamageAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill")
	FSkillGameplayEffectConfig Damage;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 300.0f;
protected:
	virtual void OnStart() override;
};

/** 현재 위치에 액터를 만든다. 장판과 소환물을 같은 생성 기능으로 표현한다. */
UCLASS(meta = (DisplayName = "Spawn Actor"))
class LABPROJECT_API USkillSpawnActorAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill")
	TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, Category = "Skill")
	FTransform Offset = FTransform::Identity;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0.01", Units = "s"))
	float LifeSpan = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Skill")
	bool bProjectToGround = true;
protected:
	virtual void OnStart() override;
};

/** 투사체 충돌을 기다린 뒤 충돌 대상/위치에서 자식 기능을 실행한다. */
UCLASS(meta = (DisplayName = "Fire Projectile"))
class LABPROJECT_API USkillProjectileAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Skill")
	TSubclassOf<AProjectileBase> ProjectileClass;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "1", Units = "cm/s"))
	float Speed = 1500.0f;
	UPROPERTY(EditAnywhere, Category = "Skill")
	FVector SpawnOffset = FVector(100.0, 0.0, 0.0);
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0.01", Units = "s"))
	float MaxFlightSeconds = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Skill")
	FSkillGameplayEffectConfig Damage;
	UPROPERTY(EditAnywhere, Instanced, Category = "Skill")
	TObjectPtr<USkillAction> OnImpact;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	void Impacted(AActor* Target, const FHitResult& Hit);
	void ImpactActionFinished(USkillAction* Child, bool bSucceeded);
	UFUNCTION()
	void ProjectileDestroyed(AActor* Actor);
	UPROPERTY(Transient)
	TObjectPtr<AProjectileBase> Projectile;
};

/** 기능을 간격을 두고 반복한다. Count=0이면 Ability 종료까지 반복한다. */
UCLASS(meta = (DisplayName = "Repeat"))
class LABPROJECT_API USkillRepeatAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Skill")
	TObjectPtr<USkillAction> Action;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0"))
	int32 Count = 1;
	UPROPERTY(EditAnywhere, Category = "Skill", meta = (ClampMin = "0.01", Units = "s"))
	float Interval = 0.5f;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	void RunNext();
	void ChildFinished(USkillAction* Child, bool bSucceeded);
	UPROPERTY(Transient)
	TObjectPtr<USkillAction> Current;
	int32 CompletedCount = 0;
	FTimerHandle Timer;
};
