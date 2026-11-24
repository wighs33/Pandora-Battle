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
class UPdAbilitySystemComponent;
class USkillDefinition;
struct FGameplayAbilitySpec;

/**
 * 대기실에서 방장이 넣은 봇(서버). 사람 참가자처럼 PlayerState를 가지고(엔진이 IsABot으로 표시한다) 플레이어 캐릭터를 조종한다.
 * 가장 가까운 적을 쫓아 무기로 치고, 지금 판도라의 스킬을 사람과 같은 입력으로 봇 공통 규칙(BotSkillRules)대로 쓴다.
 * 지금 판도라의 스킬을 모두 썼으면(재사용 대기) 다음 판도라 칸으로 바꿔 든다(무기도 칸을 따라 바뀐다).
 * 경기에서 지급이 끝나면 칸마다 그 칸 판도라에 맞는 무기를 인벤토리에서 찾아 끼운다(사람은 정보 화면에서 직접 끼운다).
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
		/** 쫓아가 무기로 친다. 쉬는 시간이 지나면 쓸 스킬을 찾는다. */
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

	AActor* FindNearestEnemy(const APdPlayer& Self) const;
	bool IsValidEnemy(const APdPlayer& Self, const AActor* Candidate) const;

	/** 칸마다 그 칸 판도라에 맞는 무기를 끼우고 첫 칸을 든다. 지급이 덜 끝났으면 false를 돌려주고 다음 틱에 다시 한다. */
	bool TryPrepareLoadout();

	void TickFight(APdPlayer& Self, AActor& Target, float DeltaSeconds, double Now, bool bSkillsAllowed);
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
	void SwitchToNextPandora(double Now);

	void Chase(APdPlayer& Self, const AActor& Target);
	void StopChasing();
	void FaceTarget(APdPlayer& Self, const AActor& Target, float DeltaSeconds);
	bool IsUsingRangedWeapon(const APdPlayer& Self) const;
	/** 총·활을 든 채 쏠 자리에 서면 조준하고, 움직이거나 스킬을 쓰면 조준을 푼다. */
	void SetAiming(APdPlayer& Self, bool bAiming);
	float GetWeaponReach(const APdPlayer& Self) const;
	void ReleaseHeldInputs();

	TWeakObjectPtr<AActor> CurrentTarget;
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
	/** 무기를 칸마다 끼운 캐릭터. 경기가 바뀌어 새 캐릭터를 조종하면 다시 끼운다(봇은 심리스 이동으로 경기를 옮겨 다닌다). */
	TWeakObjectPtr<const APawn> LoadoutPreparedPawn;
	bool bAttackHeld = false;
	bool bChasing = false;
};
