#include "AbilitySystem/Ability/DashAbility.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RootMotionSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DashAbility)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraDashAbility, Log, All);

UDashAbility::UDashAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FAbilityTriggerData DashTrigger;
	DashTrigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	DashTrigger.TriggerTag = LabGameplayTags::Event_ActivateAbility_Dash;
	AbilityTriggers.Add(DashTrigger);

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_Movement_Dash);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Movement_Dash_Active);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility);
}

void UDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		UE_LOG(LogPandoraDashAbility, Warning,
			TEXT("Dash cancelled: missing SkillDataAsset. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (SkillDataAsset->DashStrength <= 0.0 || SkillDataAsset->DashDuration <= 0.0)
	{
		UE_LOG(LogPandoraDashAbility, Warning,
			TEXT("Dash cancelled: invalid SkillDataAsset values. skill=%s strength=%.2f duration=%.2f"),
			*GetNameSafe(SkillDataAsset),
			SkillDataAsset->DashStrength,
			SkillDataAsset->DashDuration);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (SkillDataAsset->DashCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		K2_AddGameplayCueWithParams(SkillDataAsset->DashCueTag, CueParameters, true);
	}

	if (!CommitDashCostAndMaybeCooldown(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_ApplyRootMotionConstantForce* DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("Dash"),
		ResolveDashDirection(TriggerEventData),
		static_cast<float>(SkillDataAsset->DashStrength),
		static_cast<float>(SkillDataAsset->DashDuration),
		false,
		nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector,
		GetMaxSpeed(),
		SkillDataAsset->bEnableGravityDuringDash);

	if (!DashTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashRootMotionFinished);
	DashTask->ReadyForActivation();
}

void UDashAbility::OnDashRootMotionFinished()
{
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

FVector UDashAbility::ResolveDashDirection(const FGameplayEventData* TriggerEventData) const
{
	if (TriggerEventData)
	{
		for (int32 Index = 0; Index < TriggerEventData->TargetData.Num(); ++Index)
		{
			const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(Index);
			const FHitResult* HitResult = TargetData ? TargetData->GetHitResult() : nullptr;
			if (!HitResult)
			{
				continue;
			}

			FVector Direction = HitResult->Location;
			Direction.Z = 0.0f;
			if (!Direction.IsNearlyZero())
			{
				return Direction.GetSafeNormal();
			}
		}
	}

	return GetFallbackDashDirection();
}

FVector UDashAbility::GetFallbackDashDirection() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const APdCharacterBase* Character = Cast<APdCharacterBase>(AvatarActor);
	const UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;

	FVector Direction = MovementComponent ? MovementComponent->GetCurrentAcceleration() : FVector::ZeroVector;
	if (Direction.IsNearlyZero() && AvatarPawn)
	{
		Direction = AvatarPawn->GetLastMovementInputVector();
	}
	Direction.Z = 0.0f;

	if (Direction.IsNearlyZero() && AvatarActor)
	{
		Direction = AvatarActor->GetActorForwardVector();
		Direction.Z = 0.0f;
	}

	if (Direction.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}

	return Direction.GetSafeNormal();
}

float UDashAbility::GetMaxSpeed() const
{
	const APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	const UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	return MovementComponent ? MovementComponent->GetMaxSpeed() : 500.0f;
}

int32 UDashAbility::GetMaxDashCharges() const
{
	return FMath::Max(GetAbilityLevel(), 1);
}

bool UDashAbility::CommitDashCostAndMaybeCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!CheckCost(Handle, ActorInfo) || !CheckCooldown(Handle, ActorInfo))
	{
		return false;
	}

	if (!CommitAbilityCost(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	++DashChargesUsed;
	if (DashChargesUsed < GetMaxDashCharges())
	{
		return true;
	}

	if (!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		return false;
	}

	DashChargesUsed = 0;
	return true;
}
