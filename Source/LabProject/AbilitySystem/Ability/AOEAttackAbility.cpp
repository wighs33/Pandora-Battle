#include "AbilitySystem/Ability/AOEAttackAbility.h"

#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "Abilities/GameplayAbilityTargetActor_Trace.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AOEAttackAbility)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraAOEAttackAbility, Log, All);

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

	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("AOE ability cancelled: missing SkillDataAsset. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ActorInfo->AvatarActor.Get()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!SkillDataAsset->AOETargetActorClass || SkillDataAsset->AOERadius <= 0.0)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("AOE ability cancelled: invalid SkillDataAsset targeting values. skill=%s targetActor=%s radius=%.2f"),
			*GetNameSafe(SkillDataAsset),
			*GetNameSafe(SkillDataAsset->AOETargetActorClass.Get()),
			SkillDataAsset->AOERadius);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bStrikeTriggered = false;
	bWaitingLightningDamage = false;
	bIsWaitingTargetData = false;
	ConfirmedAOELocation = FVector::ZeroVector;
	HitActors.Reset();
	CachedAOERadius = CalculateAOERadiusFromSkillData();

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("Activate: ability=%s avatar=%s authority=%s localController=%s radius=%.1f damage=%.1f targetActor=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ActorInfo->AvatarActor.Get()),
		ActorInfo->AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		HasPlayerController() ? TEXT("true") : TEXT("false"),
		CachedAOERadius,
		CalculateDamageMagnitude(),
		*GetNameSafe(GetConfiguredTargetActorClass().Get()));

	if (AActor* AttackTarget = GetAttackTargetFromAvatar(); IsValid(AttackTarget))
	{
		if (!GetTargetGroundLocation(AttackTarget, ConfirmedAOELocation))
		{
			ConfirmedAOELocation = AttackTarget->GetActorLocation();
			UE_LOG(LogPandoraAOEAttackAbility, Warning,
				TEXT("Activate using attack target fallback location: ability=%s target=%s location=%s"),
				*GetNameSafe(this),
				*GetNameSafe(AttackTarget),
				*ConfirmedAOELocation.ToCompactString());
		}

		UE_LOG(LogPandoraAOEAttackAbility, Log,
			TEXT("Activate using attack target: ability=%s target=%s confirmedLocation=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AttackTarget),
			*ConfirmedAOELocation.ToCompactString());
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
	bIsWaitingTargetData = false;
	bWaitingLightningDamage = false;
	HitActors.Reset();
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
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("StartTargeting failed: avatar has no player controller. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetAvatarActorFromActorInfo()));
		K2_EndAbility();
		return;
	}

	bIsWaitingTargetData = true;

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
	bIsWaitingTargetData = false;

	if (ConfirmedAOELocation.IsNearlyZero())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("ConfirmStrike failed: confirmed location is zero. ability=%s"),
			*GetNameSafe(this));
		K2_EndAbility();
		return;
	}

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("ConfirmStrike: ability=%s location=%s radius=%.1f triggerMontage=%s montageTriggerTag=%s"),
		*GetNameSafe(this),
		*ConfirmedAOELocation.ToCompactString(),
		CachedAOERadius,
		*GetNameSafe(GetConfiguredTriggerMontage()),
		*GetConfiguredMontageTriggerEventTag().ToString());

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
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("ConfirmStrike without trigger montage: triggering AOE immediately. ability=%s"),
			*GetNameSafe(this));
		HandleMontageTriggerEvent(FGameplayEventData());
		return;
	}

	TriggerMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		ConfiguredTriggerMontage,
		1.0f,
		NAME_None,
		true,
		1.0f,
		0.0f,
		true);
	if (!TriggerMontageTask)
	{
		K2_EndAbility();
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
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("AOEDamage skipped: SkillDataAsset has no damage object types. ability=%s"),
			*GetNameSafe(this));
		return;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("AOEDamage failed: invalid world. ability=%s avatar=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AvatarActor));
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : ConfiguredDamageObjectTypes)
	{
		const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(ObjectType);
		if (CollisionChannel != ECC_OverlapAll_Deprecated)
		{
			ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);
		}
	}

	if (!ObjectQueryParams.IsValid())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("AOEDamage skipped: no valid collision channels from SkillDataAsset object types. ability=%s"),
			*GetNameSafe(this));
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOEDamage), false, AvatarActor);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(static_cast<float>(CachedAOERadius));

	DrawDebugDamageRadius(TEXT("AOEDamage"), FColor::Yellow, FColor::Red);

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("AOEDamage begin: ability=%s avatar=%s location=%s radius=%.1f damage=%.1f damageEffect=%s objectTypes=%d"),
		*GetNameSafe(this),
		*GetNameSafe(AvatarActor),
		*ConfirmedAOELocation.ToCompactString(),
		CachedAOERadius,
		CalculateDamageMagnitude(),
		*GetNameSafe(GetConfiguredDamageEffectClass().Get()),
		ConfiguredDamageObjectTypes.Num());

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ConfirmedAOELocation,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

	HitActors.Reset();
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* HitActor = OverlapResult.GetActor();
		UPrimitiveComponent* HitComponent = OverlapResult.GetComponent();
		const FVector HitActorLocation = IsValid(HitActor) ? HitActor->GetActorLocation() : FVector::ZeroVector;
		const double Distance3D = IsValid(HitActor) ? FVector::Dist(ConfirmedAOELocation, HitActorLocation) : 0.0;
		const double Distance2D = IsValid(HitActor) ? FVector::Dist2D(ConfirmedAOELocation, HitActorLocation) : 0.0;

		UE_LOG(LogPandoraAOEAttackAbility, Log,
			TEXT("AOEDamage candidate: actor=%s component=%s actorLocation=%s dist2D=%.1f dist3D=%.1f objectType=%d self=%s duplicate=%s"),
			*GetNameSafe(HitActor),
			*GetNameSafe(HitComponent),
			*HitActorLocation.ToCompactString(),
			Distance2D,
			Distance3D,
			HitComponent ? static_cast<int32>(HitComponent->GetCollisionObjectType()) : INDEX_NONE,
			HitActor == AvatarActor ? TEXT("true") : TEXT("false"),
			HitActors.Contains(HitActor) ? TEXT("true") : TEXT("false"));

		if (!IsValid(HitActor) || HitActor == AvatarActor || HitActors.Contains(HitActor))
		{
			continue;
		}

		HitActors.Add(HitActor);
		ApplyEffectToHitActor(HitActor);
	}

	for (TActorIterator<APawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		APawn* Pawn = *PawnIt;
		if (!IsValid(Pawn) || Pawn == AvatarActor)
		{
			continue;
		}

		bool bReturnedByOverlap = false;
		FString ReturnedComponents;
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			if (OverlapResult.GetActor() != Pawn)
			{
				continue;
			}

			bReturnedByOverlap = true;
			if (!ReturnedComponents.IsEmpty())
			{
				ReturnedComponents += TEXT(",");
			}
			ReturnedComponents += GetNameSafe(OverlapResult.GetComponent());
		}

		UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
		const FVector PawnLocation = Pawn->GetActorLocation();
		const double Distance2D = FVector::Dist2D(ConfirmedAOELocation, PawnLocation);
		const double Distance3D = FVector::Dist(ConfirmedAOELocation, PawnLocation);
		const bool bWithinRadius2D = Distance2D <= CachedAOERadius;

		UE_LOG(LogPandoraAOEAttackAbility, Log,
			TEXT("AOEDamage pawn audit: pawn=%s location=%s dist2D=%.1f dist3D=%.1f radius=%.1f withinRadius2D=%s returnedByOverlap=%s returnedComponents=%s rootComponent=%s rootCollision=%d rootObjectType=%d rootOverlapEvents=%s"),
			*GetNameSafe(Pawn),
			*PawnLocation.ToCompactString(),
			Distance2D,
			Distance3D,
			CachedAOERadius,
			bWithinRadius2D ? TEXT("true") : TEXT("false"),
			bReturnedByOverlap ? TEXT("true") : TEXT("false"),
			ReturnedComponents.IsEmpty() ? TEXT("none") : *ReturnedComponents,
			*GetNameSafe(RootPrimitive),
			RootPrimitive ? static_cast<int32>(RootPrimitive->GetCollisionEnabled()) : INDEX_NONE,
			RootPrimitive ? static_cast<int32>(RootPrimitive->GetCollisionObjectType()) : INDEX_NONE,
			RootPrimitive && RootPrimitive->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"));

		if (bWithinRadius2D && !bReturnedByOverlap)
		{
			UE_LOG(LogPandoraAOEAttackAbility, Warning,
				TEXT("AOEDamage pawn inside radius but missing from overlap: pawn=%s location=%s center=%s dist2D=%.1f radius=%.1f"),
				*GetNameSafe(Pawn),
				*PawnLocation.ToCompactString(),
				*ConfirmedAOELocation.ToCompactString(),
				Distance2D,
				CachedAOERadius);
		}
	}

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("AOEDamage end: ability=%s location=%s radius=%.1f overlaps=%d uniqueHits=%d"),
		*GetNameSafe(this),
		*ConfirmedAOELocation.ToCompactString(),
		CachedAOERadius,
		OverlapResults.Num(),
		HitActors.Num());
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
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("StartWaitTargetData failed: TargetActorClass is null. ability=%s"),
			*GetNameSafe(this));
		K2_EndAbility();
		return;
	}

	WaitTargetDataTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed,
		ConfiguredTargetActorClass);
	if (!WaitTargetDataTask)
	{
		K2_EndAbility();
		return;
	}

	WaitTargetDataTask->ValidData.AddDynamic(this, &ThisClass::HandleTargetDataValid);
	WaitTargetDataTask->Cancelled.AddDynamic(this, &ThisClass::HandleTargetDataCancelled);

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (WaitTargetDataTask->BeginSpawningActor(this, ConfiguredTargetActorClass, SpawnedTargetActor))
	{
		ConfigureSpawnedTargetActor(SpawnedTargetActor);
		WaitTargetDataTask->FinishSpawningActor(this, SpawnedTargetActor);
	}

	WaitTargetDataTask->ReadyForActivation();
}

void UAOEAttackAbility::StartWaitMontageTrigger()
{
	const FGameplayTag ConfiguredMontageTriggerEventTag = GetConfiguredMontageTriggerEventTag();
	if (!ConfiguredMontageTriggerEventTag.IsValid())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("StartWaitMontageTrigger skipped: MontageTriggerEventTag is invalid. ability=%s"),
			*GetNameSafe(this));
		return;
	}

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("StartWaitMontageTrigger: ability=%s tag=%s"),
		*GetNameSafe(this),
		*ConfiguredMontageTriggerEventTag.ToString());

	WaitMontageTriggerTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		ConfiguredMontageTriggerEventTag,
		nullptr,
		true,
		true);
	if (!WaitMontageTriggerTask)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("StartWaitMontageTrigger failed: task is null. ability=%s tag=%s"),
			*GetNameSafe(this),
			*ConfiguredMontageTriggerEventTag.ToString());
		return;
	}

	WaitMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleMontageTriggerEvent);
	WaitMontageTriggerTask->ReadyForActivation();
}

void UAOEAttackAbility::StartLightningDamageDelay()
{
	bWaitingLightningDamage = true;
	const float ConfiguredLightningDamageDelay = GetConfiguredLightningDamageDelay();
	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("StartLightningDamageDelay: ability=%s authority=%s delay=%.3f location=%s"),
		*GetNameSafe(this),
		K2_HasAuthority() ? TEXT("true") : TEXT("false"),
		ConfiguredLightningDamageDelay,
		*ConfirmedAOELocation.ToCompactString());

	if (ConfiguredLightningDamageDelay <= 0.0f)
	{
		HandleLightningDamageDelayFinished();
		return;
	}

	LightningDamageDelayTask = UAbilityTask_WaitDelay::WaitDelay(this, ConfiguredLightningDamageDelay);
	if (!LightningDamageDelayTask)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("StartLightningDamageDelay failed: task is null. ability=%s authority=%s"),
			*GetNameSafe(this),
			K2_HasAuthority() ? TEXT("true") : TEXT("false"));
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
		DecalTargetActor->Decal = GetConfiguredTargetingDecal();
		DecalTargetActor->DecalSize = CachedAOERadius * 2.0;
		DecalTargetActor->DecalColor = GetConfiguredTargetingDecalColor();
	}
}

void UAOEAttackAbility::ApplyEffectToHitActor(AActor* HitActor)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetAvatarActorFromActorInfo());
	if (!SourceASC || !TargetASC)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("ApplyEffectToHitActor skipped: sourceASC=%s targetASC=%s target=%s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetASC),
			*GetNameSafe(HitActor));
		return;
	}

	FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageEffectSpec();
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("ApplyEffectToHitActor skipped: invalid damage spec. target=%s damageEffect=%s"),
			*GetNameSafe(HitActor),
			*GetNameSafe(GetConfiguredDamageEffectClass().Get()));
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("ApplyEffectToHitActor: ability=%s target=%s applied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(HitActor),
		AppliedHandle.WasSuccessfullyApplied() ? TEXT("true") : TEXT("false"));
}

void UAOEAttackAbility::ApplyDirectAOECamera(bool bEnabled) const
{
	APdPlayer* Player = Cast<APdPlayer>(GetAvatarActorFromActorInfo());
	if (!Player || !Player->IsLocallyControlled())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Verbose,
			TEXT("Direct targeting camera skipped: ability=%s enabled=%s avatar=%s player=%s local=%s"),
			*GetNameSafe(this),
			bEnabled ? TEXT("true") : TEXT("false"),
			*GetNameSafe(GetAvatarActorFromActorInfo()),
			*GetNameSafe(Player),
			Player && Player->IsLocallyControlled() ? TEXT("true") : TEXT("false"));
		return;
	}

	const FWeaponAimCameraSettings CameraSettings = GetConfiguredAOECameraSettings();

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("Direct targeting camera apply: ability=%s enabled=%s player=%s fov=%.1f offset=%s rotation=%s interp=%.1f"),
		*GetNameSafe(this),
		bEnabled ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Player),
		CameraSettings.TargetFOV,
		*CameraSettings.TargetBoomSocketOffset.ToCompactString(),
		*CameraSettings.TargetCameraRotation.ToCompactString(),
		CameraSettings.InterpSpeed);

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

	UWorld* World = AttackTarget->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector TargetLocation = AttackTarget->GetActorLocation();
	const FVector TraceStart = TargetLocation + FVector(0.0, 0.0, 100.0);
	const FVector TraceEnd = TargetLocation - FVector(0.0, 0.0, FMath::Max(GetConfiguredTargetGroundTraceDepth(), 100.0f));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AttackTarget);
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		ActorsToIgnore.Add(AvatarActor);
	}

	FHitResult GroundHit;
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(
		this,
		TraceStart,
		TraceEnd,
		GetConfiguredTargetGroundTraceChannel(),
		false,
		ActorsToIgnore,
		GetConfiguredDebugTargeting() ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		GroundHit,
		true);

	if (bHit && GroundHit.bBlockingHit)
	{
		OutGroundLocation = GroundHit.Location;
		UE_LOG(LogPandoraAOEAttackAbility, Log,
			TEXT("AI target ground resolved: ability=%s target=%s targetLocation=%s ground=%s groundActor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AttackTarget),
			*TargetLocation.ToCompactString(),
			*OutGroundLocation.ToCompactString(),
			*GetNameSafe(GroundHit.GetActor()));
		return true;
	}

	UE_LOG(LogPandoraAOEAttackAbility, Warning,
		TEXT("AI target ground trace missed: ability=%s target=%s start=%s end=%s"),
		*GetNameSafe(this),
		*GetNameSafe(AttackTarget),
		*TraceStart.ToCompactString(),
		*TraceEnd.ToCompactString());
	return false;
}

FVector UAOEAttackAbility::ResolveConfirmedAOELocation(const FHitResult& HitResult, const FVector& TargetDataEndPoint) const
{
	FVector ResolvedLocation = HitResult.Location.IsNearlyZero() ? TargetDataEndPoint : HitResult.Location;

	AActor* HitActor = HitResult.GetActor();
	if (!IsValid(HitActor) || !HitActor->IsA<APawn>())
	{
		return ResolvedLocation;
	}

	UWorld* World = HitActor->GetWorld();
	if (!World)
	{
		return ResolvedLocation;
	}

	const FVector TraceStart = HitActor->GetActorLocation() + FVector(0.0, 0.0, 100.0);
	const FVector TraceEnd = HitActor->GetActorLocation() - FVector(0.0, 0.0, 10000.0);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AOETargetPawnGroundProjection), false);
	QueryParams.AddIgnoredActor(HitActor);
	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		QueryParams.AddIgnoredActor(AvatarActor);
	}

	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) && GroundHit.bBlockingHit)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Log,
			TEXT("TargetData pawn hit projected to ground: ability=%s hitActor=%s original=%s projected=%s groundActor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(HitActor),
			*ResolvedLocation.ToCompactString(),
			*GroundHit.Location.ToCompactString(),
			*GetNameSafe(GroundHit.GetActor()));
		return GroundHit.Location;
	}

	UE_LOG(LogPandoraAOEAttackAbility, Warning,
		TEXT("TargetData pawn hit ground projection failed: ability=%s hitActor=%s original=%s traceStart=%s traceEnd=%s"),
		*GetNameSafe(this),
		*GetNameSafe(HitActor),
		*ResolvedLocation.ToCompactString(),
		*TraceStart.ToCompactString(),
		*TraceEnd.ToCompactString());

	return ResolvedLocation;
}

FGameplayEffectSpecHandle UAOEAttackAbility::MakeDamageEffectSpec() const
{
	UPdAbilitySystemComponent* SourceASC = GetPdAbilitySystemComponentFromActorInfo();
	const TSubclassOf<UGameplayEffect> ConfiguredDamageEffectClass = GetConfiguredDamageEffectClass();
	if (!SourceASC || !ConfiguredDamageEffectClass)
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("MakeDamageEffectSpec failed: sourceASC=%s damageEffect=%s ability=%s"),
			*GetNameSafe(SourceASC),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()),
			*GetNameSafe(this));
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	if (const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec())
	{
		if (UObject* SourceObject = AbilitySpec->SourceObject.Get())
		{
			EffectContext.AddSourceObject(SourceObject);
		}
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		SourceASC->MakeOutgoingSpec(ConfiguredDamageEffectClass, FMath::Max(GetAbilityLevel(), 1), EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag ResolvedDamageDataTag = GetConfiguredDamageDataTag();
	if (!ResolvedDamageDataTag.IsValid())
	{
		SourceASC->ResolveDamageMagnitudeSetByCallerTag(ResolvedDamageDataTag);
	}

	if (ResolvedDamageDataTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(ResolvedDamageDataTag, CalculateDamageMagnitude());
	}
	else
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("MakeDamageEffectSpec warning: invalid damage data tag. ability=%s damageEffect=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ConfiguredDamageEffectClass.Get()));
	}

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("MakeDamageEffectSpec: ability=%s damageEffect=%s level=%d dataTag=%s magnitude=%.1f valid=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ConfiguredDamageEffectClass.Get()),
		FMath::Max(GetAbilityLevel(), 1),
		*ResolvedDamageDataTag.ToString(),
		CalculateDamageMagnitude(),
		DamageSpecHandle.IsValid() ? TEXT("true") : TEXT("false"));

	return DamageSpecHandle;
}

bool UAOEAttackAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	return AvatarPawn && Cast<APlayerController>(AvatarPawn->GetController());
}

UAnimMontage* UAOEAttackAbility::GetConfiguredTargetingMontage() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetingMontage.Get() : nullptr;
}

UAnimMontage* UAOEAttackAbility::GetConfiguredTriggerMontage() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETriggerMontage.Get() : nullptr;
}

TSubclassOf<UGameplayEffect> UAOEAttackAbility::GetConfiguredDamageEffectClass() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOEDamageEffectClass : nullptr;
}

TArray<TEnumAsByte<EObjectTypeQuery>> UAOEAttackAbility::GetConfiguredDamageObjectTypes() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOEDamageObjectTypes : TArray<TEnumAsByte<EObjectTypeQuery>>();
}

TSubclassOf<AGameplayAbilityTargetActor> UAOEAttackAbility::GetConfiguredTargetActorClass() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetActorClass : nullptr;
}

UMaterialInterface* UAOEAttackAbility::GetConfiguredTargetingDecal() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetingDecal.Get() : nullptr;
}

FLinearColor UAOEAttackAbility::GetConfiguredTargetingDecalColor() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetingDecalColor : FLinearColor::White;
}

FName UAOEAttackAbility::GetConfiguredTargetingTraceProfileName() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetingTraceProfileName : NAME_None;
}

float UAOEAttackAbility::GetConfiguredTargetingMaxRange() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingMaxRange) : 0.0f;
}

float UAOEAttackAbility::GetConfiguredTargetingCollisionRadius() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingCollisionRadius) : 0.0f;
}

float UAOEAttackAbility::GetConfiguredTargetingCollisionHeight() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetingCollisionHeight) : 0.0f;
}

bool UAOEAttackAbility::GetConfiguredTargetingTraceAffectsAimPitch() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->bAOETargetingTraceAffectsAimPitch;
}

bool UAOEAttackAbility::GetConfiguredDebugTargeting() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->bAOEDebugTargeting;
}

bool UAOEAttackAbility::GetConfiguredDrawDebugDamageRadius() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->bAOEDrawDebugDamageRadius;
}

float UAOEAttackAbility::GetConfiguredDebugDamageRadiusDrawTime() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->AOEDebugDamageRadiusDrawTime, 0.0)) : 0.0f;
}

FName UAOEAttackAbility::GetConfiguredTargetingSocketName() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOETargetingSocketName : NAME_None;
}

TEnumAsByte<ETraceTypeQuery> UAOEAttackAbility::GetConfiguredTargetGroundTraceChannel() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset)
	{
		return SkillDataAsset->AOETargetGroundTraceChannel;
	}

	return TEnumAsByte<ETraceTypeQuery>(TraceTypeQuery1);
}

float UAOEAttackAbility::GetConfiguredTargetGroundTraceDepth() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(SkillDataAsset->AOETargetGroundTraceDepth) : 0.0f;
}

FGameplayTag UAOEAttackAbility::GetConfiguredDamageDataTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOEDamageDataTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredMontageTriggerEventTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOEMontageTriggerEventTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredAOEIndicatorCueTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOEIndicatorCueTag : FGameplayTag();
}

FGameplayTag UAOEAttackAbility::GetConfiguredLightningBoltCueTag() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->AOELightningBoltCueTag : FGameplayTag();
}

float UAOEAttackAbility::GetConfiguredLightningDamageDelay() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? static_cast<float>(FMath::Max(SkillDataAsset->AOELightningDamageDelay, 0.0)) : 0.0f;
}

FWeaponAimCameraSettings UAOEAttackAbility::GetConfiguredAOECameraSettings() const
{
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset && SkillDataAsset->bUseAOECameraSettings)
	{
		return SkillDataAsset->AOECameraSettings;
	}

	return FWeaponAimCameraSettings();
}

double UAOEAttackAbility::CalculateAOERadiusFromSkillData() const
{
	const double AbilityLevel = static_cast<double>(FMath::Max(GetAbilityLevel(), 1));
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	const double ConfiguredBaseAOERadius = SkillDataAsset ? SkillDataAsset->AOERadius : 0.0;
	const double ConfiguredRadiusPercentIncreasePerLevel = SkillDataAsset ? SkillDataAsset->AOERadiusPercentIncreasePerLevel : 0.0;
	return ConfiguredBaseAOERadius + ((ConfiguredBaseAOERadius * ConfiguredRadiusPercentIncreasePerLevel) * (AbilityLevel - 1.0));
}

float UAOEAttackAbility::CalculateDamageMagnitude() const
{
	const double AbilityLevel = static_cast<double>(FMath::Max(GetAbilityLevel(), 1));
	const USkillDataAsset* SkillDataAsset = GetSourceSkillDataAsset();
	const double ConfiguredDamageMagnitude = SkillDataAsset ? SkillDataAsset->AOEDamageMagnitude : 0.0;
	const double ConfiguredDamagePercentIncreasePerLevel = SkillDataAsset ? SkillDataAsset->AOEDamagePercentIncreasePerLevel : 0.0;
	const double ScaledDamage = ConfiguredDamageMagnitude + ((ConfiguredDamageMagnitude * ConfiguredDamagePercentIncreasePerLevel) * (AbilityLevel - 1.0));
	return static_cast<float>(ScaledDamage);
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

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("AOEDebugRadius drawn: context=%s ability=%s avatar=%s authority=%s localController=%s center=%s radius=%.1f drawTime=%.1f shape=Hemisphere"),
		Context ? Context : TEXT("None"),
		*GetNameSafe(this),
		*GetNameSafe(AvatarActor),
		AvatarActor && AvatarActor->HasAuthority() ? TEXT("true") : TEXT("false"),
		HasPlayerController() ? TEXT("true") : TEXT("false"),
		*ConfirmedAOELocation.ToCompactString(),
		CachedAOERadius,
		DrawTime);
}

void UAOEAttackAbility::HandleCancelInputPressed(float TimeWaited)
{
	static_cast<void>(TimeWaited);
	K2_CancelAbility();
}

void UAOEAttackAbility::HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	bIsWaitingTargetData = false;

	FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Data, 0);
	const FVector TargetDataEndPoint = UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(Data, 0);
	ConfirmedAOELocation = ResolveConfirmedAOELocation(HitResult, TargetDataEndPoint);

	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("TargetData valid: ability=%s hitLocation=%s targetEndPoint=%s confirmedLocation=%s hitActor=%s blockingHit=%s"),
		*GetNameSafe(this),
		*HitResult.Location.ToCompactString(),
		*TargetDataEndPoint.ToCompactString(),
		*ConfirmedAOELocation.ToCompactString(),
		*GetNameSafe(HitResult.GetActor()),
		HitResult.bBlockingHit ? TEXT("true") : TEXT("false"));

	ConfirmStrike();
}

void UAOEAttackAbility::HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	static_cast<void>(Data);
	K2_EndAbility();
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
		K2_EndAbility();
	}
}

void UAOEAttackAbility::HandleTriggerMontageFinished()
{
	TriggerMontageTask = nullptr;
	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("TriggerMontage finished: ability=%s authority=%s strikeTriggered=%s waitingLightningDamage=%s delayTask=%s"),
		*GetNameSafe(this),
		K2_HasAuthority() ? TEXT("true") : TEXT("false"),
		bStrikeTriggered ? TEXT("true") : TEXT("false"),
		bWaitingLightningDamage ? TEXT("true") : TEXT("false"),
		*GetNameSafe(LightningDamageDelayTask));

	if (!bStrikeTriggered)
	{
		K2_EndAbility();
	}
}

void UAOEAttackAbility::HandleTriggerMontageInterrupted()
{
	TriggerMontageTask = nullptr;
	UE_LOG(LogPandoraAOEAttackAbility, Warning,
		TEXT("TriggerMontage interrupted/cancelled: ability=%s strikeTriggered=%s"),
		*GetNameSafe(this),
		bStrikeTriggered ? TEXT("true") : TEXT("false"));
	K2_EndAbility();
}

void UAOEAttackAbility::HandleMontageTriggerEvent(FGameplayEventData Payload)
{
	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("MontageTriggerEvent received: ability=%s payloadTag=%s expectedTag=%s location=%s alreadyTriggered=%s"),
		*GetNameSafe(this),
		*Payload.EventTag.ToString(),
		*GetConfiguredMontageTriggerEventTag().ToString(),
		*ConfirmedAOELocation.ToCompactString(),
		bStrikeTriggered ? TEXT("true") : TEXT("false"));

	if (bStrikeTriggered)
	{
		return;
	}

	bStrikeTriggered = true;

	if (!K2_CommitAbility())
	{
		UE_LOG(LogPandoraAOEAttackAbility, Warning,
			TEXT("Montage trigger commit failed: ability=%s"),
			*GetNameSafe(this));
		K2_EndAbility();
		return;
	}

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
	UE_LOG(LogPandoraAOEAttackAbility, Log,
		TEXT("LightningDamageDelay finished: ability=%s authority=%s location=%s"),
		*GetNameSafe(this),
		bHasAuthority ? TEXT("true") : TEXT("false"),
		*ConfirmedAOELocation.ToCompactString());

	if (!bHasAuthority)
	{
		return;
	}

	AOEDamage();
	RemovePersistentGameplayCues();
	K2_EndAbility();
}
