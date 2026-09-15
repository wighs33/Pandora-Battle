#include "AbilitySystem/Ability/SkillAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "TimerManager.h"
#include "Pandora/PandoraSkillSource.h"

USkillAbility::USkillAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 조준·몽타주·이동은 소유 클라이언트에서도 실행하고 실제 효과와 액터 생성은 서버가 처리한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;
	FGameplayTagContainer Tags;
	Tags.AddTag(LabGameplayTags::GameplayAbility);
	SetAssetTags(Tags);
}

namespace
{
	const USkillDefinition* ResolveSkill(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo)
	{
		const auto* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
		const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
		UObject* Source = Spec ? Spec->SourceObject.Get() : nullptr;
		if (const auto* Skill = Cast<USkillDefinition>(Source)) return Skill;
		const auto* PandoraSource = Cast<UPandoraSkillSource>(Source);
		return PandoraSource ? PandoraSource->GetSkillDataAsset() : nullptr;
	}
}

bool USkillAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	const USkillDefinition* Skill = ResolveSkill(Handle, ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return Skill && Skill->Action && ASC
		&& !ASC->HasAnyMatchingGameplayTags(Skill->Activation.BlockedTags)
		&& ASC->HasAllMatchingGameplayTags(Skill->Activation.RequiredTags)
		&& !ASC->AreAbilityTagsBlocked(Skill->Activation.Tags)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, RelevantTags);
}

void USkillAbility::PreActivate(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	if (const USkillDefinition* Skill = ResolveSkill(Handle, ActorInfo))
	{
		ActivationOwnedTags = Skill->Activation.OwnedTags;
		ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
		CancelAbilitiesWithTag = Skill->Activation.CancelAbilityTags;
		BlockAbilitiesWithTag = Skill->Activation.BlockAbilityTags;
		CooldownRemovalPolicyTags = Skill->Activation.CooldownRemovalTags;
	}
	bSkillCommitted = false;
	DurationEndTime = -1.0;
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, EndedDelegate, TriggerEventData);
}

bool USkillAbility::ShouldConfirmTargetingOnInputRelease() const
{
	const USkillDefinition* Skill = GetSourceSkillDataAsset();
	return Skill && Skill->SkillType == ESkillType::Press && Skill->Activation.bConfirmTargetingOnInputRelease;
}

float USkillAbility::GetRemainingDuration() const
{
	return HasDurationDeadline() && GetWorld()
		? static_cast<float>(FMath::Max(DurationEndTime - GetWorld()->GetTimeSeconds(), 0.0)) : 0.0f;
}

bool USkillAbility::CanRunActions() const
{
	return IsActive() && CanExecuteSkillPayload() && (!HasDurationDeadline() || GetRemainingDuration() > 0.0f);
}

bool USkillAbility::CommitSkill()
{
	if (bSkillCommitted) return true;
	const USkillDefinition* Skill = GetSourceSkillDataAsset();
	if (!Skill || !IsActive()) return false;
	const int32 Uses = FMath::Max(Skill->Activation.UsesPerCooldown, 1)
		* (Skill->Activation.bScaleUsesWithLevel ? FMath::Max(GetAbilityLevel(), 1) : 1);
	if (Uses == 1)
	{
		bSkillCommitted = CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
		if (bSkillCommitted) UsesSinceCooldown = 0;
		return bSkillCommitted;
	}
	if (!CheckCost(CurrentSpecHandle, CurrentActorInfo) || !CheckCooldown(CurrentSpecHandle, CurrentActorInfo)
		|| !CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)) return false;
	if (++UsesSinceCooldown >= Uses)
	{
		UsesSinceCooldown = 0;
	}
	StartConfiguredSelfBuff(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	if (!Skill->bCancelOnHit) SetCanBeCanceled(false);
	bSkillCommitted = true;
	return true;
}

void USkillAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo) return;
	ActivationTime = GetWorld()->GetTimeSeconds();
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
	DurationEndTime = Definition && Definition->SkillType == ESkillType::Duration
		? ActivationTime + (FMath::IsFinite(Definition->Time.Duration) ? FMath::Max(Definition->Time.Duration, 0.0) : 0.0)
		: -1.0;
	if (!Definition || !Definition->Action || !CanExecuteSkillPayload()
		|| !CommitSkill())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	FSkillActionContext Context;
	if (AActor* Avatar = GetAvatarActorFromActorInfo()) Context.Transform = Avatar->GetActorTransform();
	Context.TargetActor = GetAttackTargetFromAvatar();
	if (TriggerEventData)
	{
		Context.EventData = *TriggerEventData;
		Context.TargetActor = const_cast<AActor*>(TriggerEventData->Target.Get());
		if (TriggerEventData->TargetData.Num() > 0)
		{
			const FGameplayAbilityTargetData* Data = TriggerEventData->TargetData.Get(0);
			if (Data && Data->HasEndPoint()) Context.Transform = Data->GetEndPointTransform();
		}
	}
	// 액션 트리보다 먼저 타이머를 시작해 조준·준비·몽타주도 전체 지속시간에 포함한다.
	if (HasDurationDeadline())
	{
		const float Remaining = GetRemainingDuration();
		if (Remaining <= 0.0f)
		{
			DurationFinished();
			return;
		}
		GetWorld()->GetTimerManager().SetTimer(DurationTimer, this, &ThisClass::DurationFinished, Remaining, false);
	}
	ActiveAction = DuplicateObject<USkillAction>(Definition->Action, this);
	ActiveAction->OnFinished.AddUObject(this, &ThisClass::ActionFinished);
	ActiveAction->Start(this, Context);
}

void USkillAbility::ActionFinished(USkillAction* Action, bool bSucceeded)
{
	// 같은 프레임에 액션 콜백이 타이머보다 먼저 실행되어도 만료는 정상 종료로 처리한다.
	if (HasDurationDeadline() && GetRemainingDuration() <= 0.0f)
	{
		DurationFinished();
		return;
	}
	if (!bSucceeded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// 클라이언트에서 동작이 먼저 끝나도 서버의 투사체·피해 처리가 완료될 때까지 GAS 종료를 기다린다.
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
	if (HasDurationDeadline() || (Definition && Definition->SkillType == ESkillType::Press)) return;
	FinishAbilityFromDuration();
}

void USkillAbility::InputReleased(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo)
{
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
	if (ActorInfo && Definition && Definition->SkillType == ESkillType::Press)
	{
		const float Remaining = static_cast<float>(Definition->Activation.MinimumHoldSeconds - (GetWorld()->GetTimeSeconds() - ActivationTime));
		if (Remaining <= 0.0f) FinishAbilityFromDuration();
		else GetWorld()->GetTimerManager().SetTimer(DurationTimer, this, &ThisClass::DurationFinished, Remaining, false);
	}
}

void USkillAbility::DurationFinished()
{
	FinishAbilityFromDuration();
}

void USkillAbility::OnAbilityEnding()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DurationTimer);
	if (ActiveAction)
	{
		ActiveAction->OnFinished.RemoveAll(this);
		ActiveAction->Cancel();
		ActiveAction = nullptr;
	}
	Super::OnAbilityEnding();
}

// GAS의 CommitAbility는 쿨다운 적용도 요청한다. 스킬은 정상 종료 때 직접 적용한다.
void USkillAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
}

void USkillAbility::ApplyCooldownOnEnd(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	// 확정된 시전의 사용 횟수를 모두 소모했을 때만 시작한다. 별도의 적용 예약은 저장하지 않는다.
	if (!bSkillCommitted || UsesSinceCooldown != 0 || !Spec || Spec->PendingRemove
		|| UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return;
	}
	Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
}

FGameplayEffectSpecHandle USkillAbility::MakeActionDamageSpec(const FSkillGameplayEffectConfig& Damage) const
{
	return MakeConfiguredDamageEffectSpec(Damage, CalculateSkillDamageMagnitude(Damage));
}

FGameplayEffectSpecHandle USkillAbility::MakeActionStatusSpec() const
{
	return MakeConfiguredStatusEffectSpec(GetSourceSkillDataAsset());
}
