#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "UObject/Object.h"
#include "SkillAction.generated.h"

class UAbilitySystemComponent;
class USkillAbility;
class USkillAction;
struct FGameplayEffectSpec;

/** 같은 실행 단계에 참여하는 기능들이 공유하는 대상과 위치. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillActionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "!Skill")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "!Skill")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "!Skill")
	FGameplayEventData EventData;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSkillActionFinished, USkillAction*, bool);

/** 에셋에는 설정만 저장한다. 실행할 때 액션을 복제해 시전자별 상태를 분리한다. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class LABPROJECT_API USkillAction : public UObject
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual UWorld* GetWorld() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	void Start(USkillAbility* InAbility, const FSkillActionContext& InContext);
	void Cancel();
	bool IsRunning() const { return bRunning; }

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnStart() PURE_VIRTUAL(USkillAction::OnStart, );
	virtual void OnStop() {}

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void Finish(bool bSucceeded = true);
	USkillAbility* GetAbility() const { return OwningAbility.Get(); }
	const FSkillActionContext& GetContext() const { return ExecutionContext; }

	/** HitActor가 시전자가 피해를 줄 수 있는 살아 있는 다른 팀 캐릭터면 양쪽 ASC를 채운다. */
	static bool ResolveDamageableCharacterTarget(
		AActor* SourceActor,
		AActor* HitActor,
		UAbilitySystemComponent*& OutSourceASC,
		UAbilitySystemComponent*& OutTargetASC);
	/** 피해를 적용하고, 들어갔으면 스킬에 설정된 상태 이상도 건다. */
	bool ApplyDamageWithConfiguredStatus(
		UAbilitySystemComponent& SourceASC,
		UAbilitySystemComponent& TargetASC,
		const FGameplayEffectSpec& DamageSpec) const;

public:
	FOnSkillActionFinished OnFinished;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USkillAbility> OwningAbility;

	UPROPERTY(Transient)
	FSkillActionContext ExecutionContext;

	bool bRunning = false;
};
