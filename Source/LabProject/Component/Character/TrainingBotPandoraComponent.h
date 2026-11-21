#pragma once

#include "CoreMinimal.h"
#include "AI/Bot/BotSkillRules.h"
#include "Components/ActorComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "TrainingBotPandoraComponent.generated.h"

class AActor;
class AEnemyBase;
class FContentLease;
class UBrainComponent;
class UItemDefinition;
class UPandoraDefinition;
class UPandoraSkillSource;
class UPdAbilitySystemComponent;
class USkillDefinition;
struct FStreamableHandle;

/** 훈련 봇이 받은 판도라 하나: 판도라, 짝 무기, 스킬 칸 순서대로 부여한 능력. */
USTRUCT()
struct FTrainingBotPandora
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UPandoraDefinition> Pandora;

	UPROPERTY()
	TObjectPtr<const UItemDefinition> Weapon;

	TArray<FGameplayAbilitySpecHandle> SkillHandles;
};

/**
 * 훈련 봇이 무작위 판도라 몇 개(기본 3개)의 스킬을 쓴다(서버). 사람처럼 한 번에 한 판도라만 들고, 그 판도라의 짝 무기로 바꿔 든다.
 * 지금 판도라의 스킬을 모두 썼으면(재사용 대기) 쓸 스킬이 남은 다음 판도라로 바꾼다.
 *
 * 스킬을 쓰는 동안과 스킬마다 정한 뒤처리(ESkillBotFollowUp) 동안은 봇의 행동 트리를 멈추고 이 컴포넌트가 몸을 맡는다.
 * 행동 트리가 꺼져 있거나(테스트·연출), 공격이 꺼져 있거나, 죽었거나, 경직·무기 교체 중이면 스킬을 쓰지 않는다.
 * 훈련 봇은 체력까지 차는 회복을 받지 않으므로, 다음 스킬에 마나가 모자라면 마나만 채운다. 스킬 횟수는 재사용 대기가 정한다.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UTrainingBotPandoraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	UTrainingBotPandoraComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 서버: 판도라 사용을 켜고 끈다. 켤 때마다 판도라를 새로 고르고, 끄면 받은 스킬을 거두고 켜기 전에 들던 무기로 돌아간다. */
	void SetPandorasEnabled(bool bEnabled);
	bool ArePandorasEnabled() const { return bPandorasEnabled; }

	/** 판도라를 고르고 스킬을 받아 쓸 준비가 끝났는지. */
	bool HasPandoraSkills() const { return !Pandoras.IsEmpty(); }

	/** 지금 들고 있는 판도라. 고르는 중이거나 꺼져 있으면 없다. */
	const UPandoraDefinition* GetCurrentPandora() const;

	/** 받은 판도라들(고른 순서). */
	TArray<const UPandoraDefinition*> GetPandoras() const;

	/** 판도라 사용을 끄면 돌아갈 무기(판도라 짝 무기로 바꾸기 전 무기). 아직 바꾸지 않았으면 지금 무기다. 없으면 맨손이다. */
	const UItemDefinition* GetWeaponBeforePandoras() const;

protected:
	/**
	 * 훈련장에 들어왔을 때 판도라 사용을 켜 둘지. 설정 화면에서 바꿀 수 있다.
	 * 훈련장은 무기 연습용 기본 봇으로 시작하고, 판도라 스킬 상대 연습은 설정에서 켠다(켜 둔 봇이 건 보호막·회복은 꺼도 지속 시간 동안 남는다).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "!Training|Bot|Pandora")
	bool bUsePandorasByDefault = false;

	/** 받을 판도라 수. 사람의 판도라 칸 수와 같다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Training|Bot|Pandora", meta = (ClampMin = "1", ClampMax = "3"))
	int32 PandoraCount = 3;

	/** 판도라마다 바꿔 들 무기 후보. 판도라가 쓸 수 있는 무기 가운데 이 목록에서 먼저 나오는 것을 든다. 맞는 무기가 없는 판도라는 고르지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Training|Bot|Pandora")
	TArray<TSoftObjectPtr<UItemDefinition>> PandoraWeapons;

private:
	enum class EPhase : uint8
	{
		/** 행동 트리가 싸운다. 쉬는 시간이 지나면 쓸 스킬을 찾는다. */
		Idle,
		/** 멈춰 서서 대상을 바라본다. */
		Preparing,
		/** 스킬 시전 동작 중이다. */
		Casting,
		/** 스킬마다 정한 뒤처리 중이다. */
		FollowUp,
	};

	// Internal Helpers ------------------------------------------------------------------------------------------------
	AEnemyBase* GetEnemy() const;
	UPdAbilitySystemComponent* GetAbilitySystem() const;
	UBrainComponent* GetBrain() const;

	void BeginPandoraSelection();
	void HandlePandoraContentLoaded(int32 RequestId);
	void PickAndGrantPandoras();
	void ClearPandoraSkills();
	void SelectPandora(int32 PandoraIndex);
	int32 FindNextPandoraWithReadySkill() const;

	void TickSkills(AEnemyBase& Enemy, float DeltaTime);
	bool CanUseSkillsNow(const AEnemyBase& Enemy) const;
	int32 FindCastableSkill(const AEnemyBase& Enemy, const AActor& Target) const;
	bool CanActivateSkill(FGameplayAbilitySpecHandle Handle) const;
	bool IsCastAnimating() const;
	void RefillManaForCurrentPandora() const;

	void BeginPreparing(int32 SkillSlot, double Now);
	void TickPreparing(AEnemyBase& Enemy, const AActor& Target, float DeltaTime, double Now);
	void TickCasting(AEnemyBase& Enemy, const AActor* Target, float DeltaTime, double Now);
	void TickFollowUp(AEnemyBase& Enemy, const AActor* Target, float DeltaTime, double Now);
	void TickWeaponAttacksOnly(AEnemyBase& Enemy, const AActor& Target, float DeltaTime, double Now);
	void EnterFollowUp(double Now);
	/** 행동 트리에 몸을 돌려주고 다음 스킬까지 쉰다. */
	void ReturnToBehaviorTree(double Now);
	void CancelCastingSkill();
	void PauseBrain(AEnemyBase& Enemy);
	void ResumeBrain();
	void FaceTarget(AEnemyBase& Enemy, const AActor& Target, float DeltaTime) const;

	UPROPERTY(Transient)
	TArray<FTrainingBotPandora> Pandoras;

	/** 부여한 능력의 출처. 능력 Spec은 출처를 약하게 잡으므로 여기서 붙잡아 둔다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPandoraSkillSource>> SkillSources;

	/** 판도라 짝 무기로 처음 바꾸기 직전에 들던(또는 바꾸던) 무기. 끄면 이 무기로 돌아간다(없으면 맨손). */
	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> WeaponBeforePandoras;

	TSharedPtr<FStreamableHandle> PandoraPreloadHandle;
	TSharedPtr<FContentLease> WeaponLease;
	BotSkillRules::FRoamAroundTarget Roam;

	EPhase Phase = EPhase::Idle;
	ESkillBotFollowUp ActiveFollowUp = ESkillBotFollowUp::None;
	int32 CurrentPandoraIndex = INDEX_NONE;
	int32 PendingSkillSlot = INDEX_NONE;
	FGameplayAbilitySpecHandle CastingHandle;
	double PhaseStartTime = 0.0;
	double FollowUpEndTime = 0.0;
	double NextSkillTime = 0.0;
	double NextWeaponAttackTime = 0.0;
	/** 비동기 로드가 끝났을 때 그사이 껐다 켠 요청인지 가린다. */
	int32 SelectionRequestId = 0;
	int32 PendingContentLoads = 0;
	bool bPandorasEnabled = false;
	/** 켰지만 봇 정의가 아직 적용되지 않아(시작 무기를 모름) 판도라 고르기를 미뤄 둔 상태. */
	bool bSelectionPending = false;
	/** 판도라 짝 무기로 바꿔 든 적이 있는지. 끌 때 바꾼 적이 있을 때만 켜기 전 무기로 돌아간다. */
	bool bSwitchedWeapon = false;
	bool bBrainPausedBySkill = false;
};
