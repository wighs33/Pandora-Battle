#include "AbilitySystem/Ability/GrappleAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/TargetingActors/GrappleTargetActor.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerController.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Component/Player/GrappleComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GrappleAbility)

UGrappleAbility::UGrappleAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_Movement_Grapple);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Movement_Grapple_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::Status_Frostbite);
}

FGameplayTag UGrappleAbility::GetDefaultInputTag() const
{
	return LabGameplayTags::Input_Ability_Movement_Grapple;
}

void UGrappleAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	ReleaseCharacterActionDefinitionPreload();

	const TSoftObjectPtr<UCharacterActionDefinition> Definition(UCharacterActionDefinition::GetDefaultDefinitionPath());
	LoadedCharacterActionDefinition = Definition.Get();
	if (LoadedCharacterActionDefinition || Definition.IsNull())
	{
		return;
	}

	const AActor* AvatarActor = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UGameInstance* GameInstance = AvatarActor ? AvatarActor->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (ContentSubsystem)
	{
		CharacterActionDefinitionLease = ContentSubsystem->AcquireContent({ Definition.ToSoftObjectPath() },
			FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCharacterActionDefinitionPreloadComplete));
	}
}

void UGrappleAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	ReleaseCharacterActionDefinitionPreload();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGrappleAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	UGrappleComponent* GrappleComponent = GetGrappleComponent();
	const APdPlayer* Player = Cast<APdPlayer>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!ActorInfo || !Player || !GrappleComponent || Player->IsStatusFrozen() || GrappleComponent->IsGrappling())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	GrappleFinishedDelegateHandle = GrappleComponent->OnGrappleFinished.AddUObject(this,
		&ThisClass::HandleGrappleFinished);

	SetLocalAimPresentation(true);
	StartTargetDataTask();
	StartInputReleaseTask();

	if (!WaitTargetDataTask || !WaitInputReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGrappleAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	SetLocalAimPresentation(false);

	if (WaitInputReleaseTask)
	{
		WaitInputReleaseTask->EndTask();
		WaitInputReleaseTask = nullptr;
	}
	if (WaitTargetDataTask)
	{
		WaitTargetDataTask->EndTask();
		WaitTargetDataTask = nullptr;
	}
	if (IsValid(SpawnedTargetActor))
	{
		SpawnedTargetActor->Destroy();
	}
	SpawnedTargetActor = nullptr;

	if (UGrappleComponent* GrappleComponent = GetGrappleComponent())
	{
		if (GrappleFinishedDelegateHandle.IsValid())
		{
			GrappleComponent->OnGrappleFinished.Remove(GrappleFinishedDelegateHandle);
		}

		if (GrappleComponent->IsGrappling())
		{
			GrappleComponent->StopGrapple();
		}
	}

	GrappleFinishedDelegateHandle.Reset();
}

const FGameplayTagContainer* UGrappleAbility::GetCooldownTags() const
{
	GrappleCooldownTags.Reset();
	GrappleCooldownTags.AddTag(LabGameplayTags::Cooldown_Grapple);
	return &GrappleCooldownTags;
}

void UGrappleAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(LabGameplayTags::Cooldown_Grapple);
	ApplySharedCooldownEffect(Handle, ActorInfo, ActivationInfo,
		static_cast<float>(FMath::Max(GetConfiguredCooldownDuration(), 0.0)), CooldownTags);
}

void UGrappleAbility::StartTargetDataTask()
{
	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(this, TEXT("GrappleTargetData"),
		EGameplayTargetingConfirmation::Custom, AGrappleTargetActor::StaticClass());
	if (!WaitTargetDataTask)
	{
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	AGameplayAbilityTargetActor* TargetActor = BeginSpawningTargetDataActor(WaitTargetDataTask,
		AGrappleTargetActor::StaticClass());
	SpawnedTargetActor = Cast<AGrappleTargetActor>(TargetActor);
	if (TargetActor)
	{
		FinishSpawningTargetDataActor(WaitTargetDataTask, TargetActor);
	}

	WaitTargetDataTask->ReadyForActivation();
}

void UGrappleAbility::StartInputReleaseTask()
{
	WaitInputReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	if (!WaitInputReleaseTask)
	{
		return;
	}

	WaitInputReleaseTask->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	WaitInputReleaseTask->ReadyForActivation();
}

void UGrappleAbility::SetLocalAimPresentation(const bool bEnabled) const
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	if (APdPlayer* Player = Cast<APdPlayer>(ActorInfo->AvatarActor.Get()))
	{
		const UCharacterActionDefinition* ActionDefinition = LoadCharacterActionDefinition();
		const FWeaponAimCameraSettings CameraSettings = ActionDefinition
			? ActionDefinition->GetAimCameraSettings(ECharacterActionType::GrappleHook)
			: FWeaponAimCameraSettings();
		Player->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
	}

	if (APdPlayerController* PlayerController = Cast<APdPlayerController>(ActorInfo->PlayerController.Get()))
	{
		if (bEnabled)
		{
			PlayerController->ShowAimCrosshair(LabGameplayTags::UI_Widget_AimCrosshair);
		}
		else
		{
			PlayerController->HideAimCrosshair();
		}
	}
}

UCharacterActionDefinition* UGrappleAbility::LoadCharacterActionDefinition() const
{
	if (LoadedCharacterActionDefinition)
	{
		return LoadedCharacterActionDefinition;
	}

	TSoftObjectPtr<UCharacterActionDefinition> Definition(UCharacterActionDefinition::GetDefaultDefinitionPath());
	return Definition.LoadSynchronous();
}

void UGrappleAbility::HandleCharacterActionDefinitionPreloadComplete()
{
	const TSoftObjectPtr<UCharacterActionDefinition> Definition(UCharacterActionDefinition::GetDefaultDefinitionPath());
	LoadedCharacterActionDefinition = Definition.Get();
}

void UGrappleAbility::ReleaseCharacterActionDefinitionPreload()
{
	CharacterActionDefinitionLease.Reset();
	LoadedCharacterActionDefinition = nullptr;
}

double UGrappleAbility::GetConfiguredCooldownDuration() const
{
	const UCharacterActionDefinition* ActionDefinition = LoadCharacterActionDefinition();
	return ActionDefinition ? ActionDefinition->GetCooldownDuration(ECharacterActionType::GrappleHook) : 0.0;
}

UGrappleComponent* UGrappleAbility::GetGrappleComponent() const
{
	const APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	return Player ? Player->GetGrappleComponent() : nullptr;
}

void UGrappleAbility::HandleGrappleFinished()
{
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UGrappleAbility::HandleInputReleased(const float TimeHeld)
{
	static_cast<void>(TimeHeld);
	SetLocalAimPresentation(false);

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	if (IsValid(SpawnedTargetActor))
	{
		SpawnedTargetActor->ConfirmTargeting();
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UGrappleAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* ClientHitResult = TargetData ? TargetData->GetHitResult() : nullptr;
	UGrappleComponent* GrappleComponent = GetGrappleComponent();
	if (!ClientHitResult || !GrappleComponent)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		const bool bCommittedGrapple = CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
		if (!bCommittedGrapple)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
		return;
	}

	FHitResult ServerHitResult;
	if (!GrappleComponent->ValidateTargetDataAndTrace(*ClientHitResult, ServerHitResult)
		|| !CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (!GrappleComponent->StartGrappleFromValidatedHit(ServerHitResult))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGrappleAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
