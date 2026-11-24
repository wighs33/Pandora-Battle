#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/Bot/BotSkillRules.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Interface/TargetingInterface.h"
#include "PdBotController.generated.h"

class APdPlayer;
class APdPlayerState;
class ARewardChest;
class UBotGrowthComponent;
class UPdAbilitySystemComponent;
class USkillDefinition;
struct FGameplayAbilitySpec;

/**
 * 대기실에서 방장이 넣은 봇(서버). 사람 참가자처럼 PlayerState를 가지고(엔진이 IsABot으로 표시한다) 플레이어 캐릭터를 조종한다.
 * 경기에서는 사람처럼 상자와 몬스터를 찾아다니며 크고(UBotGrowthComponent), 가까이 보이는 적이나 다 큰 뒤의 가장 가까운 적을 쫓아 싸운다.
 * 무기로 치고, 지금 판도라의 스킬을 사람과 같은 입력으로 봇 공통 규칙(BotSkillRules)대로 쓴다.
 * 지금 판도라의 스킬을 모두 썼으면(재사용 대기) 스킬을 쓸 수 있는 다음 칸으로 바꿔 든다(무기도 칸을 따라 바뀐다).
 */
UCLASS()
class LABPROJECT_API APdBotController : public AAIController, public ITargetingInterface
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	APdBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	/** 스킬·무기가 겨눌 대상. 사람은 조준으로 정하고, 봇은 쫓는 적을 겨눈다. */
	virtual AActor* GetAttackTarget_Implementation() const override;

private:
	enum class EPhase : uint8
	{
		/** 대상에게 다가가 무기로 치거나 상자를 연다. 쉬는 시간이 지나면 쓸 스킬을 찾는다. */
		Fight,
		/** 멈춰 서서 대상을 바라본다. */
		Preparing,
		/** 스킬 시전 동작 중이다. */
		Casting,
		/** 스킬마다 정한 뒤처리 중이다. */
		FollowUp,
	};

	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayer* GetBotPawn() const;
	APdPlayerState* GetBotPlayerState() const;
	UPdAbilitySystemComponent* GetAbilitySystem() const;
	bool IsBotAlive() const;

	/** bSkipPostponed면 다가가지 못해 미뤄 둔 적은 고르지 않는다. */
	APdPlayer* FindNearestEnemy(const APdPlayer& Self, bool bSkipPostponed) const;
	bool IsValidEnemy(const APdPlayer& Self, const AActor* Candidate) const;
	/** 지금 싸울 적이나 크려고 찾아갈 상자·몬스터. */
	AActor* ChooseTarget(const APdPlayer& Self) const;
	bool IsValidTarget(const APdPlayer& Self, AActor* Candidate) const;

	/** 첫 칸을 들고 성장을 시작한다. 지급이 덜 끝났으면 false를 돌려주고 다음 틱에 다시 한다. */
	bool TryPrepareLoadout();

	void TickFight(APdPlayer& Self, AActor& Target, float DeltaSeconds, double Now, bool bSkillsAllowed);
	void TickOpenChest(APdPlayer& Self, ARewardChest& Chest, float DeltaSeconds, double Now);
	/** 대상에게 한동안 가까워지지도, 피해를 주지도 못하면 미뤄 두고 다른 곳으로 간다. */
	void PostponeStalledTarget(const APdPlayer& Self, const AActor& Target, double Now);
	void TickPreparing(APdPlayer& Self, AActor& Target, float DeltaSeconds, double Now);
	void TickCasting(APdPlayer& Self, const AActor* Target, float DeltaSeconds, double Now);
	void TickFollowUp(APdPlayer& Self, AActor* Target, float DeltaSeconds, double Now);
	void EnterFollowUp(double Now);
	void ReturnToFight(double Now);

	/** 지금 판도라의 스킬 칸 SkillSlot(0~2)에 묶인 능력. 칸에 스킬이 없으면 없다. */
	const FGameplayAbilitySpec* FindSkillSpec(int32 SkillSlot) const;
	const USkillDefinition* GetSkillDefinition(const FGameplayAbilitySpec& Spec) const;
	bool IsSkillReady(const FGameplayAbilitySpec& Spec) const;
	int32 FindCastableSkillSlot(const APdPlayer& Self, const AActor& Target) const;
	bool HasAnyReadySkill() const;
	/** 지금 쓸 수 있는 스킬 가운데 가장 먼 거리. 쓸 수 있는 스킬이 없으면 0. */
	float GetReadySkillRange() const;
	void SwitchToNextPandora(double Now);

	void Chase(APdPlayer& Self, const AActor& Target, float AcceptanceRadius, double Now);
	void StopChasing();
	void StopAttacking(APdPlayer& Self);
	void FaceTarget(APdPlayer& Self, const AActor& Target, float DeltaSeconds);
	bool IsUsingRangedWeapon(const APdPlayer& Self) const;
	/** 총·활을 든 채 쏠 자리에 서면 조준하고, 움직이거나 스킬을 쓰면 조준을 푼다. */
	void SetAiming(APdPlayer& Self, bool bAiming);
	float GetWeaponReach(const APdPlayer& Self) const;
	void ReleaseHeldInputs();

	UPROPERTY(Transient)
	TObjectPtr<UBotGrowthComponent> Growth;

	/** 싸울 적이나 찾아갈 상자·몬스터. */
	TWeakObjectPtr<AActor> CurrentTarget;
	/** 쫓거나 찾아가는 대상과, 마지막으로 가까워지거나 피해를 준 때의 거리·체력·시각. */
	TWeakObjectPtr<const AActor> ProgressTarget;
	float ProgressDistance = 0.0f;
	float ProgressHealth = 0.0f;
	double ProgressTime = 0.0;
	BotSkillRules::FRoamAroundTarget Roam;
	EPhase Phase = EPhase::Fight;
	ESkillBotFollowUp ActiveFollowUp = ESkillBotFollowUp::None;
	int32 PendingSkillSlot = INDEX_NONE;
	FGameplayAbilitySpecHandle CastingHandle;
	TWeakObjectPtr<const USkillDefinition> CastingSkill;
	FGameplayTag HeldSkillInput;
	double PhaseStartTime = 0.0;
	double FollowUpEndTime = 0.0;
	double NextSkillTime = 0.0;
	double NextPandoraSwitchTime = 0.0;
	double NextAttackTime = 0.0;
	double AttackReleaseTime = 0.0;
	double NextTargetSearchTime = 0.0;
	/** 쫓는 동안 마지막으로 움직인 시각과, 막혀서 대상 쪽으로 곧장 뛰는 것을 끝낼 시각. */
	double LastChaseMoveTime = 0.0;
	double DirectRunEndTime = 0.0;
	/** 성장을 시작한 캐릭터. 경기가 바뀌어 새 캐릭터를 조종하면 다시 시작한다(봇은 심리스 이동으로 경기를 옮겨 다닌다). */
	TWeakObjectPtr<const APawn> LoadoutPreparedPawn;
	bool bAttackHeld = false;
	bool bChasing = false;
};
