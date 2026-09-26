#include "AbilitySystem/Ability/SkillAbility.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraSkillSource.h"
#include "TimerManager.h"

USkillAbility::USkillAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 조준·몽타주·이동은 소유 클라이언트에서도 즉시 실행하고, 실제 게임 결과는 서버가 확정한다.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bReplicateInputDirectly = true;

	FGameplayTagContainer Tags;
	Tags.AddTag(LabGameplayTags::GameplayAbility);
	SetAssetTags(Tags);
}

const USkillDefinition* USkillAbility::ResolveSkill(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo)
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	UObject* SourceObject = Spec ? Spec->SourceObject.Get() : nullptr;

	return ResolveSourceSkillDataAsset(SourceObject);
}

const UPandoraSkillSource* USkillAbility::GetPandoraSkillSource() const
{
	return Cast<UPandoraSkillSource>(GetCurrentSourceObject());
}

bool USkillAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* RelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;

	// Pandora 스킬은 출처 데이터가 준비되어 있고 현재 선택된 Pandora에 속한 스킬만 실행한다.
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		const UPandoraSkillSource* PandoraSource = Cast<UPandoraSkillSource>(Spec->SourceObject.Get());
		if (!PandoraSource || !PandoraSource->IsSourceReady())
		{
			return false;
		}

		if (!Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Pandora_Selected))
		{
			return false;
		}
	}

	const USkillDefinition* Skill = ResolveSkill(Handle, ActorInfo);
	return Skill && Skill->Action && ASC
		&& !ASC->HasAnyMatchingGameplayTags(Skill->Activation.BlockedTags)
		&& ASC->HasAllMatchingGameplayTags(Skill->Activation.RequiredTags)
		&& !ASC->AreAbilityTagsBlocked(Skill->Activation.Tags)
		&& Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, RelevantTags);
}

void USkillAbility::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;

	// Pandora 스킬은 새 시전을 시작할 때 현재 슬롯 방향을 Source에 확정한다.
	// 이후 Pandora를 교체해도 이미 실행 중인 시전의 방향은 바뀌지 않는다.
	if (ActorInfo && ActorInfo->IsNetAuthority() && Spec)
	{
		UPandoraSkillSource* PandoraSource = Cast<UPandoraSkillSource>(Spec->SourceObject.Get());
		if (PandoraSource)
		{
			const APdPlayerState* PlayerState = Cast<APdPlayerState>(ActorInfo->OwnerActor.Get());
			const UPandoraComponent* Pandora = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;

			if (Pandora && PandoraSource->GetPandoraDefinition() == Pandora->GetCurrentPandoraDefinition())
			{
				PandoraSource->Initialize(
					PandoraSource->GetPandoraDefinition(),
					PandoraSource->GetSkillIndex(),
					Pandora->GetCurrentPandoraLoadoutDirection());
			}
		}
	}

	// 이번 시전에 적용할 GAS 태그를 SkillDefinition에서 가져온다.
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
	DestroyActiveSkillPresentationActor();
}

bool USkillAbility::ShouldConfirmTargetingOnInputRelease() const
{
	const USkillDefinition* Skill = ResolveSkill(CurrentSpecHandle, CurrentActorInfo);
	return Skill && Skill->SkillType == ESkillType::Press && Skill->Activation.bConfirmTargetingOnInputRelease;
}

float USkillAbility::GetRemainingDuration() const
{
	return HasDurationDeadline() && GetWorld()
		? static_cast<float>(FMath::Max(DurationEndTime - GetWorld()->GetTimeSeconds(), 0.0))
		: 0.0f;
}

bool USkillAbility::CanRunActions() const
{
	return IsActive() && CanExecuteSkillPayload()
		&& (!HasDurationDeadline() || GetRemainingDuration() > 0.0f);
}

bool USkillAbility::CommitSkill()
{
	if (bSkillCommitted)
	{
		return true;
	}

	const USkillDefinition* Skill = ResolveSkill(CurrentSpecHandle, CurrentActorInfo);
	if (!Skill || !IsActive())
	{
		return false;
	}

	const int32 Uses = FMath::Max(Skill->Activation.UsesPerCooldown, 1)
		* (Skill->Activation.bScaleUsesWithLevel ? FMath::Max(GetAbilityLevel(), 1) : 1);

	if (Uses == 1)
	{
		bSkillCommitted = CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
		if (bSkillCommitted)
		{
			UsesSinceCooldown = 0;
		}
		return bSkillCommitted;
	}

	if (!CheckCost(CurrentSpecHandle, CurrentActorInfo)
		|| !CheckCooldown(CurrentSpecHandle, CurrentActorInfo)
		|| !CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		return false;
	}

	if (++UsesSinceCooldown >= Uses)
	{
		UsesSinceCooldown = 0;
	}

	StartConfiguredSelfBuff(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);

	if (!Skill->bCancelOnHit)
	{
		SetCanBeCanceled(false);
	}

	bSkillCommitted = true;
	return true;
}

void USkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo)
	{
		return;
	}

	ActivationTime = GetWorld()->GetTimeSeconds();

	const USkillDefinition* Definition = ResolveSkill(Handle, ActorInfo);
	DurationEndTime = Definition && Definition->SkillType == ESkillType::Duration
		? ActivationTime + (FMath::IsFinite(Definition->Time.Duration)
			? FMath::Max(Definition->Time.Duration, 0.0)
			: 0.0)
		: -1.0;

	if (!Definition || !Definition->Action || !CanExecuteSkillPayload() || !CommitSkill())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FSkillActionContext Context;
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Context.Transform = Avatar->GetActorTransform();
	}
	Context.TargetActor = GetAttackTargetFromAvatar();

	if (TriggerEventData)
	{
		Context.EventData = *TriggerEventData;
		Context.TargetActor = const_cast<AActor*>(TriggerEventData->Target.Get());

		if (TriggerEventData->TargetData.Num() > 0)
		{
			const FGameplayAbilityTargetData* Data = TriggerEventData->TargetData.Get(0);
			if (Data && Data->HasEndPoint())
			{
				Context.Transform = Data->GetEndPointTransform();
			}
		}
	}

	// 준비·조준·몽타주도 전체 Duration에 포함한다.
	if (HasDurationDeadline())
	{
		const float Remaining = GetRemainingDuration();
		if (Remaining <= 0.0f)
		{
			DurationFinished();
			return;
		}

		GetWorld()->GetTimerManager().SetTimer(
			DurationTimer, this, &ThisClass::DurationFinished, Remaining, false);
	}

	ActiveAction = DuplicateObject<USkillAction>(Definition->Action, this);
	ActiveAction->OnFinished.AddUObject(this, &ThisClass::ActionFinished);
	ActiveAction->Start(this, Context);
}

void USkillAbility::ActionFinished(USkillAction* Action, const bool bSucceeded)
{
	// 같은 프레임에 Action 완료와 Duration 만료가 겹쳐도 Duration 만료를 정상 종료로 우선 처리한다.
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

	// 클라이언트 Action이 먼저 끝나더라도 서버의 실제 효과 처리가 끝날 때까지 GAS Ability는 유지한다.
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	const USkillDefinition* Definition = ResolveSkill(CurrentSpecHandle, CurrentActorInfo);
	if (HasDurationDeadline() || (Definition && Definition->SkillType == ESkillType::Press))
	{
		return;
	}

	FinishAbilityFromDuration();
}

void USkillAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	const USkillDefinition* Definition = ResolveSkill(Handle, ActorInfo);
	if (!ActorInfo || !Definition || Definition->SkillType != ESkillType::Press)
	{
		return;
	}

	const float Remaining = static_cast<float>(
		Definition->Activation.MinimumHoldSeconds - (GetWorld()->GetTimeSeconds() - ActivationTime));

	if (Remaining <= 0.0f)
	{
		FinishAbilityFromDuration();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(
			DurationTimer, this, &ThisClass::DurationFinished, Remaining, false);
	}
}

void USkillAbility::DurationFinished()
{
	FinishAbilityFromDuration();
}

void USkillAbility::OnAbilityEnding()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DurationTimer);
	}

	if (ActiveAction)
	{
		ActiveAction->OnFinished.RemoveAll(this);
		ActiveAction->Cancel();
		ActiveAction = nullptr;
	}

	Super::OnAbilityEnding();
	DestroyActiveSkillPresentationActor();
	StopConfiguredSelfBuff();
	StopMovementContactDamage();
	StopDurationMovementLock();
	RestoreAvatarMovementForAbility();
}

void USkillAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
}

void USkillAbility::ApplyCooldownOnEnd(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!CanExecuteSkillPayload() || !bSkillCommitted || UsesSinceCooldown != 0
		|| UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return;
	}

	ApplySkillCooldown(Handle, ActorInfo, ActivationInfo);
}

float USkillAbility::GetDamageBonusPercent() const
{
	float DamageBonusPercent = Super::GetDamageBonusPercent();

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* Attributes = ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;
	const UPandoraSkillSource* PandoraSource = GetPandoraSkillSource();

	if (!Attributes || !PandoraSource)
	{
		return DamageBonusPercent;
	}

	switch (PandoraSource->GetLoadoutDirection())
	{
	case EEnum_Direction::Left:
		DamageBonusPercent += FMath::Max(Attributes->GetFirstPandora(), 0.0f);
		break;

	case EEnum_Direction::Up:
		DamageBonusPercent += FMath::Max(Attributes->GetSecondPandora(), 0.0f);
		break;

	case EEnum_Direction::Right:
		DamageBonusPercent += FMath::Max(Attributes->GetThirdPandora(), 0.0f);
		break;

	default:
		break;
	}

	return DamageBonusPercent;
}

FGameplayEffectSpecHandle USkillAbility::MakeActionDamageSpec(
	const FSkillGameplayEffectConfig& Damage) const
{
	return MakeConfiguredDamageEffectSpec(Damage, CalculateDamageMagnitude(Damage));
}

FGameplayEffectSpecHandle USkillAbility::MakeActionStatusSpec() const
{
	return MakeConfiguredStatusEffectSpec(ResolveSkill(CurrentSpecHandle, CurrentActorInfo));
}

bool USkillAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ?
		ActorInfo->AbilitySystemComponent.Get() : nullptr;

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid() ?
		AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : GetCurrentAbilitySpec();

	const USkillDefinition* SkillDefinition = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (SkillDefinition)
	{
		StopAvatarMovementForSkillActivation();

		// 비용을 지불한 보호 스킬은 일반 취소로 끊지 않는다. 사망·리셋은 별도로 취소 가능 상태를 복구한다.
		if (!SkillDefinition->bCancelOnHit)
		{
			SetCanBeCanceled(false);
		}
	}

	StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool USkillAbility::UsesInputRelease(const FGameplayAbilitySpec& Spec) const
{
	const USkillDefinition* Skill = ResolveSourceSkillDataAsset(Spec.SourceObject.Get());
	return Skill && Skill->SkillType == ESkillType::Press;
}

void USkillAbility::FinishAbilityFromDuration()
{
	if (!IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		return;
	}

	const bool bReplicateEndAbility = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, false);
}

bool USkillAbility::CanExecuteSkillPayload() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return false;
	}
	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor);
		Character && (Character->IsDead() || Character->IsDeathHandled()))
	{
		return false;
	}

	const UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent ?
		AbilitySystemComponent->GetSet<UBasicAttributeSet>() : nullptr;

	if (BasicAttributeSet && BasicAttributeSet->GetHealth() <= 0.0f)
	{
		return false;
	}

	return true;
}

const USkillDefinition* USkillAbility::ResolveSourceSkillDataAsset(UObject* SourceObject)
{
	if (USkillDefinition* SkillDefinition = Cast<USkillDefinition>(SourceObject))
	{
		return SkillDefinition;
	}
	const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(SourceObject);
	return Source ? Source->GetSkillDataAsset() : nullptr;
}

USkillDefinition* USkillAbility::GetSourceSkillDataAsset() const
{
	return const_cast<USkillDefinition*>(ResolveSourceSkillDataAsset(GetCurrentSourceObject()));
}
