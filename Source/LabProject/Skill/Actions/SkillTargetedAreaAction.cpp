#include "Skill/Actions/SkillTargetedAreaAction.h"

#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/TargetValidator.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystem/TargetingActors/GroundTargetActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Skill/Actions/SkillAreaTargeting.h"
#include "Skill/SkillDebugDraw.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTargetedAreaAction)

DEFINE_LOG_CATEGORY_STATIC(LogSkillTargetedAreaAction, Log, All);

USkillTargetedAreaAction::USkillTargetedAreaAction()
{
	Settings.TargetActorClass = AGroundTargetActor::StaticClass();
}

void USkillTargetedAreaAction::OnStart()
{
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		Finish(false);
		return;
	}

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		UE_LOG(LogSkillTargetedAreaAction, Warning, TEXT("%s has no source skill definition, so targeting cannot start."),
			*GetNameSafe(GetAbility()));
		Finish(false);
		return;
	}

	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredTargetActorClass = GetConfiguredTargetActorClass();
	if (!ConfiguredTargetActorClass || Settings.Radius <= 0.0)
	{
		UE_LOG(LogSkillTargetedAreaAction, Warning, TEXT("%s needs a target actor class and a positive radius."),
			*GetNameSafe(SkillDataAsset));
		Finish(false);
		return;
	}

	bStrikeTriggered = false;
	bStrikeConfirmed = false;
	bWaitingLightningDamage = false;
	bIsWaitingTargetData = false;
	ConfirmedAOELocation = FVector::ZeroVector;
	HitActorKeys.Reset();
	AOEOverlapResults.Reset();
	CachedAOERadius = Settings.Radius;

	if (AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar(); IsValid(AttackTarget))
	{
		ConfirmedAOELocation = PdSkillAreaTargeting::ResolveTargetLocation(
			*AttackTarget, GetAbility()->GetAvatarActorFromActorInfo(), Settings);
		ConfirmStrike();
		return;
	}

	WaitCancelInput();
	StartTargeting();
}

void USkillTargetedAreaAction::OnStop()
{
	GetAbility()->RestoreAvatarMovementForAbility();
	bIsWaitingTargetData = false;
	bWaitingLightningDamage = false;
	HitActorKeys.Reset();
	AOEOverlapResults.Reset();
	RemovePersistentGameplayCues();

	if (WaitCancelInputTask)
	{
		WaitCancelInputTask->EndTask();
		WaitCancelInputTask = nullptr;
	}
	if (TargetingMontageTask)
	{
		TargetingMontageTask->EndTask();
		TargetingMontageTask = nullptr;
	}
	if (WaitTargetDataTask)
	{
		WaitTargetDataTask->EndTask();
		WaitTargetDataTask = nullptr;
	}
	if (TriggerMontageTask)
	{
		TriggerMontageTask->EndTask();
		TriggerMontageTask = nullptr;
	}
	if (WaitMontageTriggerTask)
	{
		WaitMontageTriggerTask->EndTask();
		WaitMontageTriggerTask = nullptr;
	}
	if (LightningDamageDelayTask)
	{
		LightningDamageDelayTask->EndTask();
		LightningDamageDelayTask = nullptr;
	}
}

void USkillTargetedAreaAction::StartTargeting()
{
	if (!GetAbility()->HasPlayerController())
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			Finish(false);
		}
		return;
	}

	bIsWaitingTargetData = true;
	if (const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
		SkillDataAsset && SkillDataAsset->Movement.bLockMovementDuringDuration)
	{
		GetAbility()->LockAvatarMovementForAbility();
	}

	ApplyDirectAOECamera(true);

	LoopTargetingAnimation();
	StartWaitTargetData();
}

void USkillTargetedAreaAction::LoopTargetingAnimation()
{
	UAnimMontage* ConfiguredTargetingMontage = GetConfiguredTargetingMontage();
	if (!bIsWaitingTargetData || !ConfiguredTargetingMontage)
	{
		return;
	}

	TargetingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(GetAbility(), NAME_None,
		ConfiguredTargetingMontage, 1.0f, NAME_None, true, 1.0f, 0.0f, false);
	if (!TargetingMontageTask)
	{
		return;
	}

	TargetingMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleTargetingMontageBlendOut);
	TargetingMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleTargetingMontageInterrupted);
	TargetingMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleTargetingMontageInterrupted);
	TargetingMontageTask->ReadyForActivation();
}

void USkillTargetedAreaAction::ConfirmStrike()
{
	if (bStrikeConfirmed)
	{
		return;
	}

	bIsWaitingTargetData = false;
	GetAbility()->RestoreAvatarMovementForAbility();

	if (ConfirmedAOELocation.IsNearlyZero())
	{
		if (!ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			Finish(false);
			return;
		}
	}
	bStrikeConfirmed = true;

	DrawDebugDamageRadius(FColor::Cyan, FColor::Blue);

	FGameplayCueParameters IndicatorParams;
	IndicatorParams.RawMagnitude = static_cast<float>(CachedAOERadius * 2.0);
	IndicatorParams.Location = ConfirmedAOELocation;
	IndicatorParams.Instigator = GetAbility()->GetAvatarActorFromActorInfo();
	IndicatorParams.EffectCauser = GetAbility()->GetAvatarActorFromActorInfo();

	const FGameplayTag ConfiguredAOEIndicatorCueTag = Settings.IndicatorCueTag;
	if (ConfiguredAOEIndicatorCueTag.IsValid())
	{
		GetAbility()->K2_AddGameplayCueWithParams(ConfiguredAOEIndicatorCueTag, IndicatorParams, true);
	}

	StartWaitMontageTrigger();

	UAnimMontage* ConfiguredTriggerMontage = GetConfiguredTriggerMontage();
	if (!ConfiguredTriggerMontage)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
		return;
	}

	TriggerMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(ConfiguredTriggerMontage);
	if (!TriggerMontageTask)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
		return;
	}

	TriggerMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleTriggerMontageFinished);
	TriggerMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleTriggerMontageInterrupted);
	TriggerMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleTriggerMontageInterrupted);
	TriggerMontageTask->ReadyForActivation();
}

void USkillTargetedAreaAction::AOEDamage()
{
	if (!GetAbility()->K2_HasAuthority())
	{
		return;
	}

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOEDamage), false, AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(static_cast<float>(CachedAOERadius));

	DrawDebugDamageRadius(FColor::Yellow, FColor::Red);

	AOEOverlapResults.Reset();
	World->OverlapMultiByObjectType(AOEOverlapResults, ConfirmedAOELocation, FQuat::Identity, ObjectQueryParams,
		SphereShape, QueryParams);

	HitActorKeys.Reset();
	TArray<TWeakObjectPtr<AActor>> DamageTargets;
	DamageTargets.Reserve(AOEOverlapResults.Num());
	for (const FOverlapResult& OverlapResult : AOEOverlapResults)
	{
		AActor* HitActor = OverlapResult.GetActor();
		if (!IsValid(HitActor) || HitActor == AvatarActor)
		{
			continue;
		}

		const FObjectKey HitActorKey(HitActor);
		if (HitActorKeys.Contains(HitActorKey))
		{
			continue;
		}

		HitActorKeys.Add(HitActorKey);
		DamageTargets.Add(HitActor);
	}

	for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
	{
		AActor* HitActor = TargetPtr.Get();
		if (!IsValid(HitActor))
		{
			continue;
		}

		ApplyEffectToHitActor(HitActor);
	}
}

void USkillTargetedAreaAction::WaitCancelInput()
{
	WaitCancelInputTask = UAbilityTask_WaitInputPress::WaitInputPress(GetAbility(), false);
	if (!WaitCancelInputTask)
	{
		return;
	}

	WaitCancelInputTask->OnPress.AddDynamic(this, &ThisClass::HandleCancelInputPressed);
	WaitCancelInputTask->ReadyForActivation();
}

void USkillTargetedAreaAction::StartWaitTargetData()
{
	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredTargetActorClass = GetConfiguredTargetActorClass();
	if (!ConfiguredTargetActorClass)
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			Finish(false);
		}
		return;
	}

	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(GetAbility(), NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed, ConfiguredTargetActorClass);
	if (!WaitTargetDataTask)
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			Finish(false);
		}
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	if (AGameplayAbilityTargetActor* SpawnedTargetActor =
		GetAbility()->BeginSpawningTargetDataActor(WaitTargetDataTask, ConfiguredTargetActorClass))
	{
		ConfigureSpawnedTargetActor(SpawnedTargetActor);
		GetAbility()->FinishSpawningTargetDataActor(WaitTargetDataTask, SpawnedTargetActor);
	}

	WaitTargetDataTask->ReadyForActivation();
}

void USkillTargetedAreaAction::StartWaitMontageTrigger()
{
	const FGameplayTag ConfiguredMontageTriggerEventTag = GetConfiguredMontageTriggerEventTag();
	if (!ConfiguredMontageTriggerEventTag.IsValid())
	{
		return;
	}

	WaitMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(ConfiguredMontageTriggerEventTag, true);
	if (!WaitMontageTriggerTask)
	{
		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

void USkillTargetedAreaAction::StartLightningDamageDelay()
{
	bWaitingLightningDamage = true;
	const float ConfiguredLightningDamageDelay = static_cast<float>(FMath::Max(Settings.DamageDelay, 0.0));

	if (ConfiguredLightningDamageDelay <= 0.0f)
	{
		HandleLightningDamageDelayFinished();
		return;
	}

	LightningDamageDelayTask = UAbilityTask_WaitDelay::WaitDelay(GetAbility(), ConfiguredLightningDamageDelay);
	if (!LightningDamageDelayTask)
	{
		HandleLightningDamageDelayFinished();
		return;
	}

	LightningDamageDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleLightningDamageDelayFinished);
	LightningDamageDelayTask->ReadyForActivation();
}

void USkillTargetedAreaAction::ConfigureSpawnedTargetActor(AGameplayAbilityTargetActor* SpawnedActor)
{
	if (!SpawnedActor)
	{
		return;
	}

	SpawnedActor->StartLocation = MakeTargetStartLocation();
	SpawnedActor->ReticleClass = nullptr;
	SpawnedActor->bDebug = GetConfiguredDebugTargeting();

	if (AGameplayAbilityTargetActor_Trace* TraceActor = Cast<AGameplayAbilityTargetActor_Trace>(SpawnedActor))
	{
		TraceActor->MaxRange = static_cast<float>(Settings.TargetingMaxRange);
		TraceActor->TraceProfile = FCollisionProfileName(Settings.TargetingTraceProfileName);
		TraceActor->bTraceAffectsAimPitch = Settings.bTargetingTraceAffectsAimPitch;
	}

	if (AGameplayAbilityTargetActor_GroundTrace* GroundTraceActor = Cast<AGameplayAbilityTargetActor_GroundTrace>(SpawnedActor))
	{
		GroundTraceActor->CollisionRadius = static_cast<float>(Settings.TargetingCollisionRadius);
		GroundTraceActor->CollisionHeight = static_cast<float>(Settings.TargetingCollisionHeight);
	}

	if (AGroundTargetActor* DecalTargetActor = Cast<AGroundTargetActor>(SpawnedActor))
	{
		const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
		UMaterialInterface* TargetingDecal = GetConfiguredTargetingDecal();
		const double TargetingDecalSize = GetConfiguredTargetingDecalSize();
		const bool bUseCharacterDecal = SkillDataAsset && SkillDataAsset->CharacterDecal.DecalMaterial;
		const bool bGrowCharacterDecal = bUseCharacterDecal && SkillDataAsset->CharacterDecal.bGrowDecalSize;

		DecalTargetActor->Decal = TargetingDecal;
		DecalTargetActor->DecalSize = TargetingDecalSize;
		DecalTargetActor->bOverrideDecalColor = false;
		DecalTargetActor->DecalColor = Settings.TargetingDecalColor;

		if (bGrowCharacterDecal)
		{
			const FSkillDecalSettings& DecalSettings = SkillDataAsset->CharacterDecal;
			const double StartSize = DecalSettings.DecalSize > 0.0 ? DecalSettings.DecalSize : 512.0;
			const double FinalSize = DecalSettings.FinalDecalSize > 0.0 ? DecalSettings.FinalDecalSize : StartSize;
			DecalTargetActor->ConfigureDecalGrowth(
				StartSize, FinalSize, GetAbility()->HasDurationDeadline() ? GetAbility()->GetRemainingDuration() : 2.0f);
		}
	}
}

void USkillTargetedAreaAction::ApplyEffectToHitActor(AActor* HitActor)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(GetAbility()->GetAvatarActorFromActorInfo());
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (SourceCharacter && TargetCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAbility()->GetAvatarActorFromActorInfo());
	if (!SourceASC || !TargetASC)
	{
		return;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec();
	if (DamageSpecHandle.IsValid())
	{
		ApplyDamageWithConfiguredStatus(*SourceASC, *TargetASC, *DamageSpecHandle.Data);
	}
}

void USkillTargetedAreaAction::ApplyDirectAOECamera(bool bEnabled) const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset || !Settings.bUseCameraSettings)
	{
		return;
	}

	APdPlayer* Player = Cast<APdPlayer>(GetAbility()->GetAvatarActorFromActorInfo());
	if (!Player || !Player->IsLocallyControlled())
	{
		return;
	}

	const FWeaponAimCameraSettings CameraSettings = Settings.CameraSettings;

	Player->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

void USkillTargetedAreaAction::RemovePersistentGameplayCues()
{
	if (!GetAbility()->GetSourceSkillDataAsset())
	{
		return;
	}

	ApplyDirectAOECamera(false);

	const FGameplayTag ConfiguredAOEIndicatorCueTag = Settings.IndicatorCueTag;
	if (ConfiguredAOEIndicatorCueTag.IsValid())
	{
		GetAbility()->K2_RemoveGameplayCue(ConfiguredAOEIndicatorCueTag);
	}
}

FGameplayAbilityTargetingLocationInfo USkillTargetedAreaAction::MakeTargetStartLocation()
{
	const FName ConfiguredTargetingSocketName = Settings.TargetingSocketName;
	if (!ConfiguredTargetingSocketName.IsNone())
	{
		return GetAbility()->MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(ConfiguredTargetingSocketName);
	}

	return GetAbility()->MakeTargetLocationInfoFromOwnerActor();
}

bool USkillTargetedAreaAction::ResolveFallbackAOELocation(FVector& OutGroundLocation) const
{
	if (!(IsRunning() && GetAbility()->CanRunActions()))
	{
		return false;
	}

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar(); IsValid(AttackTarget))
	{
		OutGroundLocation = PdSkillAreaTargeting::ResolveTargetLocation(*AttackTarget, AvatarActor, Settings);
		return true;
	}

	if (!AvatarActor || !AvatarActor->GetWorld())
	{
		return false;
	}

	OutGroundLocation = PdSkillAreaTargeting::ResolveForwardLocation(*AvatarActor, Settings);
	return true;
}

bool USkillTargetedAreaAction::TryValidateServerAOELocation(const FHitResult& ClientHitResult,
	const FVector& TargetDataEndPoint, FVector& OutValidatedLocation)
{
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	FVector RequestedLocation = FVector::ZeroVector;
	if (!PdTargetValidator::TryResolveAuthorityTargetRequest(AvatarActor, ClientHitResult, TargetDataEndPoint, RequestedLocation))
	{
		return false;
	}

	UWorld* World = AvatarActor->GetWorld();

	const FGameplayAbilityTargetingLocationInfo TargetStartLocation = MakeTargetStartLocation();
	const FVector AuthoritySourceLocation = TargetStartLocation.GetTargetingTransform().GetLocation();

	PdTargetValidator::FGroundTargetValidationParams ValidationParams;
	ValidationParams.MaxRange = static_cast<float>(Settings.TargetingMaxRange);
	ValidationParams.GroundTraceDepth = static_cast<float>(Settings.TargetGroundTraceDepth);
	ValidationParams.GroundTraceType = Settings.TargetGroundTraceChannel;
	ValidationParams.LineOfSightProfileName = Settings.TargetingTraceProfileName;

	PdTargetValidator::FValidatedGroundTarget ValidatedTarget;
	if (!PdTargetValidator::ValidateGroundTarget(World, AvatarActor, AuthoritySourceLocation, RequestedLocation,
		ValidationParams, ValidatedTarget))
	{
		return false;
	}

	OutValidatedLocation = ValidatedTarget.Location;
	return true;
}

FGameplayEffectSpecHandle USkillTargetedAreaAction::MakeDamageEffectSpec() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return FGameplayEffectSpecHandle();
	}
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	return GetAbility()->MakeConfiguredDamageEffectSpec(
		DamageConfig, GetAbility()->CalculateDamageMagnitude(DamageConfig));
}

UAnimMontage* USkillTargetedAreaAction::GetConfiguredTargetingMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.SecondaryMontage.Get() : nullptr;
}

UAnimMontage* USkillTargetedAreaAction::GetConfiguredTriggerMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

TSubclassOf<AGameplayAbilityTargetActor> USkillTargetedAreaAction::GetConfiguredTargetActorClass() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredClass =
		SkillDataAsset ? Settings.TargetActorClass : nullptr;
	if (ConfiguredClass && ConfiguredClass->IsChildOf(AGameplayAbilityTargetActor_GroundTrace::StaticClass())
		&& !ConfiguredClass->IsChildOf(AGroundTargetActor::StaticClass()))
	{
		return AGroundTargetActor::StaticClass();
	}

	return ConfiguredClass;
}

UMaterialInterface* USkillTargetedAreaAction::GetConfiguredTargetingDecal() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return nullptr;
	}

	if (SkillDataAsset->CharacterDecal.DecalMaterial)
	{
		return SkillDataAsset->CharacterDecal.DecalMaterial.Get();
	}

	return Settings.TargetingDecal.Get();
}

double USkillTargetedAreaAction::GetConfiguredTargetingDecalSize() const
{
	const double FallbackSize = CachedAOERadius > 0.0 ? CachedAOERadius * 2.0 : 512.0;
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset || !SkillDataAsset->CharacterDecal.DecalMaterial)
	{
		return FallbackSize;
	}

	const FSkillDecalSettings& DecalSettings = SkillDataAsset->CharacterDecal;
	const double StartSize = DecalSettings.DecalSize > 0.0 ? DecalSettings.DecalSize : 512.0;
	return DecalSettings.bGrowDecalSize && DecalSettings.FinalDecalSize > 0.0
		? DecalSettings.FinalDecalSize
		: StartSize;
}

bool USkillTargetedAreaAction::GetConfiguredDebugTargeting() const
{
	return LabSkillDebug::IsDrawingEnabled() && Settings.bDebugTargeting;
}

FGameplayTag USkillTargetedAreaAction::GetConfiguredMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryEventTag : FGameplayTag();
}

bool USkillTargetedAreaAction::ShouldDrawDebugDamageRadius() const
{
	return LabSkillDebug::IsDrawingEnabled() && Settings.bDrawDebugDamageRadius;
}

void USkillTargetedAreaAction::DrawDebugDamageRadius(const FColor& CircleColor, const FColor& SphereColor) const
{
	if (!ShouldDrawDebugDamageRadius())
	{
		return;
	}

	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	LabSkillDebug::DrawAreaRadius(AvatarActor ? AvatarActor->GetWorld() : nullptr, ConfirmedAOELocation,
		static_cast<float>(CachedAOERadius), CircleColor, SphereColor,
		static_cast<float>(FMath::Max(Settings.DebugDamageRadiusDrawTime, 0.0)));
}

void USkillTargetedAreaAction::HandleCancelInputPressed(float TimeWaited)
{
	static_cast<void>(TimeWaited);
	if (UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->LocalInputConfirm();
	}
	else if (ResolveFallbackAOELocation(ConfirmedAOELocation))
	{
		ConfirmStrike();
	}
}

void USkillTargetedAreaAction::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	bIsWaitingTargetData = false;

	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* ClientHitResult = TargetData ? TargetData->GetHitResult() : nullptr;
	if (!GetAbility()->GetCurrentActorInfo() || !ClientHitResult)
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			Finish(false);
		}
		return;
	}

	const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	if (GetAbility()->GetCurrentActorInfo()->IsNetAuthority())
	{
		if (!TryValidateServerAOELocation(*ClientHitResult, TargetDataEndPoint, ConfirmedAOELocation))
		{
			if (ResolveFallbackAOELocation(ConfirmedAOELocation))
			{
				ConfirmStrike();
			}
			else
			{
				Finish(false);
			}
			return;
		}
	}
	else
	{
		ConfirmedAOELocation = PdSkillAreaTargeting::ResolveAimedLocation(
			*ClientHitResult, TargetDataEndPoint, GetAbility()->GetAvatarActorFromActorInfo(), Settings);
	}

	ConfirmStrike();
}

void USkillTargetedAreaAction::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	if (ResolveFallbackAOELocation(ConfirmedAOELocation))
	{
		ConfirmStrike();
	}
	else
	{
		Finish(false);
	}
}

void USkillTargetedAreaAction::HandleTargetingMontageBlendOut()
{
	TargetingMontageTask = nullptr;
	if (bIsWaitingTargetData)
	{
		LoopTargetingAnimation();
	}
}

void USkillTargetedAreaAction::HandleTargetingMontageInterrupted()
{
	TargetingMontageTask = nullptr;
	if (bIsWaitingTargetData)
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			Finish(false);
		}
	}
}

void USkillTargetedAreaAction::HandleTriggerMontageFinished()
{
	TriggerMontageTask = nullptr;

	if (!bStrikeTriggered)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
	}
}

void USkillTargetedAreaAction::HandleTriggerMontageInterrupted()
{
	TriggerMontageTask = nullptr;
	if (!bStrikeTriggered)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
	}
}

void USkillTargetedAreaAction::HandleMontageTriggerEvent(FGameplayEventData Payload)
{
	if (bStrikeTriggered)
	{
		return;
	}

	bStrikeTriggered = true;

	if (!GetAbility()->CommitSkill())
	{
		Finish();
		return;
	}

	GetAbility()->SpawnConfiguredCharacterDecal();
	FGameplayCueParameters LightningCueParams;
	LightningCueParams.Location = ConfirmedAOELocation;
	LightningCueParams.Instigator = GetAbility()->GetAvatarActorFromActorInfo();
	LightningCueParams.EffectCauser = GetAbility()->GetAvatarActorFromActorInfo();

	const FGameplayTag ResolvedLightningBoltCueTag = Settings.ImpactCueTag;
	if (ResolvedLightningBoltCueTag.IsValid())
	{
		GetAbility()->K2_ExecuteGameplayCueWithParams(ResolvedLightningBoltCueTag, LightningCueParams);
	}

	StartLightningDamageDelay();
}

void USkillTargetedAreaAction::HandleLightningDamageDelayFinished()
{
	const bool bHasAuthority = GetAbility()->K2_HasAuthority();
	LightningDamageDelayTask = nullptr;
	bWaitingLightningDamage = false;

	if (!bHasAuthority)
	{
		return;
	}

	AOEDamage();
	RemovePersistentGameplayCues();
	Finish();
}
