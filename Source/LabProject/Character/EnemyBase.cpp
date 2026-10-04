#include "Character/EnemyBase.h"

#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/EnemyCombatComponent.h"
#include "Component/Character/EnemyTrainingBotComponent.h"
#include "Component/Player/CombatComponent.h"
#include "AbilitySystem/Ability/EquipmentAbilityData.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyBase)

DEFINE_LOG_CATEGORY_STATIC(LogEnemyBaseRuntime, Log, All);

AEnemyBase::AEnemyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnemyDefinition = TSoftObjectPtr<UEnemyBaseDefinition>(UEnemyBaseDefinition::GetDefaultDefinitionPath());

	AbilitySystemComponent = CreateDefaultSubobject<UPdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	BasicAttributeSet = CreateDefaultSubobject<UBasicAttributeSet>(TEXT("BasicAttributeSet"));

	EnemyCombatComponent = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("EnemyCombatComponent"));
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	EnemyTrainingBotComponent = CreateDefaultSubobject<UEnemyTrainingBotComponent>(TEXT("EnemyTrainingBotComponent"));
}

void AEnemyBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	ApplyEnemyDefinition();
	BeginEnemyDefinitionPreload();
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	TryInitializeCharacterRuntime();
}

void AEnemyBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsCharacterRuntimeInitialized() && EnemyCombatComponent)
	{
		EnemyCombatComponent->HandlePossessed();
	}
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EnemyDefinitionLease.Reset();
	bEnemyDefinitionReady = false;
	bEnemyRuntimeInitialized = false;
	LoadedEnemyDefinition = nullptr;

	if (EnemyTrainingBotComponent)
	{
		EnemyTrainingBotComponent->ShutdownRuntime();
	}
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->ShutdownRuntime();
	}

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* AEnemyBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void AEnemyBase::ApplyEnemyDefinition()
{
	LoadedEnemyDefinition = EnemyDefinition.Get();
	const UEnemyBaseDefinition* ResolvedDefinition = LoadedEnemyDefinition;
	if (!ResolvedDefinition)
	{
		ResolvedDefinition = GetDefault<UEnemyBaseDefinition>();
	}
	ApplyResolvedEnemyDefinition(ResolvedDefinition);

	FEnemyCombatSettings CombatSettings =
		ResolvedDefinition
			? ResolvedDefinition->GetCombatSettings()
			: FEnemyCombatSettings();
	if (ResolvedDefinition && CombatSettings.DefaultStatDefinition.IsNull())
	{
		CombatSettings.DefaultStatDefinition = ResolvedDefinition->GetEffectiveDefaultStatDefinition();
	}
	FEnemyTrainingBotSettings TrainingBotSettings =
		ResolvedDefinition
			? ResolvedDefinition->GetTrainingBotSettings()
			: FEnemyTrainingBotSettings();

	ModifyResolvedEnemySettings(CombatSettings, TrainingBotSettings);

	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->ApplySettings(CombatSettings);
	}
	if (EnemyTrainingBotComponent)
	{
		EnemyTrainingBotComponent->ApplySettings(TrainingBotSettings);
	}
}

void AEnemyBase::BeginEnemyDefinitionPreload()
{
	EnemyDefinitionLease.Reset();
	bEnemyDefinitionReady = false;

	if (EnemyDefinition.IsNull())
	{
		bEnemyDefinitionReady = true;
		return;
	}

	if (UEnemyBaseDefinition* LoadedDefinition = EnemyDefinition.Get())
	{
		LoadedEnemyDefinition = LoadedDefinition;
		bEnemyDefinitionReady = true;
		return;
	}

	EnemyDefinitionLease = UContentDataSubsystem::AcquireContent({EnemyDefinition.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEnemyDefinitionPreloaded));
}

void AEnemyBase::HandleEnemyDefinitionPreloaded()
{
	LoadedEnemyDefinition = EnemyDefinition.Get();
	bEnemyDefinitionReady = true;
	if (!LoadedEnemyDefinition)
	{
		UE_LOG(LogEnemyBaseRuntime, Error,
			TEXT("Enemy definition '%s' did not resolve after asynchronous preload; native defaults will be used."),
			*EnemyDefinition.ToString());
	}

	ApplyEnemyDefinition();
	TryInitializeCharacterRuntime();
}

bool AEnemyBase::IsAdditionalCharacterRuntimeContentReady() const
{
	return bEnemyDefinitionReady && EnemyCombatComponent && EnemyCombatComponent->IsRuntimeContentReady();
}

void AEnemyBase::HandleCharacterRuntimeInitialized()
{
	InitializeEnemyRuntime();
}

void AEnemyBase::InitializeEnemyRuntime()
{
	if (!bEnemyRuntimeInitialized)
	{
		bEnemyRuntimeInitialized = true;
		if (CombatComponent)
		{
			// 기본 자산은 기존 구성을 유지하되 전투 컴포넌트에는 공통 설정만 전달한다.
			if (const UPlayerPawnDefinition* Definition = Cast<UPlayerPawnDefinition>(UPlayerPawnDefinition::GetDefaultDefinitionPath().TryLoad()))
			{
				CombatComponent->ApplySettings(Definition->GetCombatDamageSettings(), Definition->GetUnarmedCombatSettings());
			}
		}
		if (EnemyTrainingBotComponent)
		{
			EnemyTrainingBotComponent->InitializeRuntime();
		}
		SetHealthBarVisibleForLocalViewer(false);
	}

	if (Controller && EnemyCombatComponent)
	{
		EnemyCombatComponent->HandlePossessed();
	}
}

void AEnemyBase::ModifyResolvedEnemySettings(FEnemyCombatSettings& CombatSettings,
	FEnemyTrainingBotSettings& TrainingBotSettings) const
{
	static_cast<void>(CombatSettings);
	static_cast<void>(TrainingBotSettings);
}

void AEnemyBase::ApplyResolvedEnemyDefinition(const UEnemyBaseDefinition* ResolvedDefinition)
{
	static_cast<void>(ResolvedDefinition);
}

void AEnemyBase::InitializeBehaviorTreeCombat()
{
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->InitializeBehaviorTreeCombat();
	}
}

bool AEnemyBase::IsActorValidAttackTarget(AActor* InActor) const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsActorValidAttackTarget(InActor);
}

float AEnemyBase::GetAttackDistanceToActor(const AActor* InActor) const
{
	return EnemyCombatComponent
		? EnemyCombatComponent->GetAttackDistanceToActor(InActor)
		: TNumericLimits<float>::Max();
}

float AEnemyBase::GetAttackStartDistance() const
{
	return EnemyCombatComponent ? EnemyCombatComponent->GetAttackStartDistance() : 0.0f;
}

bool AEnemyBase::IsUsingRangedWeapon() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsUsingRangedWeapon();
}

bool AEnemyBase::IsUsingGunWeapon() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsUsingGunWeapon();
}

bool AEnemyBase::RequestTrainingBotWeaponChange(const UItemDefinition* WeaponDefinition)
{
	return EnemyTrainingBotComponent && EnemyTrainingBotComponent->RequestWeaponChange(WeaponDefinition);
}

bool AEnemyBase::RequestTrainingBotUnarmed()
{
	return EnemyTrainingBotComponent && EnemyTrainingBotComponent->RequestUnarmed();
}

const UItemDefinition* AEnemyBase::GetCurrentOrStartingEnemyWeaponDefinition() const
{
	return EnemyCombatComponent ? EnemyCombatComponent->GetCurrentOrStartingEnemyWeaponDefinition() : nullptr;
}

bool AEnemyBase::GetFallbackAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();
	return false;
}

void AEnemyBase::HandleDamageTaken(const float DamageAmount, const bool bCriticalHit, const bool bAllowHitReact,
	AActor* DamageInstigator, AActor* DamageCauser)
{
	Super::HandleDamageTaken(DamageAmount, bCriticalHit, bAllowHitReact, DamageInstigator, DamageCauser);

	if (EnemyTrainingBotComponent)
	{
		EnemyTrainingBotComponent->HandleDamageTaken(DamageAmount, bAllowHitReact);
	}
}

void AEnemyBase::HandleDeath_Implementation()
{
	if (EnemyTrainingBotComponent && EnemyTrainingBotComponent->ShouldSuppressDeathHandling())
	{
		return;
	}

	Super::HandleDeath_Implementation();

	if (EnemyTrainingBotComponent)
	{
		EnemyTrainingBotComponent->HandleDeathAfterBase();
	}
}

void AEnemyBase::Attack()
{
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->Attack();
	}
}

void AEnemyBase::SetAttackEnabled(const bool bInAttackEnabled)
{
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->SetAttackEnabled(bInAttackEnabled);
	}
}

bool AEnemyBase::IsAttackEnabled() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsAttackEnabled();
}

bool AEnemyBase::IsAttackAbilityActive() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsAttackAbilityActive();
}

bool AEnemyBase::IsAttackInProgress() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsAttackInProgress();
}

bool AEnemyBase::MoveToAttackTarget(AActor* CurrentAttackTarget)
{
	return EnemyCombatComponent && EnemyCombatComponent->MoveToAttackTarget(CurrentAttackTarget);
}

bool AEnemyBase::RequestMoveToAttackTarget(AActor* InAttackTarget)
{
	return EnemyCombatComponent && EnemyCombatComponent->RequestMoveToAttackTarget(InAttackTarget);
}

bool AEnemyBase::IsTrainingHitStunned() const
{
	return EnemyTrainingBotComponent && EnemyTrainingBotComponent->IsHitStunned();
}

bool AEnemyBase::IsDefaultAttributeSetupComplete() const
{
	return EnemyCombatComponent && EnemyCombatComponent->IsDefaultAttributeSetupComplete();
}

void AEnemyBase::SetUseNearestPlayerWhenTargetUnset(const bool bInUseNearestPlayer)
{
	if (EnemyCombatComponent)
	{
		EnemyCombatComponent->SetUseNearestPlayerWhenTargetUnset(bInUseNearestPlayer);
	}
}

AActor* AEnemyBase::GetAttackTarget_Implementation() const
{
	return EnemyCombatComponent ? EnemyCombatComponent->ResolveAttackTarget() : nullptr;
}

void AEnemyBase::MulticastPlayTrainingHitReactMontage_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer || !EnemyTrainingBotComponent)
	{
		return;
	}
	EnemyTrainingBotComponent->PlayHitReactMontageLocal();
}

void AEnemyBase::MulticastPlayTrainingBotUnequipMontage_Implementation(UAnimMontage* UnequipMontage,
	const float PlayRate)
{
	if (GetNetMode() == NM_DedicatedServer || !EnemyTrainingBotComponent)
	{
		return;
	}
	EnemyTrainingBotComponent->PlayUnequipMontageLocal(UnequipMontage, PlayRate);
}

void AEnemyBase::MulticastResetTrainingBotRespawnVisuals_Implementation(const FTransform& RespawnTransform)
{
	if (EnemyTrainingBotComponent)
	{
		EnemyTrainingBotComponent->ResetRespawnVisualsLocal(RespawnTransform);
	}
}
