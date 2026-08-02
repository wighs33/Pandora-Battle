#include "AI/MonsterCharacter.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/EquipmentAbilityData.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "Definition/Item/RewardDefinition.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Logging/PdLogRateLimiter.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/LevelingComponent.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "UObject/ConstructorHelpers.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogMonsterReward, Log, All);

namespace
{
	constexpr double MissingMonsterRewardLogIntervalSeconds = 30.0;
	FPdLogRateLimiter MissingMonsterRewardLogLimiter;
}

AMonsterCharacter::AMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CharacterDefinition =
		TSoftObjectPtr<UCharacterBaseDefinition>(
			UCharacterBaseDefinition::
				GetDefaultDefinitionPath());

	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Keep the legacy facade defaults aligned with the native monster
	// settings so existing Blueprint overrides can still be detected and
	// migrated into the focused components during PreInitializeComponents.
	bStartCombatOnPossess = false;
	bUseBehaviorTreeCombat = false;
	bEnableTrainingBotHitReaction = false;

	ContactDamageDataTag = LabGameplayTags::Data_Damage;

	static ConstructorHelpers::FClassFinder<UGameplayEffect> IncomingDamageEffectFinder(
		TEXT("/Game/GAS/Effect/GE_IncomingDamage"));
	if (IncomingDamageEffectFinder.Succeeded())
	{
		ContactDamageEffectClass = IncomingDamageEffectFinder.Class;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DeathMontageFinder(
		TEXT("/Game/StackOBot/Characters/Blobling/Anim/AM_Baddy_Death"));
	if (DeathMontageFinder.Succeeded())
	{
		DeathMontage = DeathMontageFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> HitReactMontageFinder(
		TEXT("/Game/StackOBot/Characters/Blobling/Anim/AM_Baddy_Hit"));
	if (HitReactMontageFinder.Succeeded())
	{
		MonsterHitReactMontage = HitReactMontageFinder.Object;
	}
}

void AMonsterCharacter::ModifyResolvedEnemySettings(
	FEnemyCombatSettings& CombatSettings,
	FEnemyTrainingBotSettings& TrainingBotSettings) const
{
	Super::ModifyResolvedEnemySettings(
		CombatSettings,
		TrainingBotSettings);

	CombatSettings.bAttackEnabled = true;
	CombatSettings.bStartCombatOnPossess = false;
	CombatSettings.bUseBehaviorTreeCombat = false;
	CombatSettings.bAttackImmediatelyAfterStart = false;
	TrainingBotSettings.bRespawnOnDeath = false;
	TrainingBotSettings.bEnableHitReaction = false;
}

#if WITH_EDITOR
EDataValidationResult AMonsterCharacter::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (MonsterRewardDefinition.IsNull())
	{
		Context.AddError(FText::FromString(
			FString::Printf(
				TEXT("%s requires MonsterRewardDefinition. Monster rewards have no runtime fallback."),
				*GetPathName())));
		return EDataValidationResult::Invalid;
	}

	if (!MonsterRewardDefinition.LoadSynchronous())
	{
		Context.AddError(FText::FromString(
			FString::Printf(
				TEXT("%s references an unloadable MonsterRewardDefinition: %s"),
				*GetPathName(),
				*MonsterRewardDefinition.ToString())));
		return EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

void AMonsterCharacter::Attack()
{
	if (TryPlayMonsterAttackMontage())
	{
		return;
	}

	Super::Attack();
}

bool AMonsterCharacter::IsAttackInProgress() const
{
	if (bAttackRequestedWhileContentLoading || Super::IsAttackInProgress())
	{
		return true;
	}

	UAnimMontage* AttackMontage = MonsterAttackMontage.Get();
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return IsValid(AttackMontage)
		&& IsValid(AnimInstance)
		&& AnimInstance->Montage_IsPlaying(AttackMontage);
}

void AMonsterCharacter::BeginPlay()
{
	Super::BeginPlay();

	BeginMonsterContentPreload();
	DeactivateDamageSphere();

	if (UPrimitiveComponent* AttackComponent = ResolveAttackComponent())
	{
		AttackComponent->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&ThisClass::HandleAttackComponentBeginOverlap);
	}
	DeactivateAttackSphere();
}

void AMonsterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	BeginMonsterContentPreload();
	if (HasAuthority() && !IsDefaultAttributeSetupComplete())
	{
		InitializeBehaviorTreeCombat();
	}

	ApplyMonsterHealthDefaults();
}

void AMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackSphereTimerHandle);
	GetWorldTimerManager().ClearTimer(DeathDestroyTimerHandle);
	ReleaseMonsterContentPreload();

	DeactivateDamageSphere();

	if (UPrimitiveComponent* AttackComponent = ResolveAttackComponent())
	{
		AttackComponent->OnComponentBeginOverlap.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AMonsterCharacter::HandleDamageTaken(
	float DamageAmount,
	bool bCriticalHit,
	bool bAllowHitReact,
	AActor* DamageInstigator,
	AActor* DamageCauser)
{
	RememberDamageSource(DamageInstigator, DamageCauser);
	Super::HandleDamageTaken(DamageAmount, bCriticalHit, bAllowHitReact, DamageInstigator, DamageCauser);
	TryPlayMonsterHitReactMontage(DamageAmount, bAllowHitReact);
}

FVector AMonsterCharacter::GetDamageIndicatorWorldLocation() const
{
	const FVector DefaultIndicatorLocation = Super::GetDamageIndicatorWorldLocation();
	const float HalfHeightZ = GetActorLocation().Z
		+ (DefaultIndicatorLocation.Z - GetActorLocation().Z) * 0.5f;
	return FVector(DefaultIndicatorLocation.X, DefaultIndicatorLocation.Y, HalfHeightZ);
}

void AMonsterCharacter::ApplyMonsterHealthDefaults()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	const float ClampedMaxHealth = FMath::Max(MonsterMaxHealth, 1.0f);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetMaxHealthAttribute(),
		ClampedMaxHealth);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetHealthAttribute(),
		ClampedMaxHealth);
	RefreshHealthBarViewModel();
}

void AMonsterCharacter::BeginMonsterContentPreload()
{
	if (bMonsterContentPreloadStarted)
	{
		return;
	}
	bMonsterContentPreloadStarted = true;

	TArray<FSoftObjectPath> AssetPaths;
	if (!MonsterAttackMontage.IsNull())
	{
		AssetPaths.Add(MonsterAttackMontage.ToSoftObjectPath());
	}
	if (!MonsterHitReactMontage.IsNull())
	{
		AssetPaths.Add(MonsterHitReactMontage.ToSoftObjectPath());
	}
	if (!MonsterRewardDefinition.IsNull())
	{
		AssetPaths.Add(MonsterRewardDefinition.ToSoftObjectPath());
	}

	if (AssetPaths.IsEmpty())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bMonsterContentPreloadStarted = false;
		UE_LOG(
			LogMonsterReward,
			Error,
			TEXT("Monster content preload could not start for '%s': ContentDataSubsystem is unavailable."),
			*GetPathName());
		return;
	}

	bMonsterContentLoadPending = true;
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	MonsterContentPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			AssetPaths,
			FSimpleDelegate::CreateLambda(
				[WeakThis]()
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleMonsterContentPreloadComplete();
					}
				}));
}

void AMonsterCharacter::HandleMonsterContentPreloadComplete()
{
	if (!bMonsterContentLoadPending)
	{
		return;
	}
	bMonsterContentLoadPending = false;

	if (!MonsterAttackMontage.IsNull() && !MonsterAttackMontage.Get())
	{
		UE_LOG(
			LogMonsterReward,
			Error,
			TEXT("Monster attack montage '%s' did not resolve after preload for '%s'."),
			*MonsterAttackMontage.ToString(),
			*GetPathName());
	}
	if (!MonsterHitReactMontage.IsNull() && !MonsterHitReactMontage.Get())
	{
		UE_LOG(
			LogMonsterReward,
			Error,
			TEXT("Monster hit-react montage '%s' did not resolve after preload for '%s'."),
			*MonsterHitReactMontage.ToString(),
			*GetPathName());
	}

	const bool bShouldRetryAttack = bAttackRequestedWhileContentLoading;
	bAttackRequestedWhileContentLoading = false;
	if (bShouldRetryAttack && HasAuthority() && !bDying)
	{
		Attack();
	}

	if (bDefeatRewardGrantPending)
	{
		GrantDefeatRewards();
	}

	if (bDestroyAfterContentLoad && !bDefeatRewardGrantPending)
	{
		bDestroyAfterContentLoad = false;
		Destroy();
	}
}

void AMonsterCharacter::ReleaseMonsterContentPreload()
{
	bMonsterContentLoadPending = false;
	bAttackRequestedWhileContentLoading = false;
	bDefeatRewardGrantPending = false;
	bDestroyAfterContentLoad = false;

	if (MonsterContentPreloadHandle.IsValid())
	{
		MonsterContentPreloadHandle->CancelHandle();
		MonsterContentPreloadHandle->ReleaseHandle();
		MonsterContentPreloadHandle.Reset();
	}
}

void AMonsterCharacter::HandleDeath_Implementation()
{
	BeginMonsterDeath();
	Super::HandleDeath_Implementation();
}

bool AMonsterCharacter::GetFallbackAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();

	UAnimMontage* LoadedAttackMontage = ResolveMonsterAttackMontage();
	if (!LoadedAttackMontage)
	{
		return false;
	}

	OutAttackData.AttackMontage = LoadedAttackMontage;
	return true;
}

UAnimMontage* AMonsterCharacter::ResolveMonsterAttackMontage() const
{
	return MonsterAttackMontage.Get();
}

UAnimMontage* AMonsterCharacter::ResolveMonsterHitReactMontage() const
{
	return MonsterHitReactMontage.Get();
}

bool AMonsterCharacter::TryPlayMonsterAttackMontage()
{
	if (!HasAuthority() || bDying || !IsAttackEnabled())
	{
		return false;
	}

	if (!AbilitySystemComponent || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return false;
	}

	if (IsTrainingHitStunned())
	{
		return false;
	}

	AActor* CurrentAttackTarget = ITargetingInterface::Execute_GetAttackTarget(this);
	if (!IsActorValidAttackTarget(CurrentAttackTarget))
	{
		return false;
	}

	if (!IsStatusFrozen())
	{
		FVector ToTarget = CurrentAttackTarget->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.0f;
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator LookAtRotation = ToTarget.Rotation();
			if (AController* CurrentController = GetController())
			{
				CurrentController->SetControlRotation(LookAtRotation);
			}
			SetActorRotation(LookAtRotation);
		}
	}

	if (MoveToAttackTarget(CurrentAttackTarget))
	{
		return true;
	}

	UAnimMontage* AttackMontage = ResolveMonsterAttackMontage();
	if (!AttackMontage)
	{
		if (bMonsterContentLoadPending && !MonsterAttackMontage.IsNull())
		{
			bAttackRequestedWhileContentLoading = true;
			return true;
		}
		return false;
	}

	bAttackRequestedWhileContentLoading = false;
	ActivateAttackSphere();
	MulticastPlayMonsterAttackMontage(AttackMontage, 1.0f);
	return true;
}

void AMonsterCharacter::TryPlayMonsterHitReactMontage(const float DamageAmount, const bool bAllowHitReact)
{
	if (!HasAuthority() || bDying || !bAllowHitReact || DamageAmount <= 0.0f)
	{
		return;
	}

	if (!AbilitySystemComponent || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	if (!AttributeSet || AttributeSet->GetHealth() <= DamageAmount)
	{
		return;
	}

	UAnimMontage* HitReactMontage = ResolveMonsterHitReactMontage();
	if (!HitReactMontage)
	{
		return;
	}

	MulticastPlayMonsterHitReactMontage(HitReactMontage, MonsterHitReactPlayRate);
}

void AMonsterCharacter::HandleAttackComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	if (!HasAuthority() || bDying || !IsValidMonsterDamageTarget(OtherActor))
	{
		return;
	}

	ApplyAttackDamageToCharacter(Cast<ACharacterBase>(OtherActor));
}

UPrimitiveComponent* AMonsterCharacter::ResolveDamageComponent() const
{
	return ResolvePrimitiveComponentByName(DamageComponentName);
}

UPrimitiveComponent* AMonsterCharacter::ResolveAttackComponent() const
{
	return ResolvePrimitiveComponentByName(AttackComponentName);
}

UPrimitiveComponent* AMonsterCharacter::ResolvePrimitiveComponentByName(FName ComponentName) const
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetFName() == ComponentName)
		{
			return PrimitiveComponent;
		}
	}

	const FString ConfiguredName = ComponentName.ToString();
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetName().StartsWith(ConfiguredName))
		{
			return PrimitiveComponent;
		}
	}

	return nullptr;
}

bool AMonsterCharacter::IsValidMonsterDamageTarget(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return false;
	}

	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(OtherActor);
	if (!TargetCharacter || !CanDamageCharacterByTeam(TargetCharacter))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = TargetCharacter->GetAbilitySystemComponent();
	return TargetASC && !TargetASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
}

void AMonsterCharacter::ApplyAttackDamageToCharacter(ACharacterBase* TargetCharacter)
{
	if (!IsValid(TargetCharacter) || AttackHitActorsThisSwing.Contains(TargetCharacter))
	{
		return;
	}

	AttackHitActorsThisSwing.Add(TargetCharacter);
	ApplyMonsterDamageToCharacter(TargetCharacter, AttackDamageMagnitude, 0.0f);
}

bool AMonsterCharacter::ApplyMonsterDamageToCharacter(
	ACharacterBase* TargetCharacter,
	float DamageMagnitude,
	float KnockbackStrength)
{
	if (!HasAuthority()
		|| !IsValid(TargetCharacter)
		|| !ContactDamageEffectClass
		|| DamageMagnitude <= 0.0f)
	{
		return false;
	}

	UPdAbilitySystemComponent* SourceASC = GetEnemyAbilitySystemComponent();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter->GetPdAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(this, this);
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		ContactDamageEffectClass,
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	FGameplayTag DamageDataTag = ContactDamageDataTag;
	if (!DamageDataTag.IsValid())
	{
		SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageDataTag);
	}

	if (!DamageDataTag.IsValid())
	{
		return false;
	}

	UBasicAttributeSet* TargetAttributeSet =
		const_cast<UBasicAttributeSet*>(TargetASC->GetSet<UBasicAttributeSet>());
	const bool bSuppressPlayerHitReact = TargetCharacter->IsA<APdPlayer>();
	if (bSuppressPlayerHitReact && TargetAttributeSet)
	{
		TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(false);
	}

	SpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	if (bSuppressPlayerHitReact && TargetAttributeSet)
	{
		TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(true);
	}

	if (KnockbackStrength > 0.0f)
	{
		FVector KnockbackDirection = TargetCharacter->GetActorLocation() - GetActorLocation();
		KnockbackDirection.Z = 0.0f;
		KnockbackDirection = KnockbackDirection.GetSafeNormal();
		if (!KnockbackDirection.IsNearlyZero())
		{
			TargetCharacter->LaunchCharacter(KnockbackDirection * KnockbackStrength, true, false);
		}
	}

	return true;
}

void AMonsterCharacter::DeactivateDamageSphere()
{
	if (UPrimitiveComponent* DamageComponent = ResolveDamageComponent())
	{
		DamageComponent->OnComponentBeginOverlap.RemoveAll(this);
		DamageComponent->SetGenerateOverlapEvents(false);
		DamageComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AMonsterCharacter::ActivateAttackSphere()
{
	if (!HasAuthority() || bDying || AttackDamageMagnitude <= 0.0f)
	{
		return;
	}

	UPrimitiveComponent* AttackComponent = ResolveAttackComponent();
	if (!AttackComponent)
	{
		return;
	}

	AttackHitActorsThisSwing.Reset();
	AttackComponent->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandleAttackComponentBeginOverlap);
	AttackComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AttackComponent->SetGenerateOverlapEvents(true);
	AttackComponent->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	AttackComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (IsValidMonsterDamageTarget(OverlappingActor))
		{
			ApplyAttackDamageToCharacter(Cast<ACharacterBase>(OverlappingActor));
		}
	}

	GetWorldTimerManager().ClearTimer(AttackSphereTimerHandle);
	GetWorldTimerManager().SetTimer(
		AttackSphereTimerHandle,
		this,
		&ThisClass::DeactivateAttackSphere,
		FMath::Max(AttackSphereActiveDuration, 0.01f),
		false);
}

void AMonsterCharacter::DeactivateAttackSphere()
{
	GetWorldTimerManager().ClearTimer(AttackSphereTimerHandle);

	if (UPrimitiveComponent* AttackComponent = ResolveAttackComponent())
	{
		AttackComponent->SetGenerateOverlapEvents(false);
		AttackComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	AttackHitActorsThisSwing.Reset();
}

void AMonsterCharacter::BeginMonsterDeath()
{
	if (bDying)
	{
		return;
	}

	GrantDefeatRewards();

	bDying = true;
	SetAttackEnabled(false);
	DeactivateAttackSphere();
	GetWorldTimerManager().ClearTimer(DeathDestroyTimerHandle);
	DeactivateDamageSphere();
	StartDeathDissolve(FMath::Max(DeathDestroyDelay, 0.01f));

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (HasAuthority())
	{
		MulticastPlayMonsterDeathPresentation();

		GetWorldTimerManager().SetTimer(
			DeathDestroyTimerHandle,
			this,
			&ThisClass::FinishMonsterDeath,
			FMath::Max(DeathDestroyDelay, 0.01f),
			false);
	}
}

void AMonsterCharacter::FinishMonsterDeath()
{
	if (bDefeatRewardGrantPending && bMonsterContentLoadPending)
	{
		bDestroyAfterContentLoad = true;
		return;
	}

	Destroy();
}

void AMonsterCharacter::RememberDamageSource(AActor* DamageInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return;
	}

	if (IsValid(DamageInstigator) && DamageInstigator != this)
	{
		LastDamageInstigator = DamageInstigator;
	}

	if (IsValid(DamageCauser) && DamageCauser != this)
	{
		LastDamageCauser = DamageCauser;
	}
}

APdPlayerState* AMonsterCharacter::ResolveRewardPlayerState() const
{
	if (APdPlayerState* ResolvedPlayerState = ResolvePlayerStateFromActor(LastDamageInstigator.Get()))
	{
		return ResolvedPlayerState;
	}

	return ResolvePlayerStateFromActor(LastDamageCauser.Get());
}

APdPlayerState* AMonsterCharacter::ResolvePlayerStateFromActor(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (APdPlayerState* ActorPlayerState = Cast<APdPlayerState>(Actor))
	{
		return ActorPlayerState;
	}

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (APdPlayerState* PawnPlayerState = Pawn->GetPlayerState<APdPlayerState>())
		{
			return PawnPlayerState;
		}
	}

	if (AController* ActorController = Cast<AController>(Actor))
	{
		if (APdPlayerState* ControllerPlayerState = Cast<APdPlayerState>(ActorController->PlayerState))
		{
			return ControllerPlayerState;
		}
	}

	if (APawn* InstigatorPawn = Actor->GetInstigator())
	{
		if (APdPlayerState* InstigatorPlayerState = InstigatorPawn->GetPlayerState<APdPlayerState>())
		{
			return InstigatorPlayerState;
		}
	}

	AActor* OwnerActor = Actor->GetOwner();
	return OwnerActor && OwnerActor != Actor ? ResolvePlayerStateFromActor(OwnerActor) : nullptr;
}

const URewardDefinition* AMonsterCharacter::GetMonsterRewardDefinition() const
{
	if (MonsterRewardDefinition.IsNull())
	{
		uint32 SuppressedCount = 0;
		if (MissingMonsterRewardLogLimiter.TryAcquire(
			MissingMonsterRewardLogIntervalSeconds,
			SuppressedCount))
		{
			UE_LOG(
				LogMonsterReward,
				Error,
				TEXT("[MonsterReward] Required MonsterRewardDefinition is not configured for '%s'. "
					"No defeat reward will be granted. SuppressedSinceLast=%u"),
				*GetPathName(),
				SuppressedCount);
		}
		return nullptr;
	}

	if (const URewardDefinition* LoadedRewardDefinition = MonsterRewardDefinition.Get())
	{
		return LoadedRewardDefinition;
	}

	if (bMonsterContentLoadPending)
	{
		return nullptr;
	}

	uint32 SuppressedCount = 0;
	if (MissingMonsterRewardLogLimiter.TryAcquire(
		MissingMonsterRewardLogIntervalSeconds,
		SuppressedCount))
	{
		UE_LOG(
			LogMonsterReward,
			Error,
			TEXT("[MonsterReward] Required reward definition '%s' could not be loaded for '%s'. "
				"No defeat reward will be granted. SuppressedSinceLast=%u"),
			*MonsterRewardDefinition.ToString(),
			*GetPathName(),
			SuppressedCount);
	}
	return nullptr;
}

void AMonsterCharacter::GrantDefeatRewards()
{
	if (!HasAuthority() || bDefeatRewardsGranted)
	{
		return;
	}

	APdPlayerState* RewardPlayerState = ResolveRewardPlayerState();
	if (!RewardPlayerState)
	{
		bDefeatRewardGrantPending = false;
		bDefeatRewardsGranted = true;
		return;
	}

	const URewardDefinition* RewardDefinition = GetMonsterRewardDefinition();
	if (!RewardDefinition)
	{
		if (bMonsterContentLoadPending && !MonsterRewardDefinition.IsNull())
		{
			bDefeatRewardGrantPending = true;
			return;
		}

		bDefeatRewardGrantPending = false;
		bDefeatRewardsGranted = true;
		return;
	}

	bDefeatRewardGrantPending = false;
	bDefeatRewardsGranted = true;

	int32 GrantedExperience = RewardDefinition->RollMonsterDefeatExperienceReward();
	if (GrantedExperience > 0)
	{
		ULevelingComponent* LevelingComponent = RewardPlayerState->GetLevelingComponent();
		if (!LevelingComponent || !LevelingComponent->GrantRewardExperience(GrantedExperience))
		{
			GrantedExperience = 0;
		}
	}

	int32 GrantedSoulDust = RewardDefinition->RollMonsterDefeatSoulDustReward();
	if (GrantedSoulDust > 0)
	{
		UPandoraTreeComponent* PandoraTreeComponent = RewardPlayerState->GetPandoraTreeComponent();
		if (!PandoraTreeComponent || !PandoraTreeComponent->AddSoulDust(GrantedSoulDust))
		{
			GrantedSoulDust = 0;
		}
	}

	UPlayerNotificationComponent* NotificationComponent = RewardPlayerState->GetPlayerNotificationComponent();
	if (!NotificationComponent)
	{
		return;
	}

	if (GrantedExperience > 0)
	{
		NotificationComponent->SendExperienceRewardNotification(
			static_cast<float>(GrantedExperience),
			RewardDefinition->Notification.ExperienceIcon);
	}

	if (GrantedSoulDust > 0)
	{
		NotificationComponent->SendSoulDustRewardNotification(
			GrantedSoulDust,
			RewardDefinition->Notification.SoulDustIcon);
	}
}

void AMonsterCharacter::MulticastPlayMonsterAttackMontage_Implementation(UAnimMontage* AttackMontage, float PlayRate)
{
	if (!AttackMontage)
	{
		return;
	}

	PlayAnimMontage(AttackMontage, PlayRate);
}

void AMonsterCharacter::MulticastPlayMonsterHitReactMontage_Implementation(UAnimMontage* HitReactMontage, float PlayRate)
{
	if (!HitReactMontage || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PlayAnimMontage(HitReactMontage, FMath::Max(PlayRate, 0.01f));
}

void AMonsterCharacter::MulticastPlayMonsterDeathPresentation_Implementation()
{
	if (DeathMontage)
	{
		PlayAnimMontage(DeathMontage);
	}
}
