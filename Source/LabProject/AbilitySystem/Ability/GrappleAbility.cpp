#include "AbilitySystem/Ability/GrappleAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/TargetingActors/TargetActor_GrappleTrace.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Component/Player/GrappleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

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

void UGrappleAbility::OnAvatarSet(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	ReleaseCharacterActionDefinitionPreload();

	const TSoftObjectPtr<UCharacterActionDefinition> Definition(
		UCharacterActionDefinition::GetDefaultDefinitionPath());
	LoadedCharacterActionDefinition = Definition.Get();
	if (LoadedCharacterActionDefinition || Definition.IsNull())
	{
		return;
	}

	CharacterActionDefinitionPreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			Definition.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleCharacterActionDefinitionPreloadComplete));
}

void UGrappleAbility::OnRemoveAbility(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec& Spec)
{
	ReleaseCharacterActionDefinitionPreload();
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGrappleAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	UGrappleComponent* GrappleComponent = GetGrappleComponent();
	const APdPlayer* Player = Cast<APdPlayer>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!ActorInfo
		|| !Player
		|| !GrappleComponent
		|| Player->IsStatusFrozen()
		|| GrappleComponent->IsGrappling())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bCommittedGrapple = false;
	bStartedGrapple = false;
	GrappleFinishedDelegateHandle = GrappleComponent->OnGrappleFinished.AddUObject(
		this,
		&ThisClass::HandleGrappleFinished);

	SetLocalAimPresentation(true);
	StartTargetDataTask();
	StartInputReleaseTask();

	if (!WaitTargetDataTask || !WaitInputReleaseTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGrappleAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
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
	bCommittedGrapple = false;
	bStartedGrapple = false;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

const FGameplayTagContainer* UGrappleAbility::GetCooldownTags() const
{
	GrappleCooldownTags.Reset();
	GrappleCooldownTags.AddTag(LabGameplayTags::Cooldown_Grapple);
	return &GrappleCooldownTags;
}

void UGrappleAbility::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(LabGameplayTags::Cooldown_Grapple);
	ApplySharedCooldownEffect(
		Handle,
		ActorInfo,
		ActivationInfo,
		static_cast<float>(FMath::Max(GetConfiguredCooldownDuration(), 0.0)),
		CooldownTags);
}

void UGrappleAbility::StartTargetDataTask()
{
	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		TEXT("GrappleTargetData"),
		EGameplayTargetingConfirmation::Custom,
		ATargetActor_GrappleTrace::StaticClass());
	if (!WaitTargetDataTask)
	{
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	AGameplayAbilityTargetActor* TargetActor = BeginSpawningTargetDataActor(
		WaitTargetDataTask,
		ATargetActor_GrappleTrace::StaticClass());
	SpawnedTargetActor = Cast<ATargetActor_GrappleTrace>(TargetActor);
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

	APlayerController* PlayerController = ActorInfo->PlayerController.Get();
	if (APdHUD* HUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr)
	{
		if (bEnabled)
		{
			HUD->ShowAimCrosshair(LabGameplayTags::UI_Widget_AimCrosshair);
		}
		else
		{
			HUD->HideAimCrosshair();
		}
	}
}

UCharacterActionDefinition* UGrappleAbility::LoadCharacterActionDefinition() const
{
	if (LoadedCharacterActionDefinition)
	{
		return LoadedCharacterActionDefinition;
	}

	TSoftObjectPtr<UCharacterActionDefinition> Definition(
		UCharacterActionDefinition::GetDefaultDefinitionPath());
	return Definition.LoadSynchronous();
}

void UGrappleAbility::HandleCharacterActionDefinitionPreloadComplete()
{
	const TSoftObjectPtr<UCharacterActionDefinition> Definition(
		UCharacterActionDefinition::GetDefaultDefinitionPath());
	LoadedCharacterActionDefinition = Definition.Get();
}

void UGrappleAbility::ReleaseCharacterActionDefinitionPreload()
{
	if (CharacterActionDefinitionPreloadHandle.IsValid())
	{
		CharacterActionDefinitionPreloadHandle->CancelHandle();
		CharacterActionDefinitionPreloadHandle->ReleaseHandle();
		CharacterActionDefinitionPreloadHandle.Reset();
	}
	LoadedCharacterActionDefinition = nullptr;
}

double UGrappleAbility::GetConfiguredCooldownDuration() const
{
	const UCharacterActionDefinition* ActionDefinition = LoadCharacterActionDefinition();
	return ActionDefinition
		? ActionDefinition->GetCooldownDuration(ECharacterActionType::GrappleHook)
		: 0.0;
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
		bCommittedGrapple = CommitAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo);
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

	bCommittedGrapple = true;
	bStartedGrapple = GrappleComponent->StartGrappleFromValidatedHit(ServerHitResult);
	if (!bStartedGrapple)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UGrappleAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
