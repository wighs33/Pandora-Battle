#include "AbilitySystem/Ability/DashAbility.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RootMotionSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DashAbility)

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
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FSkillMovementSettings& MovementConfig = SkillDataAsset->Movement;
	if (SkillDataAsset->SkillDataType != ESkillDataType::Dash || !MovementConfig.bUseOneShotDash)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const double DashStrength = MovementConfig.DashStrength;
	const double DashDuration = MovementConfig.DashDuration;
	const bool bEnableGravityDuringDash = MovementConfig.bEnableGravityDuringDash;

	if (DashStrength <= 0.0 || DashDuration <= 0.0)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitDashCostAndMaybeCooldown(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!SkillDataAsset->bCancelOnHit)
	{
		// Dash owns a multi-charge commit path instead of CommitAbility, so it
		// opts into the same post-commit execution guarantee explicitly.
		SetCanBeCanceled(false);
	}

	UAbilityTask_ApplyRootMotionConstantForce* DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("Dash"),
		ResolveDashDirection(TriggerEventData),
		static_cast<float>(DashStrength),
		static_cast<float>(DashDuration),
		false,
		nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector,
		GetMaxSpeed(),
		bEnableGravityDuringDash);

	if (!DashTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GetPresentationRuntime().SpawnConfiguredCharacterDecal(*this);
	StartMovementContactDamage();

	if (SkillDataAsset->Niagara.GameplayCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		CueParameters.RawMagnitude = MovementConfig.bHideCharacterDuringDash ? 1.0f : -1.0f;
		K2_AddGameplayCueWithParams(SkillDataAsset->Niagara.GameplayCueTag, CueParameters, true);
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

	return ResolveDefaultDashDirection();
}

FVector UDashAbility::ResolveDefaultDashDirection() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const APawn* AvatarPawn = Cast<APawn>(AvatarActor);
	const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor);
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
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
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
		StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
		return true;
	}

	if (!CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, false))
	{
		return false;
	}

	DashChargesUsed = 0;
	StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
	return true;
}
