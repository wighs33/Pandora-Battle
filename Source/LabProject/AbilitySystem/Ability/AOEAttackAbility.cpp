#include "AbilitySystem/Ability/AOEAttackAbility.h"

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
#include "Definition/AbilitySystem/SkillTypes.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(AOEAttackAbility)

namespace
{
	constexpr float AOEGroundProjectionStartHeight = 500.0f;
	constexpr float AOEGroundProjectionMinDepth = 1000.0f;
	constexpr float AOEGroundMinNormalZ = 0.35f;

	const USkillDefinition* GetAOESkillDataAsset(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == ESkillDataType::Area
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

UAOEAttackAbility::UAOEAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_AOEAttack);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_AOEAttack_Active);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility_ShootProjectile);
}

void UAOEAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (SkillDataAsset->SkillDataType != ESkillDataType::Area)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredTargetActorClass = GetConfiguredTargetActorClass();
	const double ConfiguredAOERadius = CalculateAOERadiusFromSkillData();
	if (!ConfiguredTargetActorClass || ConfiguredAOERadius <= 0.0)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
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

	if (AActor* AttackTarget = GetAttackTargetFromAvatar(); IsValid(AttackTarget))
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

void UAOEAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	RestoreAvatarMovementForAbility();
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

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAOEAttackAbility::StartTargeting()
{
	if (!HasPlayerController())
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			K2_CancelAbility();
		}
		return;
	}

	bIsWaitingTargetData = true;
	if (const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
		SkillDataAsset && SkillDataAsset->Movement.bLockMovementDuringDuration)
	{
		LockAvatarMovementForAbility();
	}

	ApplyDirectAOECamera(true);

	LoopTargetingAnimation();
	StartWaitTargetData();
}

void UAOEAttackAbility::LoopTargetingAnimation()
{
	UAnimMontage* ConfiguredTargetingMontage = GetConfiguredTargetingMontage();
	if (!bIsWaitingTargetData || !ConfiguredTargetingMontage)
	{
		return;
	}

	TargetingMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
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

void UAOEAttackAbility::ConfirmStrike()
{
	if (bStrikeConfirmed)
	{
		return;
	}

	bIsWaitingTargetData = false;
	RestoreAvatarMovementForAbility();

	if (ConfirmedAOELocation.IsNearlyZero())
	{
		if (!ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			K2_CancelAbility();
			return;
		}
	}
	bStrikeConfirmed = true;

DrawDebugDamageRadius(TEXT("ConfirmStrike"), FColor::Cyan, FColor::Blue);

	FGameplayCueParameters IndicatorParams;
	IndicatorParams.RawMagnitude = static_cast<float>(CachedAOERadius * 2.0);
	IndicatorParams.Location = ConfirmedAOELocation;
	IndicatorParams.Instigator = GetAvatarActorFromActorInfo();
	IndicatorParams.EffectCauser = GetAvatarActorFromActorInfo();

	const FGameplayTag ConfiguredAOEIndicatorCueTag = GetConfiguredAOEIndicatorCueTag();
	if (ConfiguredAOEIndicatorCueTag.IsValid())
	{
		K2_AddGameplayCueWithParams(ConfiguredAOEIndicatorCueTag, IndicatorParams, true);
	}

	StartWaitMontageTrigger();

	UAnimMontage* ConfiguredTriggerMontage = GetConfiguredTriggerMontage();
	if (!ConfiguredTriggerMontage)
	{

		HandleMontageTriggerEvent(FGameplayEventData());
		return;
	}

	TriggerMontageTask = CreateDefaultMontageAndWaitTask(ConfiguredTriggerMontage);
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

void UAOEAttackAbility::AOEDamage()
{
	if (!K2_HasAuthority())
	{
		return;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ConfiguredDamageObjectTypes = GetConfiguredDamageObjectTypes();
	if (ConfiguredDamageObjectTypes.IsEmpty())
	{

		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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

void UAOEAttackAbility::WaitCancelInput()
{
	WaitCancelInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (!WaitCancelInputTask)
	{
		return;
	}

	WaitCancelInputTask->OnPress.AddDynamic(this, &ThisClass::HandleCancelInputPressed);
	WaitCancelInputTask->ReadyForActivation();
}

void UAOEAttackAbility::StartWaitTargetData()
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
			K2_CancelAbility();
		}
		return;
	}

	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
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
			K2_CancelAbility();
		}
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	if (AGameplayAbilityTargetActor* SpawnedTargetActor =
		BeginSpawningTargetDataActor(WaitTargetDataTask, ConfiguredTargetActorClass))
	{
		ConfigureSpawnedTargetActor(SpawnedTargetActor);
		FinishSpawningTargetDataActor(WaitTargetDataTask, SpawnedTargetActor);
	}

	WaitTargetDataTask->ReadyForActivation();
}

void UAOEAttackAbility::StartWaitMontageTrigger()
{
	const FGameplayTag ConfiguredMontageTriggerEventTag = GetConfiguredMontageTriggerEventTag();
	if (!ConfiguredMontageTriggerEventTag.IsValid())
	{

		return;
	}

WaitMontageTriggerTask = CreateWaitGameplayEventTask(ConfiguredMontageTriggerEventTag, true);
	if (!WaitMontageTriggerTask)
	{

		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

void UAOEAttackAbility::StartLightningDamageDelay()
{
	bWaitingLightningDamage = true;
	const float ConfiguredLightningDamageDelay = GetConfiguredLightningDamageDelay();

	if (ConfiguredLightningDamageDelay <= 0.0f)
	{
		HandleLightningDamageDelayFinished();
		return;
	}

	LightningDamageDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ConfiguredLightningDamageDelay);
	if (!LightningDamageDelayTask)
	{

		HandleLightningDamageDelayFinished();
		return;
	}

	LightningDamageDelayTask->OnFinish.AddDynamic(this, &ThisClass::HandleLightningDamageDelayFinished);
	LightningDamageDelayTask->ReadyForActivation();
}

void UAOEAttackAbility::ConfigureSpawnedTargetActor(AGameplayAbilityTargetActor* SpawnedActor)
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
		const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
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
			DecalTargetActor->ConfigureDecalGrowth(StartSize, FinalSize, ResolveConfiguredCharacterDecalDuration(SkillDataAsset));
		}
	}
}

void UAOEAttackAbility::ApplyEffectToHitActor(AActor* HitActor)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (SourceCharacter && TargetCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{

		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo());
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

void UAOEAttackAbility::ApplyDirectAOECamera(bool bEnabled) const
{
	APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	if (!Player || !Player->IsLocallyControlled())
	{

		return;
	}

	const FWeaponAimCameraSettings CameraSettings = GetConfiguredAOECameraSettings();

Player->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

void UAOEAttackAbility::RemovePersistentGameplayCues()
{
	ApplyDirectAOECamera(false);

	const FGameplayTag ConfiguredAOEIndicatorCueTag = GetConfiguredAOEIndicatorCueTag();
	if (ConfiguredAOEIndicatorCueTag.IsValid())
	{
		K2_RemoveGameplayCue(ConfiguredAOEIndicatorCueTag);
	}
}

FGameplayAbilityTargetingLocationInfo UAOEAttackAbility::MakeTargetStartLocation()
{
	const FName ConfiguredTargetingSocketName = GetConfiguredTargetingSocketName();
	if (!ConfiguredTargetingSocketName.IsNone())
	{
		return MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(ConfiguredTargetingSocketName);
	}

	return MakeTargetLocationInfoFromOwnerActor();
}

bool UAOEAttackAbility::GetTargetGroundLocation(AActor* AttackTarget, FVector& OutGroundLocation) const
{
	if (!IsValid(AttackTarget))
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AttackTarget);
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
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

bool UAOEAttackAbility::ResolveFallbackAOELocation(
	FVector& OutGroundLocation) const
{
	if (!CanExecuteSkillPayload())
	{
		return false;
	}

	if (AActor* AttackTarget = GetAttackTargetFromAvatar();
		IsValid(AttackTarget))
	{
		if (GetTargetGroundLocation(AttackTarget, OutGroundLocation))
		{
			return true;
		}

		OutGroundLocation = ResolveActorFeetLocation(AttackTarget);
		return true;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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

FVector UAOEAttackAbility::ResolveConfirmedAOELocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint) const
{
	FVector ResolvedLocation = HitResult.Location.IsNearlyZero() ? TargetDataEndPoint : HitResult.Location;

	TArray<AActor*> ActorsToIgnore;
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		ActorsToIgnore.Add(AvatarActor);
	}

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor) || !HitActor->IsA<APawn>())
	{
		FVector GroundLocation = FVector::ZeroVector;
		UWorld* World = HitActor ? HitActor->GetWorld() : (GetAvatarActorFromActorInfo() ? GetAvatarActorFromActorInfo()->GetWorld() : nullptr);
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

bool UAOEAttackAbility::TryValidateServerAOELocation(
	const FHitResult& ClientHitResult,
	const FVector& TargetDataEndPoint,
	FVector& OutValidatedLocation)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
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

FGameplayEffectSpecHandle UAOEAttackAbility::MakeDamageEffectSpec() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!DamageConfig.GameplayEffectClass)
	{

		return FGameplayEffectSpecHandle();
	}

	return MakeConfiguredDamageEffectSpec(DamageConfig, CalculateDamageMagnitude());
}

const UStatusEffectDefinition* UAOEAttackAbility::GetConfiguredStatusEffectDataAsset() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> UAOEAttackAbility::GetConfiguredStatusEffectClass() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset
		? StatusEffectDataAsset->DebuffGameplayEffectClass
		: nullptr;
}

float UAOEAttackAbility::GetConfiguredStatusEffectLevel() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset && SkillDataAsset->StatusEffectDataAsset
		? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
		: 1.0f;
}

float UAOEAttackAbility::GetConfiguredStatusEffectDuration() const
{
	const UStatusEffectDefinition* StatusEffectDataAsset = GetConfiguredStatusEffectDataAsset();
	return StatusEffectDataAsset ? FMath::Max(StatusEffectDataAsset->StatusDuration, 0.0f) : 0.0f;
}

FGameplayEffectSpecHandle UAOEAttackAbility::MakeStatusEffectSpec() const
{
	return MakeConfiguredStatusEffectSpec(
		GetAOESkillDataAsset(GetSourceSkillDataAsset()));
}

void UAOEAttackAbility::ApplyStatusEffectToHitActor(
	AActor* HitActor,
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC) const
{
	if (!IsValid(HitActor) || !SourceASC || !TargetASC)
	{
		return;
	}

	ApplyConfiguredStatusEffectToTarget(
		GetAOESkillDataAsset(GetSourceSkillDataAsset()),
		TargetASC);
}

bool UAOEAttackAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	return AvatarPawn && Cast<APlayerController>(AvatarPawn->GetController());
}

UAnimMontage* UAOEAttackAbility::GetConfiguredTargetingMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.SecondaryMontage.Get() : nullptr;
}

UAnimMontage* UAOEAttackAbility::GetConfiguredTriggerMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> UAOEAttackAbility::GetConfiguredDamageEffectClass() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass : nullptr;
}

TArray<TEnumAsByte<EObjectTypeQuery>> UAOEAttackAbility::GetConfiguredDamageObjectTypes() const
{
	return { UEngineTypes::ConvertToObjectType(ECC_Pawn) };
}

TSubclassOf<AGameplayAbilityTargetActor> UAOEAttackAbility::GetConfiguredTargetActorClass() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	const TSubclassOf<AGameplayAbilityTargetActor> ConfiguredClass =
		SkillDataAsset ? SkillDataAsset->AOETargetActorClass : nullptr;
	if (ConfiguredClass
		&& ConfiguredClass->IsChildOf(AGameplayAbilityTargetActor_GroundTrace::StaticClass())
		&& !ConfiguredClass->IsChildOf(ATargetActor_GroundTrace_Decal::StaticClass()))
	{
		return ATargetActor_GroundTrace_Decal::StaticClass();
	}

	return ConfiguredClass;
}

UMaterialInterface* UAOEAttackAbility::GetConfiguredTargetingDecal() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	if (!SkillDataAsset)
	{
		return nullptr;
	}

	if (SkillDataAsset->CharacterDecal.DecalMaterial)
	{
		return SkillDataAsset->CharacterDecal.DecalMaterial.Get();
	}

	return SkillDataAsset->AOETargetingDecal.Get();
}

double UAOEAttackAbility::GetConfiguredTargetingDecalSize() const
{
	const double FallbackSize = CachedAOERadius > 0.0 ? CachedAOERadius * 2.0 : 512.0;
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
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

FLinearColor UAOEAttackAbility::GetConfiguredTargetingDecalColor() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOETargetingDecalColor : FLinearColor::White;
}

FName UAOEAttackAbility::GetConfiguredTargetingTraceProfileName() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOETargetingTraceProfileName : NAME_None;
}

float UAOEAttackAbility::GetConfiguredTargetingMaxRange() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingMaxRange) : 0.0f;
}

float UAOEAttackAbility::GetConfiguredTargetingCollisionRadius() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingCollisionRadius) : 0.0f;
}

float UAOEAttackAbility::GetConfiguredTargetingCollisionHeight() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingCollisionHeight) : 0.0f;
}

bool UAOEAttackAbility::GetConfiguredTargetingTraceAffectsAimPitch() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset && SkillDataAsset->bAOETargetingTraceAffectsAimPitch;
}

bool UAOEAttackAbility::GetConfiguredDebugTargeting() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& SkillDataAsset
		&& SkillDataAsset->bAOEDebugTargeting;
}

bool UAOEAttackAbility::GetConfiguredDrawDebugDamageRadius() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return LabSkillDebug::IsDrawingEnabled()
		&& SkillDataAsset
		&& SkillDataAsset->bAOEDrawDebugDamageRadius;
}

float UAOEAttackAbility::GetConfiguredDebugDamageRadiusDrawTime() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->AOEDebugDamageRadiusDrawTime, 0.0)) : 0.0f;
}

FName UAOEAttackAbility::GetConfiguredTargetingSocketName() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOETargetingSocketName : NAME_None;
}

TEnumAsByte<ETraceTypeQuery> UAOEAttackAbility::GetConfiguredTargetGroundTraceChannel() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset
		? SkillDataAsset->AOETargetGroundTraceChannel
		: TEnumAsByte<ETraceTypeQuery>(LabCollisionChannels::VisibilityTrace());
}

float UAOEAttackAbility::GetConfiguredTargetGroundTraceDepth() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetGroundTraceDepth) : 0.0f;
}

FGameplayTag UAOEAttackAbility::GetConfiguredDamageDataTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig().MagnitudeDataTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredMontageTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryEventTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredAOEIndicatorCueTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOEIndicatorCueTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredLightningBoltCueTag() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOELightningBoltCueTag : FGameplayTag();
}

float UAOEAttackAbility::GetConfiguredLightningDamageDelay() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->AOELightningDamageDelay, 0.0)) : 0.0f;
}

FWeaponAimCameraSettings UAOEAttackAbility::GetConfiguredAOECameraSettings() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset && SkillDataAsset->bUseAOECameraSettings ? SkillDataAsset->AOECameraSettings : FWeaponAimCameraSettings();
}

double UAOEAttackAbility::CalculateAOERadiusFromSkillData() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset ? SkillDataAsset->AOERadius : 0.0;
}

float UAOEAttackAbility::CalculateDamageMagnitude() const
{
	const USkillDefinition* SkillDataAsset = GetAOESkillDataAsset(GetSourceSkillDataAsset());
	return SkillDataAsset
		? CalculateSkillDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig())
		: 0.0f;
}

bool UAOEAttackAbility::ShouldDrawDebugDamageRadius() const
{
	return GetConfiguredDrawDebugDamageRadius();
}

void UAOEAttackAbility::DrawDebugDamageRadius(const TCHAR* Context, const FColor& CircleColor, const FColor& SphereColor) const
{
	if (!ShouldDrawDebugDamageRadius())
	{
		return;
	}

	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
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

void UAOEAttackAbility::HandleCancelInputPressed(float TimeWaited)
{
	static_cast<void>(TimeWaited);
	if (UPdAbilitySystemComponent* AbilitySystemComponent =
		GetPdAbilitySystemComponentFromActorInfo())
	{
		AbilitySystemComponent->LocalInputConfirm();
	}
	else if (ResolveFallbackAOELocation(ConfirmedAOELocation))
	{
		ConfirmStrike();
	}
}

void UAOEAttackAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	bIsWaitingTargetData = false;

	const FGameplayAbilityTargetData* TargetData = Data.Get(0);
	const FHitResult* ClientHitResult = TargetData ? TargetData->GetHitResult() : nullptr;
	if (!CurrentActorInfo || !ClientHitResult)
	{
		if (ResolveFallbackAOELocation(ConfirmedAOELocation))
		{
			ConfirmStrike();
		}
		else
		{
			K2_CancelAbility();
		}
		return;
	}

	const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	if (CurrentActorInfo->IsNetAuthority())
	{
		if (!TryValidateServerAOELocation(*ClientHitResult, TargetDataEndPoint, ConfirmedAOELocation))
		{
			if (ResolveFallbackAOELocation(ConfirmedAOELocation))
			{
				ConfirmStrike();
			}
			else
			{
				K2_CancelAbility();
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

void UAOEAttackAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	if (ResolveFallbackAOELocation(ConfirmedAOELocation))
	{
		ConfirmStrike();
	}
	else
	{
		K2_CancelAbility();
	}
}

void UAOEAttackAbility::HandleTargetingMontageBlendOut()
{
	TargetingMontageTask = nullptr;
	if (bIsWaitingTargetData)
	{
		LoopTargetingAnimation();
	}
}

void UAOEAttackAbility::HandleTargetingMontageInterrupted()
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
			K2_CancelAbility();
		}
	}
}

void UAOEAttackAbility::HandleTriggerMontageFinished()
{
	TriggerMontageTask = nullptr;

	if (!bStrikeTriggered)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
	}
}

void UAOEAttackAbility::HandleTriggerMontageInterrupted()
{
	TriggerMontageTask = nullptr;
	if (!bStrikeTriggered)
	{
		HandleMontageTriggerEvent(FGameplayEventData());
	}
}

void UAOEAttackAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{

	if (bStrikeTriggered)
	{
		return;
	}

	bStrikeTriggered = true;

	if (!K2_CommitAbility())
	{

		K2_EndAbility();
		return;
	}

	SpawnConfiguredCharacterDecal();
	FGameplayCueParameters LightningCueParams;
	LightningCueParams.Location = ConfirmedAOELocation;
	LightningCueParams.Instigator = GetAvatarActorFromActorInfo();
	LightningCueParams.EffectCauser = GetAvatarActorFromActorInfo();

	const FGameplayTag ResolvedLightningBoltCueTag = GetConfiguredLightningBoltCueTag();
	if (ResolvedLightningBoltCueTag.IsValid())
	{
		K2_ExecuteGameplayCueWithParams(ResolvedLightningBoltCueTag, LightningCueParams);
	}

	StartLightningDamageDelay();
}

void UAOEAttackAbility::HandleLightningDamageDelayFinished()
{
	const bool bHasAuthority = K2_HasAuthority();
	LightningDamageDelayTask = nullptr;
	bWaitingLightningDamage = false;

	if (!bHasAuthority)
	{
		return;
	}

	AOEDamage();
	RemovePersistentGameplayCues();
	K2_EndAbility();
}
