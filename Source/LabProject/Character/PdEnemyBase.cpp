#include "Character/PdEnemyBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/Ability/RangedAttackAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AIController.h"
#include "Algo/RandomShuffle.h"
#include "Common/LabGameplayTags.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "UI/Widget/EnemyAvatarWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdEnemyBase)

DEFINE_LOG_CATEGORY_STATIC(LogPdEnemyBase, Log, All);

namespace
{
	bool IsDeadCharacter(const AActor* Actor)
	{
		const APdCharacterBase* Character = Cast<APdCharacterBase>(Actor);
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
	}

	bool IsValidEnemyAttackTarget(const APdEnemyBase* Enemy, const AActor* PotentialTarget)
	{
		if (!IsValid(Enemy) || !IsValid(PotentialTarget) || PotentialTarget == Enemy)
		{
			return false;
		}

		if (IsDeadCharacter(PotentialTarget))
		{
			return false;
		}

		const APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(PotentialTarget);
		if (TargetCharacter && Enemy->GetFactionId() != 0 && TargetCharacter->GetFactionId() == Enemy->GetFactionId())
		{
			return false;
		}

		return true;
	}
}

APdEnemyBase::APdEnemyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UPdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttackAbilityTags.AddTag(LabGameplayTags::Action_Attack);
	AttackAbilityTags.AddTag(LabGameplayTags::Action_RangedAttack);

	static ConstructorHelpers::FClassFinder<UUserWidget> EnemyAvatarWidgetFinder(TEXT("/Game/UI/Widget/WBP_EnemyAvatar"));
	if (EnemyAvatarWidgetFinder.Succeeded())
	{
		DefaultEnemyAvatarWidgetClass = EnemyAvatarWidgetFinder.Class;
		if (HealthBarWidget)
		{
			HealthBarWidget->SetWidgetClass(DefaultEnemyAvatarWidgetClass);
			HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
			HealthBarWidget->SetDrawAtDesiredSize(true);
			HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
		}
	}
}

void APdEnemyBase::BeginPlay()
{
	ConfigureEnemyAvatarWidget();

	Super::BeginPlay();
}

void APdEnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!HasAuthority() || !bStartCombatOnPossess)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(InitialCombatTimerHandle);
	GetWorldTimerManager().SetTimer(
		InitialCombatTimerHandle,
		this,
		&ThisClass::HandleInitialCombatDelayElapsed,
		InitialCombatDelay,
		false);

	UE_LOG(LogPdEnemyBase, Log, TEXT("Enemy combat scheduled: enemy=%s controller=%s delay=%.2f startingWeapon=%s tags=%s"),
		*GetNameSafe(this),
		*GetNameSafe(NewController),
		InitialCombatDelay,
		*StartingWeaponDefinition.ToString(),
		*AttackAbilityTags.ToStringSimple());
}

void APdEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(InitialCombatTimerHandle);
	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* APdEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void APdEnemyBase::HandleInitialCombatDelayElapsed()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bEquippedStartingWeapon = EquipStartingWeapon();
	UE_LOG(LogPdEnemyBase, Log, TEXT("Enemy initial combat delay elapsed: enemy=%s equippedStartingWeapon=%s currentWeapon=%s"),
		*GetNameSafe(this),
		bEquippedStartingWeapon ? TEXT("true") : TEXT("false"),
		GetEquipmentComponent() ? *GetNameSafe(GetEquipmentComponent()->GetCurrentWeaponActor()) : TEXT("None"));

	StartAttackTimer();
}

void APdEnemyBase::StartAttackTimer()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	if (bAttackImmediatelyAfterStart)
	{
		Attack();
	}

	if (AttackInterval <= 0.0f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ThisClass::Attack,
		AttackInterval,
		true,
		AttackInterval);

	UE_LOG(LogPdEnemyBase, Log, TEXT("Enemy attack timer started: enemy=%s interval=%.2f"),
		*GetNameSafe(this),
		AttackInterval);
}

bool APdEnemyBase::EquipStartingWeapon()
{
	if (!HasAuthority())
	{
		return false;
	}

	if (StartingWeaponDefinition.IsNull())
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("EquipStartingWeapon skipped: enemy=%s has no StartingWeaponDefinition."),
			*GetNameSafe(this));
		return false;
	}

	UEquipmentComponent* CurrentEquipmentComponent = GetEquipmentComponent();
	if (!CurrentEquipmentComponent)
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("EquipStartingWeapon failed: enemy=%s has no EquipmentComponent."),
			*GetNameSafe(this));
		return false;
	}

	UItemDefinition* LoadedWeaponDefinition = StartingWeaponDefinition.LoadSynchronous();
	if (!LoadedWeaponDefinition)
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("EquipStartingWeapon failed: enemy=%s could not load definition=%s"),
			*GetNameSafe(this),
			*StartingWeaponDefinition.ToString());
		return false;
	}

	const bool bEquipped = CurrentEquipmentComponent->EquipWeaponDefinition(LoadedWeaponDefinition);
	UE_LOG(LogPdEnemyBase, Log, TEXT("EquipStartingWeapon: enemy=%s definition=%s result=%s"),
		*GetNameSafe(this),
		*GetNameSafe(LoadedWeaponDefinition),
		bEquipped ? TEXT("true") : TEXT("false"));
	return bEquipped;
}

bool APdEnemyBase::MoveToAttackTarget(AActor* CurrentAttackTarget)
{
	if (!HasAuthority() || !bMoveToTargetBeforeAttack || !IsValidEnemyAttackTarget(this, CurrentAttackTarget))
	{
		return false;
	}

	const float Distance2D = FVector::Dist2D(GetActorLocation(), CurrentAttackTarget->GetActorLocation());
	if (Distance2D <= AttackRange)
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->StopMovement();
		}
		return false;
	}

	AAIController* AIController = Cast<AAIController>(GetController());
	if (!AIController)
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("MoveToAttackTarget failed: enemy=%s controller=%s is not AIController."),
			*GetNameSafe(this),
			*GetNameSafe(GetController()));
		return false;
	}

	const EPathFollowingRequestResult::Type MoveResult = AIController->MoveToActor(
		CurrentAttackTarget,
		MoveAcceptanceRadius,
		true,
		true,
		true,
		nullptr,
		true);

	UE_LOG(LogPdEnemyBase, Log, TEXT("MoveToAttackTarget: enemy=%s target=%s distance=%.1f attackRange=%.1f acceptance=%.1f result=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CurrentAttackTarget),
		Distance2D,
		AttackRange,
		MoveAcceptanceRadius,
		MoveResult == EPathFollowingRequestResult::RequestSuccessful ? TEXT("RequestSuccessful")
			: MoveResult == EPathFollowingRequestResult::AlreadyAtGoal ? TEXT("AlreadyAtGoal")
			: TEXT("Failed"));

	return MoveResult == EPathFollowingRequestResult::RequestSuccessful;
}

void APdEnemyBase::Attack()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("Attack failed: enemy=%s has no ASC."), *GetNameSafe(this));
		return;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->StopMovement();
		}
		UE_LOG(LogPdEnemyBase, Log, TEXT("Attack stopped: enemy=%s is dead."), *GetNameSafe(this));
		return;
	}

	AActor* CurrentAttackTarget = ITargetingInterface::Execute_GetAttackTarget(this);
	if (!IsValidEnemyAttackTarget(this, CurrentAttackTarget))
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			AIController->StopMovement();
		}
		UE_LOG(LogPdEnemyBase, Log, TEXT("Attack skipped: enemy=%s has no living attack target. cachedTarget=%s"),
			*GetNameSafe(this),
			*GetNameSafe(AttackTarget.Get()));
		return;
	}

	if (AController* CurrentController = GetController(); CurrentController && CurrentAttackTarget)
	{
		FRotator LookAtRotation = (CurrentAttackTarget->GetActorLocation() - GetActorLocation()).Rotation();
		LookAtRotation.Pitch = 0.0f;
		LookAtRotation.Roll = 0.0f;
		CurrentController->SetControlRotation(LookAtRotation);
		SetActorRotation(LookAtRotation);
	}

	if (MoveToAttackTarget(CurrentAttackTarget))
	{
		return;
	}

	UEquipmentComponent* CurrentEquipmentComponent = GetEquipmentComponent();
	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);

	const FGameplayAbilityActorInfo* ActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
	UE_LOG(LogPdEnemyBase, Log, TEXT("Enemy attack tick: enemy=%s target=%s currentWeapon=%s currentDefinition=%s abilityCount=%d tags=%s ownedTags=%s actorInfo=%s ownerActor=%s avatarActor=%s local=%s"),
		*GetNameSafe(this),
		*GetNameSafe(CurrentAttackTarget),
		CurrentEquipmentComponent ? *GetNameSafe(CurrentEquipmentComponent->GetCurrentWeaponActor()) : TEXT("None"),
		CurrentEquipmentComponent ? *GetNameSafe(CurrentEquipmentComponent->GetCurrentWeaponDefinition()) : TEXT("None"),
		AbilitySystemComponent->GetActivatableAbilities().Num(),
		*AttackAbilityTags.ToStringSimple(),
		*OwnedTags.ToStringSimple(),
		ActorInfo ? TEXT("valid") : TEXT("invalid"),
		ActorInfo ? *GetNameSafe(ActorInfo->OwnerActor.Get()) : TEXT("None"),
		ActorInfo ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"),
		ActorInfo && ActorInfo->IsLocallyControlled() ? TEXT("true") : TEXT("false"));

	if (AttackAbilityTags.IsEmpty())
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("Attack failed: enemy=%s has no AttackAbilityTags configured."), *GetNameSafe(this));
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	for (const FGameplayTag& AttackAbilityTag : AttackAbilityTags)
	{
		if (!AttackAbilityTag.IsValid())
		{
			continue;
		}

		FGameplayTagContainer SingleTagContainer;
		SingleTagContainer.AddTag(AttackAbilityTag);

		TArray<FGameplayAbilitySpecHandle> FoundHandles;
		AbilitySystemComponent->FindAllAbilitiesWithTags(FoundHandles, SingleTagContainer, false);
		for (const FGameplayAbilitySpecHandle& FoundHandle : FoundHandles)
		{
			AbilityHandles.AddUnique(FoundHandle);
		}
	}

	if (AbilityHandles.IsEmpty())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
		{
			const UGameplayAbility* AbilityCDO = AbilitySpec.Ability;
			const UClass* AbilityClass = AbilityCDO ? AbilityCDO->GetClass() : nullptr;
			if (AbilityClass
				&& (AbilityClass->IsChildOf(UAttackAbility::StaticClass())
					|| AbilityClass->IsChildOf(URangedAttackAbility::StaticClass())))
			{
				AbilityHandles.AddUnique(AbilitySpec.Handle);
			}
		}

		if (!AbilityHandles.IsEmpty())
		{
			UE_LOG(LogPdEnemyBase, Warning,
				TEXT("Attack found no abilities by tags=%s, using attack-class fallback. enemy=%s fallbackCount=%d"),
				*AttackAbilityTags.ToStringSimple(),
				*GetNameSafe(this),
				AbilityHandles.Num());
		}
	}

	if (AbilityHandles.IsEmpty())
	{
		UE_LOG(LogPdEnemyBase, Warning, TEXT("Attack failed: enemy=%s found no abilities for tags=%s."),
			*GetNameSafe(this),
			*AttackAbilityTags.ToStringSimple());

		for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
		{
			UE_LOG(LogPdEnemyBase, Log, TEXT("  Granted ability: class=%s tags=%s active=%s"),
				*GetNameSafe(AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr),
				AbilitySpec.Ability ? *AbilitySpec.Ability->GetAssetTags().ToStringSimple() : TEXT("None"),
				AbilitySpec.IsActive() ? TEXT("true") : TEXT("false"));
		}
		return;
	}

	Algo::RandomShuffle(AbilityHandles);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
		const UGameplayAbility* AbilityCDO = AbilitySpec ? AbilitySpec->Ability : nullptr;
		FGameplayTagContainer FailureTags;
		const bool bCanActivate = AbilityCDO && ActorInfo
			&& AbilityCDO->CanActivateAbility(AbilityHandle, ActorInfo, nullptr, nullptr, &FailureTags);
		const FString NetExecutionPolicyName = AbilityCDO
			? UEnum::GetValueAsString<EGameplayAbilityNetExecutionPolicy::Type>(AbilityCDO->GetNetExecutionPolicy())
			: TEXT("None");
		const FString InstancingPolicyName = AbilityCDO
			? UEnum::GetValueAsString<EGameplayAbilityInstancingPolicy::Type>(AbilityCDO->GetInstancingPolicy())
			: TEXT("None");

		UE_LOG(LogPdEnemyBase, Log,
			TEXT("Attack candidate: enemy=%s ability=%s assetTags=%s sourceTags=%s netPolicy=%s instancing=%s active=%s inputPressed=%s canActivate=%s failureTags=%s"),
			*GetNameSafe(this),
			AbilityCDO ? *GetNameSafe(AbilityCDO->GetClass()) : TEXT("None"),
			AbilityCDO ? *AbilityCDO->GetAssetTags().ToStringSimple() : TEXT("None"),
			AbilitySpec ? *AbilitySpec->GetDynamicSpecSourceTags().ToStringSimple() : TEXT("None"),
			*NetExecutionPolicyName,
			*InstancingPolicyName,
			AbilitySpec && AbilitySpec->IsActive() ? TEXT("true") : TEXT("false"),
			AbilitySpec && AbilitySpec->InputPressed ? TEXT("true") : TEXT("false"),
			bCanActivate ? TEXT("true") : TEXT("false"),
			*FailureTags.ToStringSimple());

		if (AbilitySystemComponent->TryActivateAbility(AbilityHandle, true))
		{
			UE_LOG(LogPdEnemyBase, Log, TEXT("Attack activated ability: enemy=%s ability=%s"),
				*GetNameSafe(this),
				AbilityCDO ? *GetNameSafe(AbilityCDO->GetClass()) : TEXT("None"));
			return;
		}

		UE_LOG(LogPdEnemyBase, Warning,
			TEXT("Attack candidate activation rejected: enemy=%s ability=%s netPolicy=%s canActivate=%s failureTags=%s"),
			*GetNameSafe(this),
			AbilityCDO ? *GetNameSafe(AbilityCDO->GetClass()) : TEXT("None"),
			*NetExecutionPolicyName,
			bCanActivate ? TEXT("true") : TEXT("false"),
			*FailureTags.ToStringSimple());
	}

	UE_LOG(LogPdEnemyBase, Warning, TEXT("Attack failed: enemy=%s found %d abilities but none activated."),
		*GetNameSafe(this),
		AbilityHandles.Num());
}

void APdEnemyBase::ConfigureEnemyAvatarWidget()
{
	if (!HealthBarWidget)
	{
		return;
	}

	UClass* CurrentWidgetClass = HealthBarWidget->GetWidgetClass();
	const bool bUsesNativeOnlyEnemyAvatar = CurrentWidgetClass == UEnemyAvatarWidget::StaticClass();
	const bool bMissingWidgetClass = CurrentWidgetClass == nullptr;

	if ((bMissingWidgetClass || bUsesNativeOnlyEnemyAvatar) && DefaultEnemyAvatarWidgetClass)
	{
		UE_LOG(LogPdEnemyBase, Log, TEXT("ConfigureEnemyAvatarWidget: replacing widget class on %s from %s to %s."),
			*GetNameSafe(this),
			*GetNameSafe(CurrentWidgetClass),
			*GetNameSafe(DefaultEnemyAvatarWidgetClass.Get()));
		HealthBarWidget->SetWidgetClass(DefaultEnemyAvatarWidgetClass);
	}
	else
	{
		UE_LOG(LogPdEnemyBase, Log, TEXT("ConfigureEnemyAvatarWidget: keeping widget class on %s as %s default=%s."),
			*GetNameSafe(this),
			*GetNameSafe(CurrentWidgetClass),
			*GetNameSafe(DefaultEnemyAvatarWidgetClass.Get()));
	}

	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawAtDesiredSize(true);
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HealthBarWidget->GetRelativeLocation().IsNearlyZero())
	{
		HealthBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	}
}

void APdEnemyBase::SetAttackTarget(AActor* InAttackTarget)
{
	AttackTarget = IsValidEnemyAttackTarget(this, InAttackTarget) ? InAttackTarget : nullptr;
}

AActor* APdEnemyBase::GetAttackTarget_Implementation() const
{
	if (IsValidEnemyAttackTarget(this, AttackTarget.Get()))
	{
		return AttackTarget.Get();
	}

	if (!bUsePlayerCharacterFallbackTarget)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	AActor* BestTarget = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();

	if (World)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			const APlayerController* PlayerController = Iterator->Get();
			APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
			if (!IsValidEnemyAttackTarget(this, PlayerPawn))
			{
				continue;
			}

			const double DistanceSq = FVector::DistSquared(GetActorLocation(), PlayerPawn->GetActorLocation());
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				BestTarget = PlayerPawn;
			}
		}
	}

	if (BestTarget)
	{
		return BestTarget;
	}

	AActor* FallbackTarget = UGameplayStatics::GetPlayerCharacter(this, 0);
	return IsValidEnemyAttackTarget(this, FallbackTarget) ? FallbackTarget : nullptr;
}
