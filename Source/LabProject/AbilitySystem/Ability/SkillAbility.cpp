#include "AbilitySystem/Ability/SkillAbility.h"
#include "AbilitySystemComponent.h"
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
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, EndedDelegate, TriggerEventData);
}

bool USkillAbility::ShouldConfirmTargetingOnInputRelease() const
{
	const USkillDefinition* Skill = GetSourceSkillDataAsset();
	return Skill && Skill->SkillType == ESkillType::Press && Skill->Activation.bConfirmTargetingOnInputRelease;
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
		return bSkillCommitted;
	}
	if (!CheckCost(CurrentSpecHandle, CurrentActorInfo) || !CheckCooldown(CurrentSpecHandle, CurrentActorInfo)
		|| !CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)) return false;
	if (++UsesSinceCooldown >= Uses)
	{
		if (!CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false)) return false;
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
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
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
	ActivationTime = GetWorld()->GetTimeSeconds();
	ActiveAction = DuplicateObject<USkillAction>(Definition->Action, this);
	ActiveAction->OnFinished.AddUObject(this, &ThisClass::ActionFinished);
	ActiveAction->Start(this, Context);
}

void USkillAbility::ActionFinished(USkillAction* Action, bool bSucceeded)
{
	if (!bSucceeded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// 클라이언트에서 동작이 먼저 끝나도 서버의 투사체·피해 처리가 완료될 때까지 GAS 종료를 기다린다.
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
	if (Definition && Definition->SkillType == ESkillType::Press) return;
	FinishAbilityFromDuration();
}

void USkillAbility::InputReleased(FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo)
{
	const USkillDefinition* Definition = GetSourceSkillDataAsset();
	if (ActorInfo && Definition && Definition->SkillType == ESkillType::Press)
	{
		const float Remaining = Definition->Activation.MinimumHoldSeconds - (GetWorld()->GetTimeSeconds() - ActivationTime);
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

FGameplayEffectSpecHandle USkillAbility::MakeActionDamageSpec(const FSkillGameplayEffectConfig& Damage) const
{
	return MakeConfiguredDamageEffectSpec(Damage, CalculateSkillDamageMagnitude(Damage));
}

FGameplayEffectSpecHandle USkillAbility::MakeActionStatusSpec() const
{
	return MakeConfiguredStatusEffectSpec(GetSourceSkillDataAsset());
}
