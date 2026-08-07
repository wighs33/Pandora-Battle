#include "AbilitySystem/Ability/HitReactAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Component/Player/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HitReactAbility)

namespace
{
const USkillDefinition* ResolveSkillDataAssetFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	const UObject* SourceObject = AbilitySpec.SourceObject.Get();
	if (const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(SourceObject))
	{
		return RuntimeContext->GetSkillDataAsset();
	}

	if (const USkillDefinition* SkillDataAsset = Cast<USkillDefinition>(SourceObject))
	{
		return SkillDataAsset;
	}

	if (const UPdGameplayAbility* AbilityInstance = Cast<UPdGameplayAbility>(AbilitySpec.GetPrimaryInstance()))
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
	// Hit reactions may overlap as independent executions, but the ability
	// instance itself has no state that clients need to replicate. GAS still
	// propagates the ServerInitiated activation, montage and gameplay cues.
	// Replicating an InstancedPerExecution ability is unsupported by GAS.
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
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

	// Older GA_HitReact assets serialized ReplicateYes. Restore the supported
	// policy after Blueprint defaults are deserialized so packaged builds and
	// data validation cannot recreate the per-execution replication conflict.
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

void UHitReactAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	ReleaseHitReactMontagePreload();
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void UHitReactAbility::BeginHitReactMontagePreload()
{
	ReleaseHitReactMontagePreload();
	if (HitReactMontage.IsNull())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const uint32 RequestGeneration = HitReactMontageRequestGeneration;
	TSharedPtr<FStreamableHandle> NewHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			HitReactMontage.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleHitReactMontagePreloadComplete,
				RequestGeneration));
	if (RequestGeneration != HitReactMontageRequestGeneration)
	{
		if (NewHandle.IsValid())
		{
			NewHandle->CancelHandle();
			NewHandle->ReleaseHandle();
		}
		return;
	}

	HitReactMontagePreloadHandle = MoveTemp(NewHandle);
	if (!HitReactMontagePreloadHandle.IsValid())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UHitReactAbility::HandleHitReactMontagePreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != HitReactMontageRequestGeneration
		|| !IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		return;
	}

	UAnimMontage* Montage = HitReactMontage.Get();
	if (!Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	StartHitReactMontage(
		Montage,
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo);
}

void UHitReactAbility::ReleaseHitReactMontagePreload()
{
	++HitReactMontageRequestGeneration;
	if (HitReactMontagePreloadHandle.IsValid())
	{
		HitReactMontagePreloadHandle->CancelHandle();
		HitReactMontagePreloadHandle->ReleaseHandle();
		HitReactMontagePreloadHandle.Reset();
	}
}

void UHitReactAbility::StartHitReactMontage(
	UAnimMontage* Montage,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
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

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		1.f,
		StartSectionName,
		false,
		1.0f,
		0.f,
		true);
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
