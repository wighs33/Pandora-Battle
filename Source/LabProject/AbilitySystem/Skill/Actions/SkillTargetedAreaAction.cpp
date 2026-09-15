#include "AbilitySystem/Skill/Actions/SkillTargetedAreaAction.h"

#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/TargetValidator.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Map/MapLayerTrigger.h"
#include "Map/OutOfBoundsRespawnVolume.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTargetedAreaAction)

namespace
{
	constexpr float AOEGroundProjectionStartHeight = 500.0f;
	constexpr float AOEGroundProjectionMinDepth = 1000.0f;
	constexpr float AOEGroundMinNormalZ = 0.35f;

	const USkillDefinition* GetAOESkillDataAsset(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset
			? SkillDataAsset
			: nullptr;
	}

	bool IsIgnoredAOEGroundActor(const AActor* Actor)
	{
		return Actor
			&& (Actor->IsA<AOutOfBoundsRespawnVolume>()
				|| Actor->IsA<AMapLayerTrigger>());
	}

	bool IsValidAOEGroundHit(const FHitResult& Hit)
	{
		return Hit.bBlockingHit
			&& Hit.ImpactNormal.Z >= AOEGroundMinNormalZ
			&& !Cast<APawn>(Hit.GetActor())
			&& !IsIgnoredAOEGroundActor(Hit.GetActor());
	}

	FVector ResolveActorFeetLocation(const AActor* Actor)
	{
		if (!Actor)
		{
			return FVector::ZeroVector;
		}

		FVector FeetLocation = Actor->GetActorLocation();
		if (const ACharacterBase* Character = Cast<ACharacterBase>(Actor))
		{
			if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent())
			{
				FeetLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
			}
		}

		return FeetLocation;
	}

	bool TryResolveGroundHitLocation(
		UWorld* World,
		const FVector& SourceLocation,
		const TArray<AActor*>& ActorsToIgnore,
		const TEnumAsByte<ETraceTypeQuery> TraceType,
		const float TraceDepth,
		FVector& OutGroundLocation)
	{
		if (!World)
		{
			return false;
		}

		const float ResolvedTraceDepth = FMath::Max(TraceDepth, AOEGroundProjectionMinDepth);
		const FVector TraceStart = SourceLocation + FVector::UpVector * AOEGroundProjectionStartHeight;
		const FVector TraceEnd = SourceLocation - FVector::UpVector * ResolvedTraceDepth;

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOEGroundProjection), false);
		for (AActor* ActorToIgnore : ActorsToIgnore)
		{
			if (IsValid(ActorToIgnore))
			{
				QueryParams.AddIgnoredActor(ActorToIgnore);
			}
		}

		const ECollisionChannel TraceChannel = UEngineTypes::ConvertToCollisionChannel(TraceType);
		if (TraceChannel != ECC_MAX)
		{
			TArray<FHitResult> ChannelHits;
			if (World->LineTraceMultiByChannel(ChannelHits, TraceStart, TraceEnd, TraceChannel, QueryParams))
			{
				for (const FHitResult& Hit : ChannelHits)
				{
					if (IsValidAOEGroundHit(Hit))
					{
						OutGroundLocation = Hit.ImpactPoint;
						return true;
					}
				}
			}
		}

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		TArray<FHitResult> ObjectHits;
		if (World->LineTraceMultiByObjectType(ObjectHits, TraceStart, TraceEnd, ObjectParams, QueryParams))
		{
			for (const FHitResult& Hit : ObjectHits)
			{
				if (IsValidAOEGroundHit(Hit))
				{
					OutGroundLocation = Hit.ImpactPoint;
					return true;
				}
			}
		}

		return false;
	}
}

void USkillTargetedAreaAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		Finish(!(true));
		return;
	}

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		Finish(!(true));
		return;
	}

	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredTargetActorClass = GetConfiguredTargetActorClass();
	const double ConfiguredAOERadius = CalculateAOERadiusFromSkillData();
	if (!ConfiguredTargetActorClass || ConfiguredAOERadius <= 0.0)
	{

		Finish(!(true));
		return;
	}

	bStrikeTriggered = false;
	bStrikeConfirmed = false;
	bWaitingLightningDamage = false;
	bIsWaitingTargetData = false;
	ConfirmedAOELocation = FVector::ZeroVector;
	HitActorKeys.Reset();
	AOEOverlapResults.Reset();
	CachedAOERadius = CalculateAOERadiusFromSkillData();

	if (AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar(); IsValid(AttackTarget))
	{
		if (!GetTargetGroundLocation(AttackTarget, ConfirmedAOELocation))
		{
			ConfirmedAOELocation = ResolveActorFeetLocation(AttackTarget);

		}

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

	TargetingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		GetAbility(),
		NAME_None,
		ConfiguredTargetingMontage,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		false);
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

DrawDebugDamageRadius(TEXT("ConfirmStrike"), FColor::Cyan, FColor::Blue);

	FGameplayCueParameters IndicatorParams;
	IndicatorParams.RawMagnitude = static_cast<float>(CachedAOERadius * 2.0);
	IndicatorParams.Location = ConfirmedAOELocation;
	IndicatorParams.Instigator = GetAbility()->GetAvatarActorFromActorInfo();
	IndicatorParams.EffectCauser = GetAbility()->GetAvatarActorFromActorInfo();

	const FGameplayTag ConfiguredAOEIndicatorCueTag = GetConfiguredAOEIndicatorCueTag();
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

	TArray<TEnumAsByte<EObjectTypeQuery>> ConfiguredDamageObjectTypes = GetConfiguredDamageObjectTypes();
	if (ConfiguredDamageObjectTypes.IsEmpty())
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
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : ConfiguredDamageObjectTypes)
	{
		const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ObjectType);
		if (CollisionChannel != ECC_MAX)
		{
			ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);
		}
	}

	if (!ObjectQueryParams.IsValid())
	{

		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOEDamage), false, AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(static_cast<float>(CachedAOERadius));

	DrawDebugDamageRadius(TEXT("AOEDamage"), FColor::Yellow, FColor::Red);

AOEOverlapResults.Reset();
	World->OverlapMultiByObjectType(
		AOEOverlapResults,
		ConfirmedAOELocation,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

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

	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		GetAbility(),
		NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed,
		ConfiguredTargetActorClass);
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
	const float ConfiguredLightningDamageDelay = GetConfiguredLightningDamageDelay();

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
		TraceActor->MaxRange = GetConfiguredTargetingMaxRange();
		TraceActor->TraceProfile = FCollisionProfileName(GetConfiguredTargetingTraceProfileName());
		TraceActor->bTraceAffectsAimPitch = GetConfiguredTargetingTraceAffectsAimPitch();
	}

	if (AGameplayAbilityTargetActor_GroundTrace* GroundTraceActor = Cast<AGameplayAbilityTargetActor_GroundTrace>(SpawnedActor))
	{
		GroundTraceActor->CollisionRadius = GetConfiguredTargetingCollisionRadius();
		GroundTraceActor->CollisionHeight = GetConfiguredTargetingCollisionHeight();
	}

	if (ATargetActor_GroundTrace_Decal* DecalTargetActor = Cast<ATargetActor_GroundTrace_Decal>(SpawnedActor))
	{
		const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
		UMaterialInterface* TargetingDecal = GetConfiguredTargetingDecal();
		const double TargetingDecalSize = GetConfiguredTargetingDecalSize();
		const bool bUseCharacterDecal = SkillDataAsset && SkillDataAsset->CharacterDecal.DecalMaterial;
		const bool bGrowCharacterDecal = bUseCharacterDecal && SkillDataAsset->CharacterDecal.bGrowDecalSize;

		DecalTargetActor->Decal = TargetingDecal;
		DecalTargetActor->DecalSize = TargetingDecalSize;
		DecalTargetActor->bOverrideDecalColor = false;
		DecalTargetActor->DecalColor = GetConfiguredTargetingDecalColor();

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

	bool bAppliedDamage = false;
	FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec();
	if (DamageSpecHandle.IsValid() && DamageSpecHandle.Data.IsValid())
	{
		const FActiveGameplayEffectHandle AppliedHandle =
			SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
		bAppliedDamage = AppliedHandle.WasSuccessfullyApplied();

	}

	if (bAppliedDamage)
	{
		ApplyStatusEffectToHitActor(HitActor, SourceASC, TargetASC);
	}

}

void USkillTargetedAreaAction::ApplyDirectAOECamera(bool bEnabled) const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	if (!SkillDataAsset || !Settings.bUseCameraSettings)
	{
		return;
	}

	APdPlayer* Player = Cast<APdPlayer>(GetAbility()->GetAvatarActorFromActorInfo());
	if (!Player || !Player->IsLocallyControlled())
	{

		return;
	}

	const FWeaponAimCameraSettings CameraSettings = GetConfiguredAOECameraSettings();

	Player->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

void USkillTargetedAreaAction::RemovePersistentGameplayCues()
{
	ApplyDirectAOECamera(false);

	const FGameplayTag ConfiguredAOEIndicatorCueTag = GetConfiguredAOEIndicatorCueTag();
	if (ConfiguredAOEIndicatorCueTag.IsValid())
	{
		GetAbility()->K2_RemoveGameplayCue(ConfiguredAOEIndicatorCueTag);
	}
}

FGameplayAbilityTargetingLocationInfo USkillTargetedAreaAction::MakeTargetStartLocation()
{
	const FName ConfiguredTargetingSocketName = GetConfiguredTargetingSocketName();
	if (!ConfiguredTargetingSocketName.IsNone())
	{
		return GetAbility()->MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(ConfiguredTargetingSocketName);
	}

	return GetAbility()->MakeTargetLocationInfoFromOwnerActor();
}

bool USkillTargetedAreaAction::GetTargetGroundLocation(AActor* AttackTarget, FVector& OutGroundLocation) const
{
	if (!IsValid(AttackTarget))
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AttackTarget);
	if (AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo())
	{
		ActorsToIgnore.Add(AvatarActor);
	}

	return TryResolveGroundHitLocation(
		AttackTarget->GetWorld(),
		AttackTarget->GetActorLocation(),
		ActorsToIgnore,
		GetConfiguredTargetGroundTraceChannel(),
		GetConfiguredTargetGroundTraceDepth(),
		OutGroundLocation);
}

bool USkillTargetedAreaAction::ResolveFallbackAOELocation(
	FVector& OutGroundLocation) const
{
	if (!(IsRunning() && GetAbility()->CanRunActions()))
	{
		return false;
	}

	if (AActor* AttackTarget = GetAbility()->GetAttackTargetFromAvatar();
		IsValid(AttackTarget))
	{
		if (GetTargetGroundLocation(AttackTarget, OutGroundLocation))
		{
			return true;
		}

		OutGroundLocation = ResolveActorFeetLocation(AttackTarget);
		return true;
	}

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !World)
	{
		return false;
	}

	const float MaxRange = GetConfiguredTargetingMaxRange();
	const float ForwardDistance = FMath::Clamp(
		MaxRange > 0.0f ? MaxRange * 0.65f : 800.0f,
		300.0f,
		1200.0f);
	const FVector CandidateLocation = AvatarActor->GetActorLocation()
		+ AvatarActor->GetActorForwardVector() * ForwardDistance;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);
	if (TryResolveGroundHitLocation(
		World,
		CandidateLocation,
		ActorsToIgnore,
		GetConfiguredTargetGroundTraceChannel(),
		GetConfiguredTargetGroundTraceDepth(),
		OutGroundLocation))
	{
		return true;
	}

	OutGroundLocation = CandidateLocation;
	return true;
}

FVector USkillTargetedAreaAction::ResolveConfirmedAOELocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint) const
{
	FVector ResolvedLocation = HitResult.Location.IsNearlyZero() ? TargetDataEndPoint : HitResult.Location;

	TArray<AActor*> ActorsToIgnore;
	if (AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo())
	{
		ActorsToIgnore.Add(AvatarActor);
	}

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor) || !HitActor->IsA<APawn>())
	{
		FVector GroundLocation = FVector::ZeroVector;
		UWorld* World = HitActor ? HitActor->GetWorld() : (GetAbility()->GetAvatarActorFromActorInfo() ? GetAbility()->GetAvatarActorFromActorInfo()->GetWorld() : nullptr);
		if (TryResolveGroundHitLocation(
			World,
			ResolvedLocation,
			ActorsToIgnore,
			GetConfiguredTargetGroundTraceChannel(),
			GetConfiguredTargetGroundTraceDepth(),
			GroundLocation))
		{
			return GroundLocation;
		}

		return ResolvedLocation;
	}

	ActorsToIgnore.Add(HitActor);

	FVector GroundLocation = FVector::ZeroVector;
	if (TryResolveGroundHitLocation(
		HitActor->GetWorld(),
		HitActor->GetActorLocation(),
		ActorsToIgnore,
		GetConfiguredTargetGroundTraceChannel(),
		GetConfiguredTargetGroundTraceDepth(),
		GroundLocation))
	{
		return GroundLocation;
	}

	return ResolveActorFeetLocation(HitActor);
}

bool USkillTargetedAreaAction::TryValidateServerAOELocation(
	const FHitResult& ClientHitResult,
	const FVector& TargetDataEndPoint,
	FVector& OutValidatedLocation)
{
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !AvatarActor->HasAuthority() || !World)
	{
		return false;
	}

	FVector RequestedLocation = FVector::ZeroVector;
	if (!PdTargetValidator::TryResolveTargetDataLocation(
		ClientHitResult,
		TargetDataEndPoint,
		RequestedLocation))
	{
		return false;
	}

	const FGameplayAbilityTargetingLocationInfo TargetStartLocation = MakeTargetStartLocation();
	const FVector AuthoritySourceLocation = TargetStartLocation.GetTargetingTransform().GetLocation();

	PdTargetValidator::FGroundTargetValidationParams ValidationParams;
	ValidationParams.MaxRange = GetConfiguredTargetingMaxRange();
	ValidationParams.GroundTraceDepth = GetConfiguredTargetGroundTraceDepth();
	ValidationParams.GroundTraceType = GetConfiguredTargetGroundTraceChannel();
	ValidationParams.LineOfSightProfileName = GetConfiguredTargetingTraceProfileName();

	PdTargetValidator::FValidatedGroundTarget ValidatedTarget;
	if (!PdTargetValidator::ValidateGroundTarget(
		World,
		AvatarActor,
		AuthoritySourceLocation,
		RequestedLocation,
		ValidationParams,
		ValidatedTarget))
	{
		return false;
	}

	OutValidatedLocation = ValidatedTarget.Location;
	return true;
}

FGameplayEffectSpecHandle USkillTargetedAreaAction::MakeDamageEffectSpec() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!DamageConfig.GameplayEffectClass)
	{

		return FGameplayEffectSpecHandle();
	}

	return GetAbility()->MakeConfiguredDamageEffectSpec(DamageConfig, CalculateDamageMagnitude());
}

const UStatusEffectDefinition* USkillTargetedAreaAction::GetConfiguredStatusEffectDataAsset() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> USkillTargetedAreaAction::GetConfiguredStatusEffectClass() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset
		? StatusEffectDataAsset->StackGameplayEffectClass
		: nullptr;
}

float USkillTargetedAreaAction::GetConfiguredStatusEffectLevel() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset && SkillDataAsset->StatusEffectDataAsset
		? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
		: 1.0f;
}

float USkillTargetedAreaAction::GetConfiguredStatusEffectDuration() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset ? FMath::Max(StatusEffectDataAsset->StatusDuration, 0.0f) : 0.0f;
}

FGameplayEffectSpecHandle USkillTargetedAreaAction::MakeStatusEffectSpec() const
{
	return GetAbility()->MakeConfiguredStatusEffectSpec(
		GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset()));
}

void USkillTargetedAreaAction::ApplyStatusEffectToHitActor(
	AActor* HitActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC) const
{
	if (!IsValid(HitActor) || !SourceASC || !TargetASC)
	{
		return;
	}

	GetAbility()->ApplyConfiguredStatusEffectToTarget(
		GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset()),
		TargetASC);
}



UAnimMontage* USkillTargetedAreaAction::GetConfiguredTargetingMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.SecondaryMontage.Get() : nullptr;
}

UAnimMontage* USkillTargetedAreaAction::GetConfiguredTriggerMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> USkillTargetedAreaAction::GetConfiguredDamageEffectClass() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass : nullptr;
}

TArray<TEnumAsByte<EObjectTypeQuery>> USkillTargetedAreaAction::GetConfiguredDamageObjectTypes() const
{
	return { UEngineTypes::ConvertToObjectType(ECC_Pawn) };
}

TSubclassOf<AGameplayAbilityTargetActor> USkillTargetedAreaAction::GetConfiguredTargetActorClass() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredClass =
		SkillDataAsset ? Settings.TargetActorClass : nullptr;
	if (ConfiguredClass
		&& ConfiguredClass->IsChildOf(AGameplayAbilityTargetActor_GroundTrace::StaticClass())
		&& !ConfiguredClass->IsChildOf(ATargetActor_GroundTrace_Decal::StaticClass()))
	{
		return ATargetActor_GroundTrace_Decal::StaticClass();
	}

	return ConfiguredClass;
}

UMaterialInterface* USkillTargetedAreaAction::GetConfiguredTargetingDecal() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
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
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
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

FLinearColor USkillTargetedAreaAction::GetConfiguredTargetingDecalColor() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.TargetingDecalColor : FLinearColor::White;
}

FName USkillTargetedAreaAction::GetConfiguredTargetingTraceProfileName() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.TargetingTraceProfileName : NAME_None;
}

float USkillTargetedAreaAction::GetConfiguredTargetingMaxRange() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(Settings.TargetingMaxRange) : 0.0f;
}

float USkillTargetedAreaAction::GetConfiguredTargetingCollisionRadius() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(Settings.TargetingCollisionRadius) : 0.0f;
}

float USkillTargetedAreaAction::GetConfiguredTargetingCollisionHeight() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(Settings.TargetingCollisionHeight) : 0.0f;
}

bool USkillTargetedAreaAction::GetConfiguredTargetingTraceAffectsAimPitch() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset && Settings.bTargetingTraceAffectsAimPitch;
}

bool USkillTargetedAreaAction::GetConfiguredDebugTargeting() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& SkillDataAsset
		&& Settings.bDebugTargeting;
}

bool USkillTargetedAreaAction::GetConfiguredDrawDebugDamageRadius() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& SkillDataAsset
		&& Settings.bDrawDebugDamageRadius;
}

float USkillTargetedAreaAction::GetConfiguredDebugDamageRadiusDrawTime() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(FMath::Max(Settings.DebugDamageRadiusDrawTime, 0.0)) : 0.0f;
}

FName USkillTargetedAreaAction::GetConfiguredTargetingSocketName() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.TargetingSocketName : NAME_None;
}

TEnumAsByte<ETraceTypeQuery> USkillTargetedAreaAction::GetConfiguredTargetGroundTraceChannel() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset
		? Settings.TargetGroundTraceChannel
		: TEnumAsByte<ETraceTypeQuery>(LabCollisionChannels::VisibilityTrace());
}

float USkillTargetedAreaAction::GetConfiguredTargetGroundTraceDepth() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(Settings.TargetGroundTraceDepth) : 0.0f;
}

FGameplayTag USkillTargetedAreaAction::GetConfiguredDamageDataTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().MagnitudeDataTag : FGameplayTag();
}

FGameplayTag USkillTargetedAreaAction::GetConfiguredMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryEventTag : FGameplayTag();
}

FGameplayTag USkillTargetedAreaAction::GetConfiguredAOEIndicatorCueTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.IndicatorCueTag : FGameplayTag();
}

FGameplayTag USkillTargetedAreaAction::GetConfiguredLightningBoltCueTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.ImpactCueTag : FGameplayTag();
}

float USkillTargetedAreaAction::GetConfiguredLightningDamageDelay() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(FMath::Max(Settings.DamageDelay, 0.0)) : 0.0f;
}

FWeaponAimCameraSettings USkillTargetedAreaAction::GetConfiguredAOECameraSettings() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset && Settings.bUseCameraSettings ? Settings.CameraSettings : FWeaponAimCameraSettings();
}

double USkillTargetedAreaAction::CalculateAOERadiusFromSkillData() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset ? Settings.Radius : 0.0;
}

float USkillTargetedAreaAction::CalculateDamageMagnitude() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetAbility()->GetSourceSkillDataAsset());
	return SkillDataAsset
		? GetAbility()->CalculateSkillDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig())
		: 0.0f;
}

bool USkillTargetedAreaAction::ShouldDrawDebugDamageRadius() const
{
	return GetConfiguredDrawDebugDamageRadius();
}

void USkillTargetedAreaAction::DrawDebugDamageRadius(const TCHAR* Context, const FColor& CircleColor, const FColor& SphereColor) const
{
	if (!ShouldDrawDebugDamageRadius())
	{
		return;
	}

	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const float Radius = static_cast<float>(CachedAOERadius);
	const float DrawTime = FMath::Max(GetConfiguredDebugDamageRadiusDrawTime(), 0.0f);
	const FVector Center = ConfirmedAOELocation;
	const uint8 DepthPriority = 1;
	constexpr int32 CircleSegments = 128;
	constexpr int32 HemisphereSegments = 24;
	constexpr int32 HemisphereMeridians = 8;
	constexpr int32 LatitudeRings = 4;
	constexpr float LineThickness = 2.0f;

	DrawDebugCircle(
		World,
		Center,
		Radius,
		CircleSegments,
		CircleColor,
		false,
		DrawTime,
		DepthPriority,
		4.0f,
		FVector::ForwardVector,
		FVector::RightVector,
		false);

	for (int32 RingIndex = 1; RingIndex <= LatitudeRings; ++RingIndex)
	{
		const float Alpha = static_cast<float>(RingIndex) / static_cast<float>(LatitudeRings + 1);
		const float Angle = Alpha * HALF_PI;
		const float RingRadius = FMath::Cos(Angle) * Radius;
		const float RingHeight = FMath::Sin(Angle) * Radius;

		DrawDebugCircle(
			World,
			Center + FVector(0.0, 0.0, RingHeight),
			RingRadius,
			CircleSegments,
			SphereColor,
			false,
			DrawTime,
			DepthPriority,
			LineThickness,
			FVector::ForwardVector,
			FVector::RightVector,
			false);
	}

	for (int32 MeridianIndex = 0; MeridianIndex < HemisphereMeridians; ++MeridianIndex)
	{
		const float Azimuth = (static_cast<float>(MeridianIndex) / static_cast<float>(HemisphereMeridians)) * TWO_PI;
		const FVector HorizontalDirection(
			FMath::Cos(Azimuth),
			FMath::Sin(Azimuth),
			0.0f);

		FVector PreviousPoint = Center + HorizontalDirection * Radius;
		for (int32 SegmentIndex = 1; SegmentIndex <= HemisphereSegments; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(HemisphereSegments);
			const float Angle = Alpha * HALF_PI;
			const FVector CurrentPoint = Center
				+ HorizontalDirection * (FMath::Cos(Angle) * Radius)
				+ FVector(0.0, 0.0, FMath::Sin(Angle) * Radius);

			DrawDebugLine(
				World,
				PreviousPoint,
				CurrentPoint,
				SphereColor,
				false,
				DrawTime,
				DepthPriority,
				LineThickness);

			PreviousPoint = CurrentPoint;
		}
	}

}

void USkillTargetedAreaAction::HandleCancelInputPressed(float TimeWaited)
{
	static_cast<void>(TimeWaited);
	if (UPdAbilitySystemComponent* AbilitySystemComponent =
		GetAbility()->GetPdAbilitySystemComponentFromActorInfo())
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
		ConfirmedAOELocation = ResolveConfirmedAOELocation(*ClientHitResult, TargetDataEndPoint);
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

	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());
	FGameplayCueParameters LightningCueParams;
	LightningCueParams.Location = ConfirmedAOELocation;
	LightningCueParams.Instigator = GetAbility()->GetAvatarActorFromActorInfo();
	LightningCueParams.EffectCauser = GetAbility()->GetAvatarActorFromActorInfo();

	const FGameplayTag ResolvedLightningBoltCueTag = GetConfiguredLightningBoltCueTag();
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
