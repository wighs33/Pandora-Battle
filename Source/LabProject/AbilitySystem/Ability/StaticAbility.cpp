#include "AbilitySystem/Ability/StaticAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/SkillGroundProjection.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "AbilitySystem/StaticActors/AnimeAuraActor.h"
#include "AbilitySystem/StaticActors/OmenOrbGlitchActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StaticAbility)

namespace
{
	const FSkillStaticSettings* GetStaticSettingsFromSkill(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->StaticSettings.bEnabled
			? &SkillDataAsset->StaticSettings
			: nullptr;
	}

	bool HasConfiguredStaticTriggerDamage(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass != nullptr;
	}

	bool ShouldRepeatStaticTriggerDamage(const USkillDefinition* SkillDataAsset, const FSkillStaticSettings* StaticSettings)
	{
		if (SkillDataAsset && SkillDataAsset->Damage.bRepeatTriggerDamageWhileOverlapping)
		{
			return true;
		}

		return StaticSettings && StaticSettings->bRepeatTriggerDamageWhileOverlapping;
	}

	double GetStaticTriggerDamageInterval(const USkillDefinition* SkillDataAsset, const FSkillStaticSettings* StaticSettings)
	{
		if (SkillDataAsset && SkillDataAsset->Damage.bRepeatTriggerDamageWhileOverlapping)
		{
			return SkillDataAsset->Damage.TriggerDamageInterval;
		}

		return StaticSettings ? StaticSettings->TriggerDamageInterval : 0.0;
	}

	bool IsStaticSourceActorTarget(AActor* SourceActor, AActor* DamageSourceActor, AActor* HitActor)
	{
		if (!IsValid(HitActor))
		{
			return false;
		}

		if (HitActor == SourceActor || HitActor == DamageSourceActor)
		{
			return true;
		}

		if (DamageSourceActor)
		{
			if (HitActor == DamageSourceActor->GetOwner()
				|| HitActor == DamageSourceActor->GetInstigator()
				|| HitActor == DamageSourceActor->GetAttachParentActor())
			{
				return true;
			}
		}

		if (const APawn* SourcePawn = Cast<APawn>(SourceActor))
		{
			if (const APawn* HitPawn = Cast<APawn>(HitActor))
			{
				return SourcePawn == HitPawn
					|| (SourcePawn->GetController() && SourcePawn->GetController() == HitPawn->GetController());
			}
		}

		return false;
	}
}

UStaticAbility::UStaticAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UStaticAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	CleanupStaticTasks();
	SpawnedStaticActors.Reset();
	StaticTriggerComponents.Reset();
	PendingStaticSocketNames.Reset();
	DamagedStaticTriggerActorsBySource.Reset();
	StaticDamageSourceActorsByKey.Reset();
	StaticOverlappingActorsBySource.Reset();
	NextStaticSocketIndex = 0;
	bStaticStarted = false;
	MovementSpeedEffectHandle.Invalidate();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaticSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(StaticRepeatSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(StaticEndTimerHandle);
		World->GetTimerManager().ClearTimer(StaticTriggerDamageTickTimerHandle);
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	const AActor* AvatarActor = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset || !StaticSettings)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StaticSettings->StaticActorClass)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	StartStaticDurationTimerFromSkillStart();
	StartStaticDurationMovementLockIfAllowed();
	StartWaitStaticMontageTriggerTask();

	if (!GetResolvedStaticMontage())
	{
		TryCommitAndStartStatic();
		return;
	}

	if (!StartStaticMontageTask())
	{
		TryCommitAndStartStatic();
		return;
	}

}

void UStaticAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	CleanupStaticTasks();
	RemoveStaticMovementSpeedIncrease();

	TSet<AActor*> ActorsWithBoundDamageTriggers;
	for (const UPrimitiveComponent* TriggerComponent : StaticTriggerComponents)
	{
		if (TriggerComponent && TriggerComponent->GetOwner())
		{
			ActorsWithBoundDamageTriggers.Add(TriggerComponent->GetOwner());
		}
	}

	UnbindStaticTriggerDamage();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaticSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(StaticRepeatSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(StaticEndTimerHandle);
		World->GetTimerManager().ClearTimer(StaticTriggerDamageTickTimerHandle);
	}
	StaticSpawnTimerHandle.Invalidate();
	StaticRepeatSpawnTimerHandle.Invalidate();
	StaticEndTimerHandle.Invalidate();
	StaticTriggerDamageTickTimerHandle.Invalidate();

	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	const bool bDestroySpawnedActorsOnAbilityEnd = StaticSettings && StaticSettings->bDestroySpawnedActorsOnAbilityEnd;
	if (StaticSettings && (bDestroySpawnedActorsOnAbilityEnd || !SpawnedStaticActors.IsEmpty()))
	{
		for (AActor* SpawnedActor : SpawnedStaticActors)
		{
			const bool bForceDestroyForSourceBuffActor = SpawnedActor && SpawnedActor->IsA<AAnimeAuraActor>();
			const bool bForceDestroyForBoundDamageTrigger =
				SpawnedActor && ActorsWithBoundDamageTriggers.Contains(SpawnedActor);
			const bool bExpiresThroughConfiguredLifeSpan =
				StaticSettings->bUseSpawnedActorLifeSpan
				&& StaticSettings->SpawnedActorLifeSpan > 0.0;
			if (SpawnedActor
				&& SpawnedActor->HasAuthority()
				&& (bDestroySpawnedActorsOnAbilityEnd
					|| bForceDestroyForSourceBuffActor
					|| (bForceDestroyForBoundDamageTrigger && !bExpiresThroughConfiguredLifeSpan)))
			{
				DestroyStaticActorWhenReplicationIsSafe(SpawnedActor, *StaticSettings);
			}
		}
	}

	SpawnedStaticActors.Reset();
	StaticTriggerComponents.Reset();
	PendingStaticSocketNames.Reset();
	DamagedStaticTriggerActorsBySource.Reset();
	StaticDamageSourceActorsByKey.Reset();
	StaticOverlappingActorsBySource.Reset();
	NextStaticSocketIndex = 0;
	bStaticStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

const FSkillStaticSettings* UStaticAbility::GetStaticSettings() const
{
	return GetStaticSettingsFromSkill(GetSourceSkillDataAsset());
}

UAnimMontage* UStaticAbility::GetResolvedStaticMontage() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag UStaticAbility::GetResolvedStaticTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid()
		? SkillDataAsset->Animation.PrimaryEventTag
		: LabGameplayTags::Event_Montage_Trigger;
}

bool UStaticAbility::StartStaticMontageTask()
{
	UAnimMontage* MontageToPlay = GetResolvedStaticMontage();
	if (!MontageToPlay)
	{
		return false;
	}

	StaticMontageTask = CreateDefaultMontageAndWaitTask(MontageToPlay);
	if (!StaticMontageTask)
	{

		return false;
	}

	StaticMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleStaticMontageFinished);
	StaticMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleStaticMontageFinished);
	StaticMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleStaticMontageInterrupted);
	StaticMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleStaticMontageInterrupted);
	StaticMontageTask->ReadyForActivation();
	return true;
}

void UStaticAbility::StartWaitStaticMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedStaticTriggerEventTag();
	if (!TriggerTag.IsValid())
	{
		return;
	}

	WaitStaticMontageTriggerTask = CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitStaticMontageTriggerTask)
	{

		return;
	}

	WaitStaticMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleStaticMontageTriggerEvent);
	WaitStaticMontageTriggerTask->ReadyForActivation();
}

void UStaticAbility::TryCommitAndStartStatic()
{
	if (bStaticStarted || !CanExecuteSkillPayload())
	{
		return;
	}
	bStaticStarted = true;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		StartConfiguredDefaultFX();
		if (!StaticEndTimerHandle.IsValid() && !GetResolvedStaticMontage())
		{
			K2_EndAbilityLocally();
		}
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{

		K2_CancelAbility();
		return;
	}

	ApplyStaticMovementSpeedIncrease();
	StartConfiguredDefaultFX();
	SpawnConfiguredCharacterDecal();
	StartStaticDurationMovementLockIfAllowed();
	StartStaticSpawnSequence();
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		StartStaticRepeatAndEndTimers();
	}
}

void UStaticAbility::ApplyStaticMovementSpeedIncrease()
{
	if (MovementSpeedEffectHandle.IsValid())
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> MovementSpeedEffectClass =
		SettingDefinition
			? SettingDefinition->MovementSpeedGameplayEffectClass
			: nullptr;
	if (!SkillDataAsset
		|| !SkillDataAsset->Movement.bOverrideMovementSpeedWhileActive
		|| !Character
		|| !Character->HasAuthority()
		|| !AbilitySystemComponent
		|| !MovementSpeedEffectClass)
	{
		return;
	}

	const double ConfiguredMovementSpeedIncrease = SkillDataAsset->Movement.DashStrength > 0.0
		? SkillDataAsset->Movement.DashStrength
		: SkillDataAsset->Movement.MovementSpeedWhileActive;
	if (ConfiguredMovementSpeedIncrease <= 0.0)
	{
		return;
	}

	FGameplayEffectSpecHandle MovementSpeedSpec =
		MakeOutgoingGameplayEffectSpec(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			MovementSpeedEffectClass,
			GetAbilityLevel());
	if (!MovementSpeedSpec.IsValid() || !MovementSpeedSpec.Data.IsValid())
	{
		return;
	}

	MovementSpeedSpec.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_MovementSpeed,
		static_cast<float>(ConfiguredMovementSpeedIncrease));
	MovementSpeedEffectHandle = ApplyGameplayEffectSpecToOwner(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		MovementSpeedSpec);
}

void UStaticAbility::RemoveStaticMovementSpeedIncrease()
{
	if (!MovementSpeedEffectHandle.IsValid())
	{
		return;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (Character && Character->HasAuthority() && AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(
			MovementSpeedEffectHandle,
			1);
	}

	MovementSpeedEffectHandle.Invalidate();
}

bool UStaticAbility::StartStaticDurationTimerFromSkillStart()
{
	if (StaticEndTimerHandle.IsValid())
	{
		return true;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset
		|| SkillDataAsset->SkillType != ESkillType::Duration
		|| SkillDataAsset->Time.Duration <= 0.0)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	World->GetTimerManager().SetTimer(
		StaticEndTimerHandle,
		this,
		&ThisClass::HandleStaticDurationFinished,
		static_cast<float>(SkillDataAsset->Time.Duration),
		false);
	return true;
}

void UStaticAbility::StartStaticDurationMovementLockIfAllowed()
{
	if (ShouldSkipStaticDurationMovementLock())
	{

		return;
	}

	StartDurationMovementLock();
}

bool UStaticAbility::ShouldSkipStaticDurationMovementLock() const
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	return StaticSettings
		&& StaticSettings->StaticActorClass
		&& StaticSettings->StaticActorClass.Get()->IsChildOf(AAnimeAuraActor::StaticClass());
}

TArray<FName> UStaticAbility::GetConfiguredStaticSocketNames() const
{
	TArray<FName> SocketNames;
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (StaticSettings && !StaticSettings->bUseSpawnSockets)
	{
		SocketNames.Add(NAME_None);
		return SocketNames;
	}

	if (StaticSettings)
	{
		for (const FName& SocketName : StaticSettings->SpawnSocketNames)
		{
			if (!SocketName.IsNone())
			{
				SocketNames.Add(SocketName);
			}
		}
	}

	if (SocketNames.IsEmpty())
	{
		SocketNames.Add(NAME_None);
	}

	return SocketNames;
}

void UStaticAbility::StartStaticSpawnSequence()
{
	PendingStaticSocketNames = GetConfiguredStaticSocketNames();
	NextStaticSocketIndex = 0;

	if (PendingStaticSocketNames.IsEmpty())
	{
		ScheduleStaticAbilityEnd();
		return;
	}

SpawnNextStaticActor();
}

void UStaticAbility::StartStaticRepeatAndEndTimers()
{
	if (!ShouldRepeatStaticSpawnSequence())
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	UWorld* World = GetWorld();
	if (!World || !SkillDataAsset || !StaticSettings)
	{
		return;
	}

	const float RepeatInterval = static_cast<float>(FMath::Max(StaticSettings->RepeatSpawnInterval, 0.1));
	World->GetTimerManager().SetTimer(
		StaticRepeatSpawnTimerHandle,
		this,
		&ThisClass::HandleRepeatedStaticSpawnSequence,
		RepeatInterval,
		true);

	if (!StaticEndTimerHandle.IsValid())
	{
		World->GetTimerManager().SetTimer(
			StaticEndTimerHandle,
			this,
			&ThisClass::HandleStaticDurationFinished,
			static_cast<float>(SkillDataAsset->Time.Duration),
			false);
	}

}

void UStaticAbility::SpawnNextStaticActor()
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!StaticSettings)
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}

	if (!PendingStaticSocketNames.IsValidIndex(NextStaticSocketIndex))
	{
		FinishStaticSpawnSequence();
		return;
	}

	SpawnStaticActorForSocket(
		PendingStaticSocketNames[NextStaticSocketIndex],
		NextStaticSocketIndex,
		PendingStaticSocketNames.Num());

	++NextStaticSocketIndex;
	if (!PendingStaticSocketNames.IsValidIndex(NextStaticSocketIndex))
	{
		FinishStaticSpawnSequence();
		return;
	}

	const float SpawnInterval = static_cast<float>(FMath::Max(StaticSettings->SpawnInterval, 0.0));
	if (SpawnInterval <= KINDA_SMALL_NUMBER)
	{
		SpawnNextStaticActor();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StaticSpawnTimerHandle,
			this,
			&ThisClass::SpawnNextStaticActor,
			SpawnInterval,
			false);
	}
}

void UStaticAbility::FinishStaticSpawnSequence()
{
	PendingStaticSocketNames.Reset();
	NextStaticSocketIndex = 0;
	if (SpawnedStaticActors.IsEmpty())
	{
		CancelAbilityForSkillExecutionFailure();
		return;
	}
	ScheduleStaticAbilityEnd();
}

AActor* UStaticAbility::SpawnStaticActorForSocket(const FName SocketName, const int32 SocketIndex, const int32 SocketCount)
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!StaticSettings || !AvatarActor || !AvatarActor->HasAuthority() || !World || !StaticSettings->StaticActorClass)
	{
		return nullptr;
	}

	const FTransform SpawnTransform = ResolveStaticSpawnTransform(SocketName);
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = StaticSettings->SpawnCollisionHandling;

	AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(
		StaticSettings->StaticActorClass,
		SpawnTransform,
		SpawnParams.Owner,
		SpawnParams.Instigator,
		SpawnParams.SpawnCollisionHandlingOverride);
	if (!SpawnedActor
		&& SpawnParams.SpawnCollisionHandlingOverride
			!= ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
	{
		SpawnedActor = World->SpawnActorDeferred<AActor>(
			StaticSettings->StaticActorClass,
			SpawnTransform,
			SpawnParams.Owner,
			SpawnParams.Instigator,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	if (!SpawnedActor)
	{

		return nullptr;
	}

	const bool bShouldReplicateSpawnedActor =
		StaticSettings->bForceReplicateSpawnedActor || SpawnedActor->IsA<AEffectAreaBase>();

	if (AOmenOrbGlitchActor* OmenOrbGlitchActor = Cast<AOmenOrbGlitchActor>(SpawnedActor))
	{
		const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
		FSkillGameplayEffectConfig FinishDamageConfig;
		FinishDamageConfig.MagnitudeDataTag = LabGameplayTags::Data_Damage;
		if (SkillDataAsset)
		{
			FinishDamageConfig = SkillDataAsset->GetResolvedStaticFinishDamageConfig();
		}
		const UPandoraSkillRuntimeContext* RuntimeContext = GetSourceSkillRuntimeContext();
		OmenOrbGlitchActor->ConfigureFromStaticSettings(
			*StaticSettings,
			FinishDamageConfig,
			FMath::Max(GetAbilityLevel(), 1),
			RuntimeContext ? RuntimeContext->GetLoadoutDirection() : EEnum_Direction::Center);
	}

	if (AAnimeAuraActor* AnimeAuraActor = Cast<AAnimeAuraActor>(SpawnedActor))
	{
		AnimeAuraActor->ConfigurePresentationSettings(
			StaticSettings->AnimeAuraPresentation);
	}

	if (AEffectAreaBase* EffectArea = Cast<AEffectAreaBase>(SpawnedActor))
	{
		EffectArea->SetSourceActor(AvatarActor);
		EffectArea->SetIgnoreSourceActor(StaticSettings->bIgnoreSourceActor || StaticSettings->bEffectAreaIgnoreSourceActor);
		EffectArea->SetAffectEnemiesOnly(StaticSettings->bEffectAreaAffectEnemiesOnly);
		if (const UPandoraSkillRuntimeContext* RuntimeContext = GetSourceSkillRuntimeContext())
		{
			EffectArea->SetSourcePandoraLoadoutDirection(RuntimeContext->GetLoadoutDirection());
		}
	}

	UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);
	if (bShouldReplicateSpawnedActor)
	{
		// SpawnActorDeferred returns before the actor is initialized. SetReplicates
		// cannot register an actor with the net driver in that state, so enabling it
		// before FinishSpawning can silently miss the actor's first replication.
		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
	}
	const bool bAttachedToSocket = AttachSpawnedStaticActorToSocket(SpawnedActor, SocketName);
	if (bShouldReplicateSpawnedActor)
	{
		SpawnedActor->ForceNetUpdate();
	}

	float RequestedLifeSpan = 0.0f;
	if (StaticSettings->bUseSpawnedActorLifeSpan && StaticSettings->SpawnedActorLifeSpan > 0.0)
	{
		RequestedLifeSpan = static_cast<float>(StaticSettings->SpawnedActorLifeSpan);
	}
	else
	{
		RequestedLifeSpan = SpawnedActor->GetLifeSpan();
	}

	if (RequestedLifeSpan > 0.0f)
	{
		if (bShouldReplicateSpawnedActor)
		{
			RequestedLifeSpan = FMath::Max(
				RequestedLifeSpan,
				static_cast<float>(FMath::Max(StaticSettings->MinimumReplicatedActorLifetime, 0.0)));
		}
		SpawnedActor->SetLifeSpan(RequestedLifeSpan);
	}

	SpawnedStaticActors.Add(SpawnedActor);
	BindStaticTriggerDamage(SpawnedActor);

	return SpawnedActor;
}

void UStaticAbility::DestroyStaticActorWhenReplicationIsSafe(
	AActor* SpawnedActor,
	const FSkillStaticSettings& StaticSettings) const
{
	if (!SpawnedActor || !SpawnedActor->HasAuthority())
	{
		return;
	}

	const float MinimumReplicatedLifetime = SpawnedActor->GetIsReplicated()
		? static_cast<float>(FMath::Max(StaticSettings.MinimumReplicatedActorLifetime, 0.0))
		: 0.0f;
	const float RemainingReplicationLifetime = FMath::Max(
		MinimumReplicatedLifetime - SpawnedActor->GetGameTimeSinceCreation(),
		0.0f);
	if (RemainingReplicationLifetime > KINDA_SMALL_NUMBER)
	{
		// Damage delegates have already been removed. Keep only the replicated
		// actor alive long enough for a cold client to load and instantiate it.
		SpawnedActor->ForceNetUpdate();
		SpawnedActor->SetLifeSpan(RemainingReplicationLifetime);
		return;
	}

	SpawnedActor->Destroy();
}

FTransform UStaticAbility::ResolveStaticSpawnTransform(const FName SocketName) const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!AvatarActor || !StaticSettings)
	{
		return FTransform::Identity;
	}

	FTransform BaseTransform = AvatarActor->GetActorTransform();
	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor))
	{
		const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
		if (StaticSettings->bUseSpawnSockets && CharacterMesh && !SocketName.IsNone() && CharacterMesh->DoesSocketExist(SocketName))
		{
			BaseTransform = CharacterMesh->GetSocketTransform(SocketName, RTS_World);
		}
		else if (!StaticSettings->bUseSpawnSockets)
		{
			FVector FeetLocation = Character->GetActorLocation();
			if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent())
			{
				FeetLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
			}
			BaseTransform.SetLocation(FeetLocation);
		}
	}

	FVector SpawnLocation = BaseTransform.GetLocation()
		+ (StaticSettings->bUseSpawnSockets
			? BaseTransform.GetRotation().RotateVector(StaticSettings->SpawnLocationOffset)
			: StaticSettings->SpawnLocationOffset);
	const FRotator SpawnRotation = BaseTransform.Rotator() + StaticSettings->SpawnRotationOffset;
	if (StaticSettings->bProjectSpawnToGround)
	{
		UWorld* World = AvatarActor->GetWorld();
		const FVector TraceBaseLocation = StaticSettings->bUseSpawnSockets
			? BaseTransform.GetLocation()
			: AvatarActor->GetActorLocation();

		TArray<AActor*> ActorsToIgnore;
		PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, AvatarActor);

		PdSkillGroundProjection::FGroundProjectionResult GroundProjection;
		if (PdSkillGroundProjection::TryProjectToGround(
			World,
			TraceBaseLocation,
			StaticSettings->GroundTraceChannel,
			StaticSettings->GroundTraceStartHeight,
			StaticSettings->GroundTraceDepth,
			ActorsToIgnore,
			GroundProjection))
		{
			SpawnLocation = GroundProjection.Location
				+ (StaticSettings->bUseSpawnSockets
					? BaseTransform.GetRotation().RotateVector(StaticSettings->SpawnLocationOffset)
					: StaticSettings->SpawnLocationOffset);
		}
	}
	return FTransform(SpawnRotation, SpawnLocation, BaseTransform.GetScale3D());
}

USkeletalMeshComponent* UStaticAbility::ResolveStaticSpawnSocketMesh(const FName SocketName) const
{
	if (SocketName.IsNone())
	{
		return nullptr;
	}

	const ACharacterBase* Character = Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	return CharacterMesh && CharacterMesh->DoesSocketExist(SocketName) ? CharacterMesh : nullptr;
}

bool UStaticAbility::AttachSpawnedStaticActorToSocket(AActor* SpawnedActor, const FName SocketName) const
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!SpawnedActor
		|| !StaticSettings
		|| !StaticSettings->bUseSpawnSockets
		|| !StaticSettings->bAttachSpawnedActorToSocket
		|| SocketName.IsNone())
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = ResolveStaticSpawnSocketMesh(SocketName);
	if (!CharacterMesh)
	{

		return false;
	}

	SpawnedActor->AttachToComponent(CharacterMesh, FAttachmentTransformRules::KeepWorldTransform, SocketName);
	return true;
}

bool UStaticAbility::ShouldRepeatStaticSpawnSequence() const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	return SkillDataAsset
		&& StaticSettings
		&& StaticSettings->bRepeatSpawnSequence
		&& SkillDataAsset->SkillType == ESkillType::Duration
		&& SkillDataAsset->Time.Duration > 0.0
		&& StaticSettings->RepeatSpawnInterval > 0.0;
}

UPrimitiveComponent* UStaticAbility::FindStaticTriggerComponent(AActor* SpawnedActor) const
{
	if (!SpawnedActor)
	{
		return nullptr;
	}

	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	const FName TriggerComponentName = StaticSettings ? StaticSettings->TriggerComponentName : NAME_None;

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SpawnedActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return nullptr;
	}

	if (!TriggerComponentName.IsNone())
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetFName() == TriggerComponentName)
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(TriggerComponentName))
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetName().Contains(TriggerComponentName.ToString()))
			{
				return PrimitiveComponent;
			}
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent
			&& PrimitiveComponent->GetGenerateOverlapEvents()
			&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{

			return PrimitiveComponent;
		}
	}

	return PrimitiveComponents[0];
}

void UStaticAbility::BindStaticTriggerDamage(AActor* SpawnedActor)
{
	if (!SpawnedActor || !SpawnedActor->HasAuthority())
	{
		return;
	}

	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!StaticSettings || !HasConfiguredStaticTriggerDamage(GetSourceSkillDataAsset()))
	{

		return;
	}

	if (SpawnedActor->IsA<AOmenOrbGlitchActor>() && StaticSettings->bOmenOrbApplyFinishAreaDamage)
	{

		return;
	}

	UPrimitiveComponent* TriggerComponent = FindStaticTriggerComponent(SpawnedActor);
	if (!TriggerComponent)
	{

		return;
	}

	// Static skill trigger volumes are gameplay-only overlap queries. Keeping them
	// as WorldDynamic lets weapon object traces hit the volume and then resolve its
	// owning character as the damage target.
	TriggerComponent->SetCollisionProfileName(TEXT("Custom"));
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerComponent->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	// Hide only the gameplay collision primitive. Propagating this state from a
	// root trigger also hides attached particle/Niagara components.
	TriggerComponent->SetHiddenInGame(true, false);
	TriggerComponent->SetGenerateOverlapEvents(true);
	TriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleStaticTriggerBeginOverlap);
	TriggerComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleStaticTriggerEndOverlap);
	StaticTriggerComponents.AddUnique(TriggerComponent);
	TriggerComponent->UpdateOverlaps();

	ApplyStaticTriggerDamageToExistingOverlaps(
		SpawnedActor,
		TriggerComponent,
		StaticSettings && StaticSettings->bDamageExistingOverlapsOnSpawn);
	StartStaticTriggerDamageTickIfNeeded();

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const bool bRepeatDamage = ShouldRepeatStaticTriggerDamage(SkillDataAsset, StaticSettings);
	const double TriggerDamageInterval = GetStaticTriggerDamageInterval(SkillDataAsset, StaticSettings);

}

void UStaticAbility::UnbindStaticTriggerDamage()
{
	for (UPrimitiveComponent* TriggerComponent : StaticTriggerComponents)
	{
		if (TriggerComponent)
		{
			TriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleStaticTriggerBeginOverlap);
			TriggerComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleStaticTriggerEndOverlap);
			TriggerComponent->SetGenerateOverlapEvents(false);
			TriggerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	StaticDamageSourceActorsByKey.Reset();
	StaticOverlappingActorsBySource.Reset();
}

void UStaticAbility::StartStaticTriggerDamageTickIfNeeded()
{
	if (StaticTriggerDamageTickTimerHandle.IsValid())
	{
		return;
	}

	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const bool bRepeatDamage = ShouldRepeatStaticTriggerDamage(SkillDataAsset, StaticSettings);
	const double TriggerDamageInterval = GetStaticTriggerDamageInterval(SkillDataAsset, StaticSettings);
	if (!StaticSettings
		|| !bRepeatDamage
		|| !HasConfiguredStaticTriggerDamage(SkillDataAsset)
		|| TriggerDamageInterval <= 0.0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float DamageInterval = static_cast<float>(FMath::Max(TriggerDamageInterval, 0.05));
	World->GetTimerManager().SetTimer(
		StaticTriggerDamageTickTimerHandle,
		this,
		&ThisClass::HandleStaticTriggerDamageTick,
		DamageInterval,
		true);

}

void UStaticAbility::HandleStaticTriggerDamageTick()
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!StaticSettings || !ShouldRepeatStaticTriggerDamage(GetSourceSkillDataAsset(), StaticSettings))
	{
		return;
	}

	TArray<FObjectKey> DamageSourceKeys;
	StaticOverlappingActorsBySource.GetKeys(DamageSourceKeys);

	for (const FObjectKey& SourceKey : DamageSourceKeys)
	{
		TWeakObjectPtr<AActor>* DamageSourcePtr = StaticDamageSourceActorsByKey.Find(SourceKey);
		AActor* DamageSourceActor = DamageSourcePtr ? DamageSourcePtr->Get() : nullptr;
		if (!IsValid(DamageSourceActor))
		{
			StaticDamageSourceActorsByKey.Remove(SourceKey);
			StaticOverlappingActorsBySource.Remove(SourceKey);
			continue;
		}

		TArray<TWeakObjectPtr<AActor>>* OverlappingActors = StaticOverlappingActorsBySource.Find(SourceKey);
		if (!OverlappingActors)
		{
			StaticDamageSourceActorsByKey.Remove(SourceKey);
			continue;
		}

		TArray<TWeakObjectPtr<AActor>> DamageTargets;
		DamageTargets.Reserve(OverlappingActors->Num());
		for (int32 ActorIndex = OverlappingActors->Num() - 1; ActorIndex >= 0; --ActorIndex)
		{
			AActor* OverlappingActor = (*OverlappingActors)[ActorIndex].Get();
			if (!IsValid(OverlappingActor))
			{
				OverlappingActors->RemoveAtSwap(ActorIndex);
				continue;
			}

			DamageTargets.Add(OverlappingActor);
		}

		if (OverlappingActors->IsEmpty())
		{
			StaticDamageSourceActorsByKey.Remove(SourceKey);
			StaticOverlappingActorsBySource.Remove(SourceKey);
			continue;
		}

		for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
		{
			AActor* OverlappingActor = TargetPtr.Get();
			if (!IsValid(DamageSourceActor) || !IsValid(OverlappingActor))
			{
				continue;
			}

			const TArray<TWeakObjectPtr<AActor>>* CurrentOverlappingActors = StaticOverlappingActorsBySource.Find(SourceKey);
			if (!CurrentOverlappingActors
				|| !CurrentOverlappingActors->ContainsByPredicate(
					[OverlappingActor](const TWeakObjectPtr<AActor>& ExistingActor)
					{
						return ExistingActor.Get() == OverlappingActor;
					}))
			{
				continue;
			}

			ApplyStaticTriggerDamage(DamageSourceActor, OverlappingActor, true);
		}
	}

}

void UStaticAbility::ApplyStaticTriggerDamageToExistingOverlaps(AActor* DamageSourceActor, UPrimitiveComponent* TriggerComponent, const bool bApplyDamage)
{
	if (!DamageSourceActor || !TriggerComponent)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TrackStaticTriggerOverlap(DamageSourceActor, OverlappingActor);
		if (bApplyDamage)
		{
			ApplyStaticTriggerDamage(DamageSourceActor, OverlappingActor);
		}
	}
}

void UStaticAbility::TrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor)
{
	if (!IsValid(DamageSourceActor) || !IsValid(OtherActor) || !OtherActor->IsA<ACharacterBase>())
	{
		return;
	}

	const FObjectKey SourceKey(DamageSourceActor);
	StaticDamageSourceActorsByKey.FindOrAdd(SourceKey) = DamageSourceActor;

	TArray<TWeakObjectPtr<AActor>>& OverlappingActors = StaticOverlappingActorsBySource.FindOrAdd(SourceKey);
	for (const TWeakObjectPtr<AActor>& ExistingActor : OverlappingActors)
	{
		if (ExistingActor.Get() == OtherActor)
		{
			return;
		}
	}

	OverlappingActors.Add(OtherActor);
}

void UStaticAbility::UntrackStaticTriggerOverlap(AActor* DamageSourceActor, AActor* OtherActor)
{
	if (!DamageSourceActor || !OtherActor)
	{
		return;
	}

	const FObjectKey SourceKey(DamageSourceActor);
	TArray<TWeakObjectPtr<AActor>>* OverlappingActors = StaticOverlappingActorsBySource.Find(SourceKey);
	if (!OverlappingActors)
	{
		return;
	}

	OverlappingActors->RemoveAllSwap(
		[OtherActor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == OtherActor;
		});

	if (OverlappingActors->IsEmpty())
	{
		StaticOverlappingActorsBySource.Remove(SourceKey);
		StaticDamageSourceActorsByKey.Remove(SourceKey);
	}
}

void UStaticAbility::ApplyStaticTriggerDamage(AActor* DamageSourceActor, AActor* HitActor, const bool bAllowRepeatedDamage)
{
	AActor* SourceActor = GetAvatarActorFromActorInfo();
	if (!SourceActor || !SourceActor->HasAuthority() || !IsValid(DamageSourceActor) || !IsValid(HitActor))
	{
		return;
	}

	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (StaticSettings && StaticSettings->bIgnoreSourceActor && IsStaticSourceActorTarget(SourceActor, DamageSourceActor, HitActor))
	{

		return;
	}

	TSet<FObjectKey>& DamagedActorsForSource = DamagedStaticTriggerActorsBySource.FindOrAdd(FObjectKey(DamageSourceActor));
	const FObjectKey HitActorKey(HitActor);
	if (!bAllowRepeatedDamage && DamagedActorsForSource.Contains(HitActorKey))
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(SourceActor);
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!TargetCharacter)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceASC || !TargetASC)
	{

		return;
	}

	if (TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	if (SourceCharacter && !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	const float DamageMagnitude = CalculateStaticTriggerDamageMagnitude();
	FGameplayEffectSpecHandle DamageSpecHandle = MakeStaticTriggerDamageSpec(DamageSourceActor, DamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{

		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		ApplyConfiguredStatusEffectToTarget(
			GetSourceSkillDataAsset(),
			TargetASC);
	}
	if (!bAllowRepeatedDamage)
	{
		DamagedActorsForSource.Add(HitActorKey);
	}

}

FGameplayEffectSpecHandle UStaticAbility::MakeStaticTriggerDamageSpec(AActor* DamageSourceActor, const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig TriggerDamage = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!SkillDataAsset || !TriggerDamage.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{

		return FGameplayEffectSpecHandle();
	}

	return MakeConfiguredDamageEffectSpec(TriggerDamage, DamageMagnitude, DamageSourceActor);
}

float UStaticAbility::CalculateStaticTriggerDamageMagnitude() const
{
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (!StaticSettings)
	{
		return 0.0f;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	return SkillDataAsset
		? CalculateSkillDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig())
		: 0.0f;
}

void UStaticAbility::ScheduleStaticAbilityEnd()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillStaticSettings* StaticSettings = GetStaticSettings();
	if (ShouldRepeatStaticSpawnSequence())
	{

		return;
	}

	float EndDelay = StaticSettings
		? static_cast<float>(FMath::Max(StaticSettings->TriggerActiveDurationAfterLastSpawn, 0.0))
		: 0.0f;

	if (SkillDataAsset && SkillDataAsset->SkillType == ESkillType::Duration && SkillDataAsset->Time.Duration > 0.0)
	{
		if (StaticEndTimerHandle.IsValid())
		{

			return;
		}

		EndDelay = static_cast<float>(SkillDataAsset->Time.Duration);
	}

	if (EndDelay <= KINDA_SMALL_NUMBER)
	{
		K2_EndAbility();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			StaticEndTimerHandle,
			this,
			&ThisClass::HandleStaticDurationFinished,
			EndDelay,
			false);
	}
}

void UStaticAbility::HandleRepeatedStaticSpawnSequence()
{
	if (!ShouldRepeatStaticSpawnSequence())
	{
		return;
	}

	if (!PendingStaticSocketNames.IsEmpty())
	{

		return;
	}

StartStaticSpawnSequence();
}

void UStaticAbility::CleanupStaticTasks()
{
	if (StaticMontageTask)
	{
		StaticMontageTask->EndTask();
		StaticMontageTask = nullptr;
	}

	if (WaitStaticMontageTriggerTask)
	{
		WaitStaticMontageTriggerTask->EndTask();
		WaitStaticMontageTriggerTask = nullptr;
	}
}

void UStaticAbility::HandleStaticMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	TryCommitAndStartStatic();
}

void UStaticAbility::HandleStaticMontageFinished()
{
	StaticMontageTask = nullptr;

	if (bStaticStarted)
	{
		const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if ((!AvatarActor || !AvatarActor->HasAuthority()) && !StaticEndTimerHandle.IsValid())
		{
			K2_EndAbilityLocally();
		}
		return;
	}

	TryCommitAndStartStatic();
}

void UStaticAbility::HandleStaticMontageInterrupted()
{
	StaticMontageTask = nullptr;

	if (bStaticStarted)
	{
		const AActor* AvatarActor = GetAvatarActorFromActorInfo();
		if ((!AvatarActor || !AvatarActor->HasAuthority()) && !StaticEndTimerHandle.IsValid())
		{
			K2_EndAbilityLocally();
		}
		return;
	}

	TryCommitAndStartStatic();
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (bStaticStarted && (!AvatarActor || !AvatarActor->HasAuthority()))
	{
		K2_EndAbilityLocally();
	}
}

void UStaticAbility::HandleStaticDurationFinished()
{
	FinishAbilityFromDuration();
}

void UStaticAbility::HandleStaticTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
	TrackStaticTriggerOverlap(DamageSourceActor, OtherActor);
	ApplyStaticTriggerDamage(DamageSourceActor, OtherActor);
}

void UStaticAbility::HandleStaticTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
	UntrackStaticTriggerOverlap(DamageSourceActor, OtherActor);
}
