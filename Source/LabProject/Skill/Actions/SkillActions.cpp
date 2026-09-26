#include "Skill/Actions/SkillActions.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "Skill/Actors/SkillEffectArea.h"
#include "Skill/Actors/SkillProjectile.h"
#include "Skill/SkillGroundProjection.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/CharacterBase.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Pandora/PandoraSkillSource.h"
#include "TimerManager.h"
#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "Common/CollisionChannels.h"

void USkillWaitAction::OnStart()
{
	if (Seconds <= 0.0f) { Finish(); return; }
	if (!GetWorld()) { Finish(false); return; }
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::Elapsed, Seconds, false);
}
void USkillWaitAction::Elapsed() { Finish(); }
void USkillWaitAction::OnStop()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer);
}

void USkillWaitEventAction::OnStart()
{
	if (!GetAbility() || !GetWorld() || !EventTag.IsValid()) { Finish(false); return; }
	Task = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(GetAbility(), EventTag, nullptr, true, true);
	if (!Task) { Finish(false); return; }
	Task->EventReceived.AddDynamic(this, &ThisClass::Received);
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::TimedOut, FMath::Max(Timeout, 0.01f), false);
	Task->ReadyForActivation();
}
void USkillWaitEventAction::Received(FGameplayEventData Payload)
{
	FSkillActionContext Context = GetContext();
	Context.EventData = Payload;
	if (Payload.Target) Context.TargetActor = const_cast<AActor*>(Payload.Target.Get());
	if (Payload.TargetData.Num() > 0)
	{
		const FGameplayAbilityTargetData* Data = Payload.TargetData.Get(0);
		if (Data && Data->HasEndPoint()) Context.Transform = Data->GetEndPointTransform();
	}
	SetContext(Context);
	Finish();
}
void USkillWaitEventAction::TimedOut() { Finish(false); }
void USkillWaitEventAction::OnStop()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer);
	if (Task)
	{
		Task->EventReceived.RemoveAll(this);
		Task->EndTask();
		Task = nullptr;
	}
}

void USkillMontageAction::OnStart()
{
	if (!GetAbility() || !Montage) { Finish(false); return; }
	Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(GetAbility(), NAME_None, Montage);
	if (!Task) { Finish(false); return; }
	Task->OnCompleted.AddDynamic(this, &ThisClass::Completed);
	Task->OnInterrupted.AddDynamic(this, &ThisClass::Interrupted);
	Task->OnCancelled.AddDynamic(this, &ThisClass::Interrupted);
	Task->ReadyForActivation();
}
void USkillMontageAction::Completed() { Finish(); }
void USkillMontageAction::Interrupted() { Finish(false); }
void USkillMontageAction::OnStop()
{
	if (!Task) return;
	Task->OnCompleted.RemoveAll(this);
	Task->OnInterrupted.RemoveAll(this);
	Task->OnCancelled.RemoveAll(this);
	if (GetAbility() && GetAbility()->GetCurrentMontage() == Montage)
	{
		if (UAbilitySystemComponent* ASC = GetAbility()->GetAbilitySystemComponentFromActorInfo())
			ASC->CurrentMontageStop(0.1f);
	}
	Task->EndTask();
	Task = nullptr;
}

UAbilityTask_ApplyRootMotionConstantForce* USkillDashAction::CreateTask(UGameplayAbility* Ability,
	const FVector& Direction, float Speed, float Duration, float FinishSpeed, bool bEnableGravity)
{
	return UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(Ability, TEXT("Dash"),
		Direction, Speed, Duration, false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector, FinishSpeed, bEnableGravity);
}
void USkillDashAction::OnStart()
{
	ACharacterBase* Character = GetAbility() ? GetAbility()->GetPdCharacterFromActorInfo() : nullptr;
	if (!Character || Speed <= 0.0f || Duration <= 0.0f) { Finish(false); return; }
	FVector Direction = Character->GetLastMovementInputVector();
	if (const auto* Movement = Character->GetCharacterMovement(); Movement && !Movement->GetCurrentAcceleration().IsNearlyZero())
		Direction = Movement->GetCurrentAcceleration();
	for (int32 Index = 0; Index < GetContext().EventData.TargetData.Num(); ++Index)
	{
		const auto* Data = GetContext().EventData.TargetData.Get(Index);
		const auto* Hit = Data ? Data->GetHitResult() : nullptr;
		if (Hit && !Hit->Location.IsNearlyZero()) { Direction = Hit->Location; break; }
	}
	if (Direction.IsNearlyZero()) Direction = Character->GetActorForwardVector();
	Task = CreateTask(GetAbility(), Direction.GetSafeNormal2D(), Speed, Duration,
		Character->GetCharacterMovement()->GetMaxSpeed(), bEnableGravity);
	if (!Task) { Finish(false); return; }
	GetAbility()->GetPresentationManager().SpawnConfiguredCharacterDecal(*GetAbility());
	GetAbility()->StartMovementContactDamage();
	if (const auto* Skill = GetAbility()->GetSourceSkillDataAsset(); Skill && Skill->Niagara.GameplayCueTag.IsValid())
	{
		FGameplayCueParameters Cue;
		Cue.Location = Character->GetActorLocation();
		Cue.Instigator = Character;
		Cue.EffectCauser = Character;
		Cue.RawMagnitude = bHideCharacter ? 1.0f : -1.0f;
		GetAbility()->K2_AddGameplayCueWithParams(Skill->Niagara.GameplayCueTag, Cue, true);
	}
	Task->OnFinish.AddDynamic(this, &ThisClass::Completed);
	Task->ReadyForActivation();
}
void USkillDashAction::Completed()
{
	FSkillActionContext Context = GetContext();
	if (GetAbility() && GetAbility()->GetAvatarActorFromActorInfo())
		Context.Transform = GetAbility()->GetAvatarActorFromActorInfo()->GetActorTransform();
	SetContext(Context);
	Finish();
}
void USkillDashAction::OnStop()
{
	if (!Task) return;
	Task->OnFinish.RemoveAll(this);
	Task->EndTask();
	Task = nullptr;
}

void USkillGameplayEffectAction::OnStart()
{
	USkillAbility* Ability = GetAbility();
	if (Ability && !Ability->K2_HasAuthority()) { Finish(); return; }
	if (!Ability || !Ability->CanRunActions() || !Effect.GameplayEffectClass) { Finish(false); return; }
	AActor* Target = bApplyToSelf ? Ability->GetAvatarActorFromActorInfo() : GetContext().TargetActor.Get();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	FGameplayEffectSpecHandle Spec = Ability->MakeOutgoingGameplayEffectSpec(Effect.GameplayEffectClass, Ability->GetAbilityLevel());
	if (!TargetASC || !Spec.IsValid()) { Finish(false); return; }
	if (Effect.MagnitudeDataTag.IsValid()) Spec.Data->SetSetByCallerMagnitude(Effect.MagnitudeDataTag, static_cast<float>(Effect.Magnitude));
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	Finish();
}

void USkillAreaDamageAction::OnStart()
{
	USkillAbility* Ability = GetAbility();
	if (Ability && !Ability->K2_HasAuthority()) { Finish(); return; }
	ACharacterBase* Source = Ability ? Ability->GetPdCharacterFromActorInfo() : nullptr;
	if (!Source || !Ability->CanRunActions() || !GetWorld() || Radius <= 0.0f) { Finish(false); return; }
	const FGameplayEffectSpecHandle Spec = Ability->MakeActionDamageSpec(Damage);
	if (!Spec.IsValid()) { Finish(false); return; }
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SkillActionArea), false, Source);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetContext().Transform.GetLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Query);
	TSet<AActor*> Visited;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacterBase* Target = Cast<ACharacterBase>(Overlap.GetActor());
		if (!Target || Visited.Contains(Target) || !Source->CanDamageCharacterByTeam(Target)) continue;
		Visited.Add(Target);
		if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	Finish();
}

void USkillSpawnActorAction::OnStart()
{
	USkillAbility* Ability = GetAbility();
	if (Ability && !Ability->K2_HasAuthority()) { Finish(); return; }
	AActor* Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!Avatar || !Ability->CanRunActions() || !GetWorld() || !ActorClass) { Finish(false); return; }
	FTransform Transform = Offset * GetContext().Transform;
	if (bProjectToGround)
	{
		TArray<AActor*> Ignore { Avatar };
		PdSkillGroundProjection::FGroundProjectionResult Ground;
		if (PdSkillGroundProjection::TryProjectToGround(GetWorld(), Transform.GetLocation(),
			LabCollisionChannels::VisibilityTrace(), 100.0, 10000.0, Ignore, Ground))
			Transform.SetLocation(Ground.Location);
	}
	AActor* Spawned = GetWorld()->SpawnActorDeferred<AActor>(ActorClass, Transform, Avatar,
		Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Spawned) { Finish(false); return; }
	Spawned->SetReplicates(true);
	if (ASkillEffectArea* Area = Cast<ASkillEffectArea>(Spawned))
	{
		Area->SetSourcePandoraLoadoutDirection(Ability->GetPandoraSkillSource()->GetLoadoutDirection());
	}
	UGameplayStatics::FinishSpawningActor(Spawned, Transform);
	const float ActorLifeSpan = FMath::Max(LifeSpan, 0.01f);
	Spawned->SetLifeSpan(Ability->HasDurationDeadline() ? FMath::Min(ActorLifeSpan, Ability->GetRemainingDuration()) : ActorLifeSpan);
	Finish();
}

void USkillProjectileAction::OnStart()
{
	USkillAbility* Ability = GetAbility();
	if (Ability && !Ability->K2_HasAuthority()) { Finish(); return; }
	AActor* Avatar = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
	if (!Avatar || !Ability->CanRunActions() || !GetWorld() || !ProjectileClass || Speed <= 0.0f)
	{
		Finish(false);
		return;
	}
	const FVector Location = Avatar->GetActorTransform().TransformPosition(SpawnOffset);
	const AActor* Target = GetContext().TargetActor;
	const FVector TargetLocation = IsValid(Target) ? Target->GetActorLocation()
		: Location + Avatar->GetActorForwardVector() * 10000.0;
	const FTransform Transform((TargetLocation - Location).Rotation(), Location);
	Projectile = GetWorld()->SpawnActorDeferred<ASkillProjectile>(ProjectileClass, Transform, Avatar,
		Cast<APawn>(Avatar), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile) { Finish(false); return; }
	Projectile->OnSkillImpact.AddUObject(this, &ThisClass::Impacted);
	Projectile->OnDestroyed.AddDynamic(this, &ThisClass::ProjectileDestroyed);
	const FGameplayEffectSpecHandle DamageSpec = Ability->MakeActionDamageSpec(Damage);
	Projectile->SetDebuffEffectSpecHandle(Ability->MakeActionStatusSpec(), Ability->GetSourceSkillDataAsset()->StatusEffectDataAsset);
	Projectile->PrepareProjectile(DamageSpec);
	UGameplayStatics::FinishSpawningActor(Projectile, Transform);
	if (!IsRunning() || !IsValid(Projectile)) return;
	const float FlightLifeSpan = FMath::Max(MaxFlightSeconds, 0.01f);
	Projectile->SetLifeSpan(Ability->HasDurationDeadline() ? FMath::Min(FlightLifeSpan, Ability->GetRemainingDuration()) : FlightLifeSpan);
	Projectile->LaunchProjectile(TargetLocation, Speed, DamageSpec);
}
void USkillProjectileAction::Impacted(AActor* Target, const FHitResult& Hit)
{
	if (!IsRunning()) return;
	if (Projectile)
	{
		Projectile->OnSkillImpact.RemoveAll(this);
		Projectile->OnDestroyed.RemoveAll(this);
	}
	Projectile = nullptr; // 충돌 이후 표현의 수명은 투사체 자체가 관리한다.
	FSkillActionContext Context = GetContext();
	Context.TargetActor = Target;
	const FQuat Rotation = Hit.ImpactNormal.IsNearlyZero() ? FQuat::Identity
		: FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat();
	Context.Transform = FTransform(Rotation, Hit.ImpactPoint);
	SetContext(Context);
	if (OnImpact)
	{
		OnImpact->OnFinished.AddUObject(this, &ThisClass::ImpactActionFinished);
		OnImpact->Start(GetAbility(), Context);
	}
	else Finish();
}
void USkillProjectileAction::ImpactActionFinished(USkillAction* Child, bool bSucceeded) { Finish(bSucceeded); }
void USkillProjectileAction::ProjectileDestroyed(AActor* Actor)
{
	Projectile = nullptr;
	Finish(); // 수명 만료로 빗나간 경우 충돌 기능은 실행하지 않는다.
}
void USkillProjectileAction::OnStop()
{
	if (Projectile)
	{
		Projectile->OnSkillImpact.RemoveAll(this);
		Projectile->OnDestroyed.RemoveAll(this);
		Projectile->Destroy();
		Projectile = nullptr;
	}
	if (OnImpact)
	{
		OnImpact->OnFinished.RemoveAll(this);
		OnImpact->Cancel();
	}
}

void USkillRepeatAction::OnStart()
{
	CompletedCount = 0;
	if (!Action || !GetWorld()) { Finish(false); return; }
	RunNext();
}
void USkillRepeatAction::RunNext()
{
	Current = DuplicateObject<USkillAction>(Action, this);
	Current->OnFinished.AddUObject(this, &ThisClass::ChildFinished);
	Current->Start(GetAbility(), GetContext());
}
void USkillRepeatAction::ChildFinished(USkillAction* Child, bool bSucceeded)
{
	if (!IsRunning()) return;
	if (!bSucceeded) { Finish(false); return; }
	if (Count > 0 && ++CompletedCount >= Count) { Finish(); return; }
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ThisClass::RunNext, FMath::Max(Interval, 0.01f), false);
}
void USkillRepeatAction::OnStop()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Timer);
	if (Current)
	{
		Current->OnFinished.RemoveAll(this);
		Current->Cancel();
		Current = nullptr;
	}
}
