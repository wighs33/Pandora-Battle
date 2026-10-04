#include "Skill/Actions/SkillActorFieldAction.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Skill/Actors/SkillEffectArea.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Skill/Actors/SkillPowerUpActor.h"
#include "Skill/Actors/SkillBlackHoleActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Pandora/PandoraSkillSource.h"
#include "Skill/Actions/SkillFieldPlacement.h"
#include "Skill/Actions/SkillTriggerDamage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillActorFieldAction)

namespace
{
bool HasConfiguredFieldTriggerDamage(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->GetResolvedDamageConfig().GameplayEffectClass != nullptr;
	}

	// 액터 필드의 트리거 볼륨은 게임플레이 전용 겹침 질의다. WorldDynamic으로 두면
	// 무기 오브젝트 trace가 볼륨에 맞은 뒤 그 소유 캐릭터를 피해 대상으로 찾을 수 있다.
	void ConfigureFieldTriggerCollision(UPrimitiveComponent& TriggerComponent)
	{
		TriggerComponent.SetCollisionProfileName(TEXT("Custom"));
		TriggerComponent.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		TriggerComponent.SetCollisionObjectType(LabCollisionChannels::OverlapBox());
		TriggerComponent.SetCollisionResponseToAllChannels(ECR_Ignore);
		TriggerComponent.SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		TriggerComponent.SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);

		// 게임플레이 충돌 프리미티브만 숨긴다. 루트 트리거에서 이 상태를 전파하면
		// 붙어 있는 파티클·Niagara 컴포넌트까지 숨는다.
		TriggerComponent.SetHiddenInGame(true, false);
	}

	bool IsFieldSourceActorTarget(AActor* SourceActor, AActor* DamageSourceActor, AActor* HitActor)
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
			if (HitActor == DamageSourceActor->GetOwner() || HitActor == DamageSourceActor->GetInstigator()
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

USkillActorFieldAction::USkillActorFieldAction()
{
	Settings.SpawnSocketNames.SetNum(6);
}

void USkillActorFieldAction::OnStart()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetAbility()->GetCurrentActorInfo();

	CleanupFieldTasks();
	SpawnedFieldActors.Reset();
	PendingFieldSocketNames.Reset();
	NextFieldSocketIndex = 0;
	bFieldStarted = false;
	MovementSpeedEffectHandle.Invalidate();
	if (!FieldTriggerDamage)
	{
		FieldTriggerDamage = NewObject<USkillTriggerDamage>(this);
	}
	FieldTriggerDamage->Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FieldSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(FieldRepeatSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(FieldEndTimerHandle);
	}

	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !SkillDataAsset)
	{
		Finish(false);
		return;
	}

	if (!Settings.FieldActorClass)
	{
		Finish(false);
		return;
	}

	FieldTriggerDamage->Configure(SkillDataAsset->Damage,
		FSkillTriggerHit::CreateUObject(this, &ThisClass::ApplyFieldTriggerDamage));

	StartFieldDurationMovementLockIfAllowed();
	StartWaitFieldMontageTriggerTask();

	if (!GetResolvedFieldMontage())
	{
		TryCommitAndStartField();
		return;
	}

	if (!StartFieldMontageTask())
	{
		TryCommitAndStartField();
		return;
	}
}

void USkillActorFieldAction::OnStop()
{
	CleanupFieldTasks();
	GetAbility()->RemoveActiveMovementSpeedBonus(MovementSpeedEffectHandle);

	TSet<AActor*> ActorsWithBoundDamageTriggers;
	for (AActor* SpawnedActor : SpawnedFieldActors)
	{
		if (FieldTriggerDamage && FieldTriggerDamage->IsBound(SpawnedActor))
		{
			ActorsWithBoundDamageTriggers.Add(SpawnedActor);
		}
	}

	if (FieldTriggerDamage)
	{
		FieldTriggerDamage->Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FieldSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(FieldRepeatSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(FieldEndTimerHandle);
	}
	FieldSpawnTimerHandle.Invalidate();
	FieldRepeatSpawnTimerHandle.Invalidate();
	FieldEndTimerHandle.Invalidate();

	const bool bDestroySpawnedActorsOnAbilityEnd =
		GetAbility()->HasDurationDeadline() || Settings.bDestroySpawnedActorsOnAbilityEnd;
	if (bDestroySpawnedActorsOnAbilityEnd || !SpawnedFieldActors.IsEmpty())
	{
		for (AActor* SpawnedActor : SpawnedFieldActors)
		{
			const bool bForceDestroyForSourceBuffActor = SpawnedActor && SpawnedActor->IsA<ASkillPowerUpActor>();
			const bool bForceDestroyForBoundDamageTrigger =
				SpawnedActor && ActorsWithBoundDamageTriggers.Contains(SpawnedActor);
			const bool bExpiresThroughConfiguredLifeSpan = Settings.bUseSpawnedActorLifeSpan
				&& Settings.SpawnedActorLifeSpan > 0.0;
			if (SpawnedActor && SpawnedActor->HasAuthority() && (bDestroySpawnedActorsOnAbilityEnd
				|| bForceDestroyForSourceBuffActor
				|| (bForceDestroyForBoundDamageTrigger && !bExpiresThroughConfiguredLifeSpan)))
			{
				DestroyFieldActorWhenReplicationIsSafe(SpawnedActor, Settings);
			}
		}
	}

	SpawnedFieldActors.Reset();
	PendingFieldSocketNames.Reset();
	NextFieldSocketIndex = 0;
	bFieldStarted = false;
}

UAnimMontage* USkillActorFieldAction::GetResolvedFieldMontage() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? SkillDataAsset->Animation.PrimaryMontage.Get() : nullptr;
}

FGameplayTag USkillActorFieldAction::GetResolvedFieldTriggerEventTag() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset && SkillDataAsset->Animation.PrimaryEventTag.IsValid()
		? SkillDataAsset->Animation.PrimaryEventTag
		: LabGameplayTags::Event_Montage_Trigger;
}

bool USkillActorFieldAction::StartFieldMontageTask()
{
	UAnimMontage* MontageToPlay = GetResolvedFieldMontage();
	if (!MontageToPlay)
	{
		return false;
	}

	FieldMontageTask = GetAbility()->CreateDefaultMontageAndWaitTask(MontageToPlay);
	if (!FieldMontageTask)
	{
		return false;
	}

	FieldMontageTask->OnCompleted.AddDynamic(this, &ThisClass::HandleFieldMontageFinished);
	FieldMontageTask->OnBlendOut.AddDynamic(this, &ThisClass::HandleFieldMontageFinished);
	FieldMontageTask->OnInterrupted.AddDynamic(this, &ThisClass::HandleFieldMontageInterrupted);
	FieldMontageTask->OnCancelled.AddDynamic(this, &ThisClass::HandleFieldMontageInterrupted);
	FieldMontageTask->ReadyForActivation();
	return true;
}

void USkillActorFieldAction::StartWaitFieldMontageTriggerTask()
{
	const FGameplayTag TriggerTag = GetResolvedFieldTriggerEventTag();
	if (!TriggerTag.IsValid())
	{
		return;
	}

	WaitFieldMontageTriggerTask = GetAbility()->CreateWaitGameplayEventTask(TriggerTag);
	if (!WaitFieldMontageTriggerTask)
	{
		return;
	}

	WaitFieldMontageTriggerTask->EventReceived.AddDynamic(this, &ThisClass::HandleFieldMontageTriggerEvent);
	WaitFieldMontageTriggerTask->ReadyForActivation();
}

void USkillActorFieldAction::TryCommitAndStartField()
{
	if (bFieldStarted || !(IsRunning() && GetAbility()->CanRunActions()))
	{
		return;
	}
	bFieldStarted = true;

	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		GetAbility()->StartConfiguredDefaultFX();
		if (!GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid() && !GetResolvedFieldMontage())
		{
			Finish();
		}
		return;
	}

	if (!GetAbility()->CommitSkill())
	{
		Finish(false);
		return;
	}

	GetAbility()->ApplyActiveMovementSpeedBonus(MovementSpeedEffectHandle);
	GetAbility()->StartConfiguredDefaultFX();
	GetAbility()->SpawnConfiguredCharacterDecal();
	StartFieldDurationMovementLockIfAllowed();
	StartFieldSpawnSequence();
	if (IsRunning())
	{
		StartFieldRepeatTimer();
	}
}

void USkillActorFieldAction::StartFieldDurationMovementLockIfAllowed()
{
	if (ShouldSkipFieldDurationMovementLock())
	{
		return;
	}

	GetAbility()->StartDurationMovementLock();
}

bool USkillActorFieldAction::ShouldSkipFieldDurationMovementLock() const
{
	return Settings.FieldActorClass && Settings.FieldActorClass.Get()->IsChildOf(ASkillPowerUpActor::StaticClass());
}


void USkillActorFieldAction::StartFieldSpawnSequence()
{
	PendingFieldSocketNames = PdSkillFieldPlacement::GetSpawnSocketNames(Settings);
	NextFieldSocketIndex = 0;

	if (PendingFieldSocketNames.IsEmpty())
	{
		ScheduleCompletion();
		return;
	}

	SpawnNextFieldActor();
}

void USkillActorFieldAction::StartFieldRepeatTimer()
{
	if (!ShouldRepeatFieldSpawnSequence())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float RepeatInterval = static_cast<float>(FMath::Max(Settings.RepeatSpawnInterval, 0.1));
	World->GetTimerManager().SetTimer(FieldRepeatSpawnTimerHandle, this, &ThisClass::HandleRepeatedFieldSpawnSequence,
		RepeatInterval, true);
}

void USkillActorFieldAction::SpawnNextFieldActor()
{
	if (!PendingFieldSocketNames.IsValidIndex(NextFieldSocketIndex))
	{
		FinishFieldSpawnSequence();
		return;
	}

	SpawnFieldActorForSocket(PendingFieldSocketNames[NextFieldSocketIndex]);

	++NextFieldSocketIndex;
	if (!PendingFieldSocketNames.IsValidIndex(NextFieldSocketIndex))
	{
		FinishFieldSpawnSequence();
		return;
	}

	const float SpawnInterval = static_cast<float>(FMath::Max(Settings.SpawnInterval, 0.0));
	if (SpawnInterval <= KINDA_SMALL_NUMBER)
	{
		SpawnNextFieldActor();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FieldSpawnTimerHandle, this, &ThisClass::SpawnNextFieldActor, SpawnInterval,
			false);
	}
}

void USkillActorFieldAction::FinishFieldSpawnSequence()
{
	PendingFieldSocketNames.Reset();
	NextFieldSocketIndex = 0;
	if (SpawnedFieldActors.IsEmpty())
	{
		Finish(false);
		return;
	}
	ScheduleCompletion();
}

AActor* USkillActorFieldAction::SpawnFieldActorForSocket(const FName SocketName)
{
	AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	UWorld* World = AvatarActor ? AvatarActor->GetWorld() : nullptr;
	if (!AvatarActor || !AvatarActor->HasAuthority() || !World || !Settings.FieldActorClass)
	{
		return nullptr;
	}

	const FTransform SpawnTransform = PdSkillFieldPlacement::ResolveSpawnTransform(*AvatarActor, Settings, SocketName);
	AActor* SpawnedActor = BeginDeferredFieldSpawn(*World, *AvatarActor, SpawnTransform);
	if (!SpawnedActor)
	{
		return nullptr;
	}

	const bool bShouldReplicateSpawnedActor = Settings.bForceReplicateSpawnedActor || SpawnedActor->IsA<ASkillEffectArea>();
	ConfigureSpawnedFieldActor(*SpawnedActor, *AvatarActor);
	UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);

	if (bShouldReplicateSpawnedActor)
	{
		// 지연 생성 중인 액터는 초기화 전이라 SetReplicates가 넷 드라이버에 등록하지 못한다.
		// FinishSpawning 전에 켜면 첫 복제를 조용히 놓칠 수 있어, 생성을 마친 뒤에 켠다.
		SpawnedActor->SetReplicates(true);
		SpawnedActor->SetReplicateMovement(true);
	}

	PdSkillFieldPlacement::AttachToSpawnSocket(*SpawnedActor, *AvatarActor, Settings, SocketName);
	if (bShouldReplicateSpawnedActor)
	{
		SpawnedActor->ForceNetUpdate();
	}
	ApplyFieldActorLifeSpan(*SpawnedActor, bShouldReplicateSpawnedActor);

	SpawnedFieldActors.Add(SpawnedActor);
	BindFieldTriggerDamage(SpawnedActor);
	return SpawnedActor;
}

AActor* USkillActorFieldAction::BeginDeferredFieldSpawn(UWorld& World, AActor& AvatarActor, const FTransform& SpawnTransform) const
{
	APawn* InstigatorPawn = Cast<APawn>(&AvatarActor);
	AActor* SpawnedActor = World.SpawnActorDeferred<AActor>(Settings.FieldActorClass, SpawnTransform, &AvatarActor, InstigatorPawn,
		Settings.SpawnCollisionHandling);
	if (!SpawnedActor && Settings.SpawnCollisionHandling != ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
	{
		SpawnedActor = World.SpawnActorDeferred<AActor>(Settings.FieldActorClass, SpawnTransform, &AvatarActor, InstigatorPawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	return SpawnedActor;
}

void USkillActorFieldAction::ConfigureSpawnedFieldActor(AActor& SpawnedActor, AActor& AvatarActor) const
{
	EEnum_Direction PandoraLoadoutDirection = EEnum_Direction::Center;
	if (const UPandoraSkillSource* PandoraSource = GetAbility()->GetPandoraSkillSource())
	{
		PandoraLoadoutDirection = PandoraSource->GetLoadoutDirection();
	}

	if (ASkillBlackHoleActor* BlackHoleActor = Cast<ASkillBlackHoleActor>(&SpawnedActor))
	{
		const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
		FSkillGameplayEffectConfig FinishDamageConfig;
		FinishDamageConfig.MagnitudeDataTag = LabGameplayTags::Data_Damage;
		if (SkillDataAsset)
		{
			FinishDamageConfig = SkillDataAsset->GetResolvedDamageConfig();
		}

		BlackHoleActor->ConfigureFromFieldSettings(Settings, FinishDamageConfig,
			FMath::Max(GetAbility()->GetAbilityLevel(), 1), PandoraLoadoutDirection);
	}

	if (ASkillPowerUpActor* PowerUpActor = Cast<ASkillPowerUpActor>(&SpawnedActor))
	{
		PowerUpActor->ConfigurePresentationSettings(Settings.PowerUpPresentation);
	}

	if (ASkillEffectArea* EffectArea = Cast<ASkillEffectArea>(&SpawnedActor))
	{
		EffectArea->SetSourceActor(&AvatarActor);
		EffectArea->SetIgnoreSourceActor(Settings.bIgnoreSourceActor || Settings.bEffectAreaIgnoreSourceActor);
		EffectArea->SetAffectEnemiesOnly(Settings.bEffectAreaAffectEnemiesOnly);
		EffectArea->SetSourcePandoraLoadoutDirection(PandoraLoadoutDirection);
	}
}

void USkillActorFieldAction::ApplyFieldActorLifeSpan(AActor& SpawnedActor, const bool bReplicated) const
{
	float RequestedLifeSpan = Settings.bUseSpawnedActorLifeSpan && Settings.SpawnedActorLifeSpan > 0.0
		? static_cast<float>(Settings.SpawnedActorLifeSpan)
		: SpawnedActor.GetLifeSpan();
	if (RequestedLifeSpan <= 0.0f)
	{
		return;
	}
	if (bReplicated)
	{
		RequestedLifeSpan = FMath::Max(RequestedLifeSpan, static_cast<float>(FMath::Max(Settings.MinimumReplicatedActorLifetime, 0.0)));
	}
	SpawnedActor.SetLifeSpan(RequestedLifeSpan);
}

void USkillActorFieldAction::DestroyFieldActorWhenReplicationIsSafe(AActor* SpawnedActor,
	const FSkillActorFieldSettings& FieldSettings) const
{
	if (!SpawnedActor || !SpawnedActor->HasAuthority())
	{
		return;
	}

	const float MinimumReplicatedLifetime = SpawnedActor->GetIsReplicated()
		? static_cast<float>(FMath::Max(FieldSettings.MinimumReplicatedActorLifetime, 0.0))
		: 0.0f;
	const float RemainingReplicationLifetime = FMath::Max(
		MinimumReplicatedLifetime - SpawnedActor->GetGameTimeSinceCreation(), 0.0f);
	if (!GetAbility()->HasDurationDeadline() && RemainingReplicationLifetime > KINDA_SMALL_NUMBER)
	{
		// 피해 델리게이트는 이미 지웠다. 애셋을 아직 읽지 않은 클라이언트가 읽어 만들 수 있을 만큼만
		// 복제되는 액터를 남겨 둔다.
		SpawnedActor->ForceNetUpdate();
		SpawnedActor->SetLifeSpan(RemainingReplicationLifetime);
		return;
	}

	SpawnedActor->Destroy();
}




bool USkillActorFieldAction::ShouldRepeatFieldSpawnSequence() const
{
	return GetAbility()->GetSourceSkillDataAsset() && Settings.bRepeatSpawnSequence
		&& GetAbility()->HasDurationDeadline() && GetAbility()->GetRemainingDuration() > 0.0f
		&& Settings.RepeatSpawnInterval > 0.0;
}


void USkillActorFieldAction::BindFieldTriggerDamage(AActor* SpawnedActor)
{
	if (!SpawnedActor || !SpawnedActor->HasAuthority())
	{
		return;
	}

	if (!HasConfiguredFieldTriggerDamage(GetAbility()->GetSourceSkillDataAsset()))
	{
		return;
	}

	if (SpawnedActor->IsA<ASkillBlackHoleActor>() && Settings.bBlackHoleApplyFinishAreaDamage)
	{
		return;
	}

	UPrimitiveComponent* TriggerComponent =
		USkillTriggerDamage::FindTriggerComponent(SpawnedActor, Settings.TriggerComponentName, true);
	if (!TriggerComponent)
	{
		return;
	}

	ConfigureFieldTriggerCollision(*TriggerComponent);
	FieldTriggerDamage->Bind(TriggerComponent, Settings.bDamageExistingOverlapsOnSpawn);
}

// 트리거 추적기가 피해 차례라고 알리면 대상과 피해를 확인해 적용한다. 피해를 시도했으면 true.
bool USkillActorFieldAction::ApplyFieldTriggerDamage(AActor* DamageSourceActor, AActor* HitActor)
{
	AActor* SourceActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (!SourceActor || !SourceActor->HasAuthority() || !IsValid(DamageSourceActor) || !IsValid(HitActor))
	{
		return false;
	}

	if (Settings.bIgnoreSourceActor && IsFieldSourceActorTarget(SourceActor, DamageSourceActor, HitActor))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = nullptr;
	UAbilitySystemComponent* TargetASC = nullptr;
	if (!ResolveDamageableCharacterTarget(SourceActor, HitActor, SourceASC, TargetASC))
	{
		return false;
	}

	const FGameplayEffectSpecHandle DamageSpecHandle =
		MakeFieldTriggerDamageSpec(DamageSourceActor, CalculateFieldTriggerDamageMagnitude());
	if (!DamageSpecHandle.IsValid())
	{
		return false;
	}

	ApplyDamageWithConfiguredStatus(*SourceASC, *TargetASC, *DamageSpecHandle.Data);
	return true;
}

FGameplayEffectSpecHandle USkillActorFieldAction::MakeFieldTriggerDamageSpec(AActor* DamageSourceActor, const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig TriggerDamage = SkillDataAsset ? SkillDataAsset->GetResolvedDamageConfig() : FSkillGameplayEffectConfig();
	if (!SkillDataAsset || !TriggerDamage.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{
		return FGameplayEffectSpecHandle();
	}

	return GetAbility()->MakeConfiguredDamageEffectSpec(TriggerDamage, DamageMagnitude, DamageSourceActor);
}

float USkillActorFieldAction::CalculateFieldTriggerDamageMagnitude() const
{
	const USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	return SkillDataAsset ? GetAbility()->CalculateDamageMagnitude(SkillDataAsset->GetResolvedDamageConfig()) : 0.0f;
}

void USkillActorFieldAction::ScheduleCompletion()
{
	if (ShouldRepeatFieldSpawnSequence())
	{
		return;
	}

	const float EndDelay = static_cast<float>(FMath::Max(Settings.TriggerActiveDurationAfterLastSpawn, 0.0));

	if (GetAbility()->HasDurationDeadline())
	{
		return;
	}

	if (EndDelay <= KINDA_SMALL_NUMBER)
	{
		Finish();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(FieldEndTimerHandle, this, &ThisClass::HandleFieldDurationFinished, EndDelay,
			false);
	}
}

void USkillActorFieldAction::HandleRepeatedFieldSpawnSequence()
{
	if (!ShouldRepeatFieldSpawnSequence())
	{
		return;
	}

	if (!PendingFieldSocketNames.IsEmpty())
	{
		return;
	}

	StartFieldSpawnSequence();
}

void USkillActorFieldAction::CleanupFieldTasks()
{
	if (FieldMontageTask)
	{
		FieldMontageTask->EndTask();
		FieldMontageTask = nullptr;
	}

	if (WaitFieldMontageTriggerTask)
	{
		WaitFieldMontageTriggerTask->EndTask();
		WaitFieldMontageTriggerTask = nullptr;
	}
}

void USkillActorFieldAction::HandleFieldMontageTriggerEvent(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	TryCommitAndStartField();
}

void USkillActorFieldAction::HandleFieldMontageFinished()
{
	FieldMontageTask = nullptr;

	if (bFieldStarted)
	{
		const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
		if ((!AvatarActor || !AvatarActor->HasAuthority()) && !GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid())
		{
			Finish();
		}
		return;
	}

	TryCommitAndStartField();
}

void USkillActorFieldAction::HandleFieldMontageInterrupted()
{
	FieldMontageTask = nullptr;

	if (bFieldStarted)
	{
		const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
		if ((!AvatarActor || !AvatarActor->HasAuthority()) && !GetAbility()->HasDurationDeadline() && !FieldEndTimerHandle.IsValid())
		{
			Finish();
		}
		return;
	}

	TryCommitAndStartField();
	const AActor* AvatarActor = GetAbility()->GetAvatarActorFromActorInfo();
	if (bFieldStarted && (!AvatarActor || !AvatarActor->HasAuthority()))
	{
		Finish();
	}
}

void USkillActorFieldAction::HandleFieldDurationFinished()
{
	Finish();
}


