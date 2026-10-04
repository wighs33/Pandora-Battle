#include "AbilitySystem/Ability/HitReactAbility.h"
#include "AbilitySystem/Ability/SkillAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Pandora/PandoraSkillSource.h"
#include "Component/Player/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HitReactAbility)

namespace
{
const USkillDefinition* ResolveSkillDataAssetFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	const UObject* SourceObject = AbilitySpec.SourceObject.Get();
	if (const UPandoraSkillSource* SkillSource = Cast<UPandoraSkillSource>(SourceObject))
	{
		return SkillSource->GetSkillDataAsset();
	}

	if (const USkillDefinition* SkillDataAsset = Cast<USkillDefinition>(SourceObject))
	{
		return SkillDataAsset;
	}

	if (const USkillAbility* AbilityInstance = Cast<USkillAbility>(AbilitySpec.GetPrimaryInstance()))
	{
		return AbilityInstance->GetSourceSkillDataAsset();
	}

	return nullptr;
}

bool HasActiveSkillProtectedFromHitReact(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle HitReactHandle)
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability || AbilitySpec.Handle == HitReactHandle)
		{
			continue;
		}

		const USkillDefinition* SkillDataAsset = ResolveSkillDataAssetFromSpec(AbilitySpec);
		if (!SkillDataAsset || SkillDataAsset->bCancelOnHit)
		{
			continue;
		}

		return true;
	}

	return false;
}

void CancelDefaultActionsForHitReact(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayTagContainer TagsToCancel;
	TagsToCancel.AddTag(LabGameplayTags::Action_Attack);
	TagsToCancel.AddTag(LabGameplayTags::Action_Punch);
	TagsToCancel.AddTag(LabGameplayTags::Action_RangedAttack);
	TagsToCancel.AddTag(LabGameplayTags::Action_Equip);
	TagsToCancel.AddTag(LabGameplayTags::Action_Unequip);
	TagsToCancel.AddTag(LabGameplayTags::GameplayAbility_Movement_Grapple);
	AbilitySystemComponent->CancelAbilities(&TagsToCancel);
}

void CancelSkillsConfiguredToCancelOnHit(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle HitReactHandle)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> HandlesToCancel;
	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability || AbilitySpec.Handle == HitReactHandle)
		{
			continue;
		}

		const USkillDefinition* SkillDataAsset = ResolveSkillDataAssetFromSpec(AbilitySpec);
		if (!SkillDataAsset || !SkillDataAsset->bCancelOnHit)
		{
			continue;
		}

		HandlesToCancel.AddUnique(AbilitySpec.Handle);
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : HandlesToCancel)
	{
		AbilitySystemComponent->CancelAbilityHandle(AbilityHandle);
	}
}
} // namespace

UHitReactAbility::UHitReactAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 피격 반응은 서로 독립된 실행으로 겹칠 수 있지만, 능력 인스턴스 자체에는 클라이언트에
	// 복제할 상태가 없다. 예측, 몽타주 복제, 게임플레이 큐는 각자의 GAS 경로를 쓴다.
	// InstancedPerExecution 능력의 복제는 GAS가 지원하지 않는다.
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bRetriggerInstancedAbility = true;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
	AbilityAssetTags.AddTag(LabGameplayTags::Action_HitReact);
	SetAssetTags(AbilityAssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
	ActivationOwnedTags.AddTag(LabGameplayTags::Action_HitReact);
}

void UHitReactAbility::PostLoad()
{
	Super::PostLoad();

	// 예전 GA_HitReact 애셋은 ReplicateYes로 저장돼 있다. 블루프린트 기본값을 역직렬화한 뒤
	// 지원되는 정책으로 되돌려, 패키지 빌드와 데이터 검증에서 실행별 복제 충돌이 다시 생기지 않게 한다.
	if (InstancingPolicy == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	}
}

// State helpers
void UHitReactAbility::ClearActiveHitReactEffect()
{
	if (bApplyHitReactEffect && HitReactEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(HitReactEffectClass);
	}
}

// Timing callbacks
void UHitReactAbility::OnHitReactMontageCompleted()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UHitReactAbility::OnHitReactMontageInterrupted()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UHitReactAbility::OnHitReactMontageCancelled()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// Ability flow
void UHitReactAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!ensure(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (HasActiveSkillProtectedFromHitReact(AbilitySystemComponent, Handle))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	CancelDefaultActionsForHitReact(AbilitySystemComponent);
	CancelSkillsConfiguredToCancelOnHit(AbilitySystemComponent, Handle);

	UAnimMontage* Montage = nullptr;
	if (const UEquipmentComponent* EquipmentComponent = Character->FindComponentByClass<UEquipmentComponent>())
	{
		FHitReactData HitReactData;
		if (EquipmentComponent->GetHitReactData(HitReactData))
		{
			Montage = HitReactData.HitReactMontage;
		}
	}

	if (!Montage)
	{
		Montage = HitReactMontage.Get();
	}

	if (!Montage && !HitReactMontage.IsNull())
	{
		BeginHitReactMontagePreload();
		return;
	}

	StartHitReactMontage(Montage, Handle, ActorInfo, ActivationInfo);
	static_cast<void>(TriggerEventData);
}

void UHitReactAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	HitReactMontageLease.Reset();
}

void UHitReactAbility::BeginHitReactMontagePreload()
{
	HitReactMontageLease.Reset();
	if (HitReactMontage.IsNull())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const UGameInstance* GameInstance = AvatarActor ? AvatarActor->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	HitReactMontageLease = ContentSubsystem->AcquireContent({ HitReactMontage.ToSoftObjectPath() },
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleHitReactMontagePreloadComplete));
}

void UHitReactAbility::HandleHitReactMontagePreloadComplete()
{
	if (!IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		return;
	}

	UAnimMontage* Montage = HitReactMontage.Get();
	if (!Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	StartHitReactMontage(Montage, CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

void UHitReactAbility::StartHitReactMontage(UAnimMontage* Montage, const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!ensure(Character) || !ensure(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FName StartSectionName = HitReactStartSectionName;

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartSectionName.IsNone() && Montage->GetSectionIndex(StartSectionName) == INDEX_NONE)
	{
		StartSectionName = NAME_None;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this,
		NAME_None, Montage, 1.f, StartSectionName, false, 1.0f, 0.f, true);
	if (!ensure(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCancelled);
	MontageTask->ReadyForActivation();

	if (bApplyHitReactEffect && HitReactEffectClass)
	{
		ApplyGameplayEffect(HitReactEffectClass, 1.f, 1);
	}

	if (HitReactCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		K2_ExecuteGameplayCueWithParams(HitReactCueTag, CueParameters);
	}
}
