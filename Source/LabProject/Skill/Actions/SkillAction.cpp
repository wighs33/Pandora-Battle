#include "Skill/Actions/SkillAction.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/PdGameplayTags.h"

void USkillAction::Start(USkillAbility* InAbility, const FSkillActionContext& InContext)
{
	check(!bRunning);
	OwningAbility = InAbility;
	ExecutionContext = InContext;
	bRunning = true;
	// 타이머가 처리되기 전이라도 공통 종료 시점 이후에는 다음 액션을 실행하지 않는다.
	if (InAbility && InAbility->HasDurationDeadline() && InAbility->GetRemainingDuration() <= 0.0f)
	{
		Finish();
		return;
	}
	OnStart();
}

UWorld* USkillAction::GetWorld() const
{
	return OwningAbility.IsValid() ? OwningAbility->GetWorld() : nullptr;
}

void USkillAction::Cancel()
{
	Finish(false);
}

void USkillAction::Finish(bool bSucceeded)
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	OnStop();
	OnFinished.Broadcast(this, bSucceeded);
	OnFinished.Clear();
}

bool USkillAction::ResolveDamageableCharacterTarget(AActor* SourceActor, AActor* HitActor,
	UAbilitySystemComponent*& OutSourceASC, UAbilitySystemComponent*& OutTargetASC)
{
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!TargetCharacter)
	{
		return false;
	}

	OutSourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	OutTargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!OutSourceASC || !OutTargetASC || OutTargetASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead))
	{
		return false;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(SourceActor);
	return !SourceCharacter || SourceCharacter->CanDamageCharacterByTeam(TargetCharacter);
}

bool USkillAction::ApplyDamageWithConfiguredStatus(UAbilitySystemComponent& SourceASC,
	UAbilitySystemComponent& TargetASC, const FGameplayEffectSpec& DamageSpec) const
{
	if (!SourceASC.ApplyGameplayEffectSpecToTarget(DamageSpec, &TargetASC).WasSuccessfullyApplied())
	{
		return false;
	}

	GetAbility()->ApplyConfiguredStatusEffectToTarget(GetAbility()->GetSourceSkillDataAsset(), &TargetASC);
	return true;
}
