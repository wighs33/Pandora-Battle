#include "Component/Player/PlayerActionComponent.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerActionComponent)

namespace
{
	bool IsHitReactMontage(const UAnimMontage* Montage)
	{
		return Montage && Montage->GetName().Contains(TEXT("HitReact"), ESearchCase::IgnoreCase);
	}
}

UPlayerActionComponent::UPlayerActionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPlayerActionComponent::ApplyDefinition(const UPlayerPawnDefinition* Definition)
{
	HitReactCancelTags.Reset();
	if (Definition)
	{
		HitReactCancelTags = Definition->GetActionPolicySettings().MovementHitReactCancelTags;
	}

	if (HitReactCancelTags.IsEmpty())
	{
		HitReactCancelTags.AddTag(LabGameplayTags::Action_HitReact);
		HitReactCancelTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
	}
}

bool UPlayerActionComponent::RequestCancelHitReactForMovement(const float BlendOutTime)
{
	const bool bCancelledLocally = CancelHitReactForMovementLocally(BlendOutTime);
	const AActor* OwnerActor = GetOwner();
	if (bCancelledLocally && OwnerActor && !OwnerActor->HasAuthority())
	{
		ServerCancelHitReactForMovement(BlendOutTime);
	}

	return bCancelledLocally;
}

void UPlayerActionComponent::ServerCancelHitReactForMovement_Implementation(const float BlendOutTime)
{
	CancelHitReactForMovementLocally(BlendOutTime);
}

bool UPlayerActionComponent::CancelHitReactForMovementLocally(const float BlendOutTime)
{
	ACharacterBase* Character = Cast<ACharacterBase>(GetOwner());
	UAbilitySystemComponent* AbilitySystemComponent =
		Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (!Character || !AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayTagContainer CancelTags = ResolveHitReactCancelTags();
	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);

	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	UAnimMontage* ActiveMontage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	if (!OwnedTags.HasAny(CancelTags) && !IsHitReactMontage(ActiveMontage))
	{
		return false;
	}

	AbilitySystemComponent->CancelAbilities(&CancelTags);
	if (AnimInstance && ActiveMontage)
	{
		AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutTime), ActiveMontage);
	}

	if (Character->HasAuthority())
	{
		Character->ForceNetUpdate();
	}

	return true;
}

FGameplayTagContainer UPlayerActionComponent::ResolveHitReactCancelTags() const
{
	if (!HitReactCancelTags.IsEmpty())
	{
		return HitReactCancelTags;
	}

	FGameplayTagContainer FallbackTags;
	FallbackTags.AddTag(LabGameplayTags::Action_HitReact);
	FallbackTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
	return FallbackTags;
}

bool UPlayerActionComponent::StartCooldown(
	const ECharacterActionType ActionType,
	const double CooldownDuration)
{
	if (IsOnCooldown(ActionType))
	{
		return false;
	}

	if (CooldownDuration <= 0.0)
	{
		CooldownEndTimes.Remove(ActionType);
		CooldownDurations.Remove(ActionType);
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	CooldownEndTimes.Add(ActionType, World->GetTimeSeconds() + CooldownDuration);
	CooldownDurations.Add(ActionType, CooldownDuration);
	return true;
}

bool UPlayerActionComponent::IsOnCooldown(const ECharacterActionType ActionType) const
{
	return GetCooldownRemaining(ActionType) > 0.0f;
}

float UPlayerActionComponent::GetCooldownRemaining(const ECharacterActionType ActionType) const
{
	const double* CooldownEndTime = CooldownEndTimes.Find(ActionType);
	const UWorld* World = GetWorld();
	if (!CooldownEndTime || !World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, static_cast<float>(*CooldownEndTime - World->GetTimeSeconds()));
}

float UPlayerActionComponent::GetCooldownDuration(const ECharacterActionType ActionType) const
{
	const double* CooldownDuration = CooldownDurations.Find(ActionType);
	return CooldownDuration
		? static_cast<float>(FMath::Max(0.0, *CooldownDuration))
		: 0.0f;
}

void UPlayerActionComponent::ResetCooldowns()
{
	CooldownEndTimes.Reset();
	CooldownDurations.Reset();
}
