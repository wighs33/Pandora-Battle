#include "Component/Character/EnemyCombatComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Algo/RandomShuffle.h"
#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/Ability/PunchAbility.h"
#include "AbilitySystem/Ability/RangedAttackAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Character/EnemyBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/EnemyTrainingBotComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Weapon/Gun.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyCombatComponent)

DEFINE_LOG_CATEGORY_STATIC(LogEnemyCombatComponent, Log, All);

namespace
{
bool IsValidEnemyAttackTarget(
	const AEnemyBase* Enemy,
	const AActor* PotentialTarget)
{
	if (!IsValid(Enemy)
		|| !IsValid(PotentialTarget)
		|| PotentialTarget == Enemy)
	{
		return false;
	}

	const ACharacterBase* TargetCharacter =
		Cast<ACharacterBase>(PotentialTarget);
	if (TargetCharacter && TargetCharacter->IsDead())
	{
		return false;
	}
	if (TargetCharacter
		&& Enemy->GetFactionId() != 0
		&& TargetCharacter->GetFactionId() == Enemy->GetFactionId())
	{
		return false;
	}

	return true;
}

FGameplayTag ResolveEnemyAttackAbilityTag(
	const bool bUsingRangedWeapon,
	const bool bHasEquippedWeapon)
{
	if (bUsingRangedWeapon)
	{
		return LabGameplayTags::Action_RangedAttack;
	}

	return bHasEquippedWeapon
		? LabGameplayTags::Action_Attack
		: LabGameplayTags::Action_Punch;
}

bool IsAbilityClassCompatibleWithAttackMode(
	const UClass* AbilityClass,
	const bool bUsingRangedWeapon,
	const bool bHasEquippedWeapon)
{
	if (!AbilityClass)
	{
		return false;
	}

	if (bUsingRangedWeapon)
	{
		return AbilityClass->IsChildOf(URangedAttackAbility::StaticClass());
	}

	const bool bIsPunchAbility =
		AbilityClass->IsChildOf(UPunchAbility::StaticClass());
	if (!bHasEquippedWeapon)
	{
		return bIsPunchAbility;
	}

	return AbilityClass->IsChildOf(UAttackAbility::StaticClass())
		&& !bIsPunchAbility;
}
} // namespace

UEnemyCombatComponent::UEnemyCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	const UEnemyBaseDefinition* NativeDefaults =
		GetDefault<UEnemyBaseDefinition>();
	if (NativeDefaults)
	{
		Settings = NativeDefaults->GetCombatSettings();
	}
}

void UEnemyCombatComponent::ApplySettings(
	const FEnemyCombatSettings& InSettings)
{
	Settings = InSettings;
	const auto SanitizeNonNegative =
		[](const float Value)
		{
			return FMath::IsFinite(Value)
				? FMath::Max(Value, 0.0f)
				: 0.0f;
		};
	Settings.InitialCombatDelay =
		SanitizeNonNegative(Settings.InitialCombatDelay);
	Settings.AttackInterval =
		SanitizeNonNegative(Settings.AttackInterval);
	Settings.AttackStartDistance =
		SanitizeNonNegative(Settings.AttackStartDistance);
	Settings.RangedAttackStartDistance =
		SanitizeNonNegative(Settings.RangedAttackStartDistance);
	bDefaultStatDefinitionApplied = false;
	BeginRuntimeContentPreload();
}

void UEnemyCombatComponent::BeginRuntimeContentPreload()
{
	ReleaseRuntimeContentPreload();
	bRuntimeContentReady = false;

	TArray<FSoftObjectPath> AssetPaths;
	if (!Settings.DefaultStatDefinition.IsNull())
	{
		AssetPaths.AddUnique(Settings.DefaultStatDefinition.ToSoftObjectPath());
	}
	if (!Settings.StartingWeaponDefinition.IsNull())
	{
		AssetPaths.AddUnique(Settings.StartingWeaponDefinition.ToSoftObjectPath());
	}

	AssetPaths.RemoveAll(
		[](const FSoftObjectPath& AssetPath)
		{
			return AssetPath.ResolveObject() != nullptr;
		});
	if (AssetPaths.IsEmpty())
	{
		bRuntimeContentReady = true;
		return;
	}

	const uint32 RequestGeneration = RuntimeContentLoadGeneration;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			AssetPaths,
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					HandleRuntimeContentPreloaded(RequestGeneration);
				}));

	if (NewLoadHandle.IsValid()
		&& RequestGeneration == RuntimeContentLoadGeneration
		&& !bRuntimeContentReady)
	{
		RuntimeContentLoadHandle = MoveTemp(NewLoadHandle);
	}
	else if (NewLoadHandle.IsValid())
	{
		NewLoadHandle->ReleaseHandle();
	}
	else
	{
		HandleRuntimeContentPreloaded(RequestGeneration);
	}
}

void UEnemyCombatComponent::HandleRuntimeContentPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != RuntimeContentLoadGeneration)
	{
		return;
	}

	bRuntimeContentReady = true;
	if (!Settings.DefaultStatDefinition.IsNull()
		&& !Settings.DefaultStatDefinition.Get())
	{
		UE_LOG(
			LogEnemyCombatComponent,
			Error,
			TEXT("Enemy stat definition '%s' did not resolve after asynchronous preload."),
			*Settings.DefaultStatDefinition.ToString());
	}
	if (!Settings.StartingWeaponDefinition.IsNull()
		&& !Settings.StartingWeaponDefinition.Get())
	{
		UE_LOG(
			LogEnemyCombatComponent,
			Error,
			TEXT("Enemy starting weapon '%s' did not resolve after asynchronous preload."),
			*Settings.StartingWeaponDefinition.ToString());
	}

	if (AEnemyBase* Enemy = GetEnemyOwner())
	{
		Enemy->TryInitializeCharacterRuntime();
	}

	if (bHandlePossessedWhenContentReady)
	{
		HandlePossessed();
	}
}

void UEnemyCombatComponent::ReleaseRuntimeContentPreload()
{
	++RuntimeContentLoadGeneration;
	if (RuntimeContentLoadHandle.IsValid())
	{
		RuntimeContentLoadHandle->CancelHandle();
		RuntimeContentLoadHandle->ReleaseHandle();
		RuntimeContentLoadHandle.Reset();
	}
}

AEnemyBase* UEnemyCombatComponent::GetEnemyOwner() const
{
	return Cast<AEnemyBase>(GetOwner());
}

const AEnemyBase* UEnemyCombatComponent::GetEnemyOwnerConst() const
{
	return Cast<AEnemyBase>(GetOwner());
}

void UEnemyCombatComponent::HandlePossessed()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}
	if (!bRuntimeContentReady)
	{
		bHandlePossessedWhenContentReady = true;
		return;
	}
	bHandlePossessedWhenContentReady = false;

	Enemy->InitializeAbilitySystemActorInfo();
	EnsureDefaultAttributeSetup();

	if (Settings.bUseBehaviorTreeCombat)
	{
		InitializeBehaviorTreeCombat();
		return;
	}

	if (!Settings.bStartCombatOnPossess)
	{
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(InitialCombatTimerHandle);
	Enemy->GetWorldTimerManager().SetTimer(
		InitialCombatTimerHandle,
		this,
		&ThisClass::HandleInitialCombatDelayElapsed,
		Settings.InitialCombatDelay,
		false);
}

void UEnemyCombatComponent::ShutdownRuntime()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy)
	{
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(InitialCombatTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	AttackTarget = nullptr;
	bHandlePossessedWhenContentReady = false;
	ReleaseRuntimeContentPreload();
}

void UEnemyCombatComponent::InitializeBehaviorTreeCombat()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}
	if (!bRuntimeContentReady)
	{
		bHandlePossessedWhenContentReady = true;
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(InitialCombatTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(AttackTimerHandle);

	Enemy->InitializeAbilitySystemActorInfo();
	EnsureDefaultAttributeSetup();
	if (!Settings.StartingWeaponDefinition.IsNull())
	{
		EquipStartingWeapon();
	}
}

void UEnemyCombatComponent::HandleInitialCombatDelayElapsed()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}
	if (!bRuntimeContentReady)
	{
		bHandlePossessedWhenContentReady = true;
		return;
	}

	Enemy->InitializeAbilitySystemActorInfo();
	EnsureDefaultAttributeSetup();
	EquipStartingWeapon();
	StartAttackTimer();
}

void UEnemyCombatComponent::StartAttackTimer()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(AttackTimerHandle);
	if (!Settings.bAttackEnabled)
	{
		return;
	}

	if (Settings.bAttackImmediatelyAfterStart)
	{
		Enemy->Attack();
	}

	if (Settings.AttackInterval <= 0.0f)
	{
		return;
	}

	FTimerDelegate AttackDelegate;
	AttackDelegate.BindWeakLambda(
		Enemy,
		[WeakEnemy = TWeakObjectPtr<AEnemyBase>(Enemy)]()
		{
			if (AEnemyBase* ResolvedEnemy = WeakEnemy.Get())
			{
				ResolvedEnemy->Attack();
			}
		});
	Enemy->GetWorldTimerManager().SetTimer(
		AttackTimerHandle,
		AttackDelegate,
		Settings.AttackInterval,
		true,
		Settings.AttackInterval);
}

bool UEnemyCombatComponent::IsActorValidAttackTarget(
	const AActor* InActor) const
{
	return IsValidEnemyAttackTarget(GetEnemyOwnerConst(), InActor);
}

void UEnemyCombatComponent::SetAttackTarget(AActor* InAttackTarget)
{
	AttackTarget = IsActorValidAttackTarget(InAttackTarget)
		? InAttackTarget
		: nullptr;
}

AActor* UEnemyCombatComponent::GetCachedAttackTarget() const
{
	return IsActorValidAttackTarget(AttackTarget.Get())
		? AttackTarget.Get()
		: nullptr;
}

void UEnemyCombatComponent::SetUseNearestPlayerWhenTargetUnset(
	const bool bInUseNearestPlayer)
{
	Settings.bUseNearestPlayerWhenTargetUnset = bInUseNearestPlayer;
}

AActor* UEnemyCombatComponent::ResolveAttackTarget() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy)
	{
		return nullptr;
	}

	if (AActor* CachedTarget = GetCachedAttackTarget())
	{
		return CachedTarget;
	}

	if (!Settings.bUseNearestPlayerWhenTargetUnset)
	{
		return nullptr;
	}

	UWorld* World = Enemy->GetWorld();
	AActor* BestTarget = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();
	if (World)
	{
		for (FConstPlayerControllerIterator Iterator =
				World->GetPlayerControllerIterator();
			Iterator;
			++Iterator)
		{
			const APlayerController* PlayerController = Iterator->Get();
			APawn* PlayerPawn =
				PlayerController ? PlayerController->GetPawn() : nullptr;
			if (!IsActorValidAttackTarget(PlayerPawn))
			{
				continue;
			}

			const double DistanceSq = FVector::DistSquared(
				Enemy->GetActorLocation(),
				PlayerPawn->GetActorLocation());
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

	AActor* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(Enemy, 0);
	return IsActorValidAttackTarget(PlayerCharacter)
		? PlayerCharacter
		: nullptr;
}

float UEnemyCombatComponent::GetAttackDistanceToActor(
	const AActor* InActor) const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy || !IsValid(InActor))
	{
		return TNumericLimits<float>::Max();
	}

	const float CenterDistance2D = FVector::Dist2D(
		Enemy->GetActorLocation(),
		InActor->GetActorLocation());

	float SelfRadius = 0.0f;
	float SelfHalfHeight = 0.0f;
	Enemy->GetSimpleCollisionCylinder(SelfRadius, SelfHalfHeight);

	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	InActor->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);

	return FMath::Max(
		CenterDistance2D - SelfRadius - TargetRadius,
		0.0f);
}

float UEnemyCombatComponent::GetAttackStartDistance() const
{
	return IsUsingRangedWeapon()
		? Settings.RangedAttackStartDistance
		: Settings.AttackStartDistance;
}

bool UEnemyCombatComponent::IsUsingRangedWeapon() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UEquipmentComponent* Equipment =
		Enemy ? Enemy->GetEquipmentComponent() : nullptr;
	const AWeaponBase* Weapon =
		Equipment ? Equipment->GetCurrentWeaponActor() : nullptr;
	if (Weapon && Weapon->SupportsAimInput())
	{
		return true;
	}

	const UItemDefinition* WeaponDefinition =
		Equipment ? Equipment->GetCurrentWeaponDefinition() : nullptr;
	return WeaponDefinition
		&& WeaponDefinition->WeaponData.Aim.bSupportsInput;
}

bool UEnemyCombatComponent::IsUsingGunWeapon() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UEquipmentComponent* Equipment =
		Enemy ? Enemy->GetEquipmentComponent() : nullptr;
	const AWeaponBase* Weapon =
		Equipment ? Equipment->GetCurrentWeaponActor() : nullptr;
	if (Weapon && Weapon->IsA<AGun>())
	{
		return true;
	}

	const UItemDefinition* WeaponDefinition =
		Equipment ? Equipment->GetCurrentWeaponDefinition() : nullptr;
	return WeaponDefinition && WeaponDefinition->WeaponData.Gun.HasAnyData();
}

void UEnemyCombatComponent::EnsureDefaultAttributeSetup()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!Enemy || !Enemy->HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	if (!AbilitySystemComponent->GetAttributeSet(
			UBasicAttributeSet::StaticClass()))
	{
		UBasicAttributeSet* BasicAttributeSet =
			NewObject<UBasicAttributeSet>(
				Enemy,
				UBasicAttributeSet::StaticClass(),
				TEXT("EnemyBasicAttributeSet"));
		if (BasicAttributeSet)
		{
			AbilitySystemComponent->AddSpawnedAttribute(BasicAttributeSet);
		}
	}

	if (DefaultAttributeConfigHandle == INDEX_NONE)
	{
		FAttributeConfig AttributeConfig;
		const auto AddMapping =
			[&AttributeConfig](
				const FGameplayTag& StatTag,
				const FGameplayAttribute& Attribute)
			{
				if (!StatTag.IsValid() || !Attribute.IsValid())
				{
					return;
				}

				FAttributeTagMapping Mapping;
				Mapping.StatTag = StatTag;
				Mapping.Attribute = Attribute;
				AttributeConfig.AttributeMappings.Add(Mapping);
			};

		AddMapping(LabGameplayTags::Status_Offense_Strength, UBasicAttributeSet::GetStrengthAttribute());
		AddMapping(LabGameplayTags::Status_Offense_StrengthLevel, UBasicAttributeSet::GetStrengthLevelAttribute());
		AddMapping(LabGameplayTags::Status_Offense_Intelligence, UBasicAttributeSet::GetIntelligenceAttribute());
		AddMapping(LabGameplayTags::Status_Offense_IntelligenceLevel, UBasicAttributeSet::GetIntelligenceLevelAttribute());
		AddMapping(LabGameplayTags::Status_Offense_Critical, UBasicAttributeSet::GetCriticalAttribute());
		AddMapping(LabGameplayTags::Status_Offense_CriticalLevel, UBasicAttributeSet::GetCriticalLevelAttribute());
		AddMapping(LabGameplayTags::Status_Defense_Armor, UBasicAttributeSet::GetArmorAttribute());
		AddMapping(LabGameplayTags::Status_Defense_ArmorLevel, UBasicAttributeSet::GetArmorLevelAttribute());
		AddMapping(LabGameplayTags::Status_Defense_Recovery, UBasicAttributeSet::GetRecoveryAttribute());
		AddMapping(LabGameplayTags::Status_Defense_RecoveryLevel, UBasicAttributeSet::GetRecoveryLevelAttribute());
		AddMapping(LabGameplayTags::Status_Defense_MaxShield, UBasicAttributeSet::GetMaxShieldAttribute());
		AddMapping(LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute());
		AddMapping(LabGameplayTags::Status_Defense_MaxShieldLevel, UBasicAttributeSet::GetMaxShieldLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_Frostbite, UBasicAttributeSet::GetFrostbiteAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_FrostbiteLevel, UBasicAttributeSet::GetFrostbiteLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_Burn, UBasicAttributeSet::GetBurnAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_BurnLevel, UBasicAttributeSet::GetBurnLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_ElectricShock, UBasicAttributeSet::GetElectricShockAttribute());
		AddMapping(LabGameplayTags::Status_Resistance_ElectricShockLevel, UBasicAttributeSet::GetElectricShockLevelAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_FirstPandora, UBasicAttributeSet::GetFirstPandoraAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_FirstPandoraLevel, UBasicAttributeSet::GetFirstPandoraLevelAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_SecondPandora, UBasicAttributeSet::GetSecondPandoraAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_SecondPandoraLevel, UBasicAttributeSet::GetSecondPandoraLevelAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_ThirdPandora, UBasicAttributeSet::GetThirdPandoraAttribute());
		AddMapping(LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel, UBasicAttributeSet::GetThirdPandoraLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resource_Health, UBasicAttributeSet::GetHealthAttribute());
		AddMapping(LabGameplayTags::Status_Resource_Mana, UBasicAttributeSet::GetManaAttribute());
		AddMapping(LabGameplayTags::Status_Resource_Stamina, UBasicAttributeSet::GetStaminaAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxHealth, UBasicAttributeSet::GetMaxHealthAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxHealthLevel, UBasicAttributeSet::GetMaxHealthLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxMana, UBasicAttributeSet::GetMaxManaAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxManaIncreasePercent, UBasicAttributeSet::GetMaxManaIncreasePercentAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxManaLevel, UBasicAttributeSet::GetMaxManaLevelAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxStamina, UBasicAttributeSet::GetMaxStaminaAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute());
		AddMapping(LabGameplayTags::Status_Resource_MaxStaminaLevel, UBasicAttributeSet::GetMaxStaminaLevelAttribute());
		AddMapping(LabGameplayTags::Status_Agility_AttackSpeed, UBasicAttributeSet::GetAttackSpeedAttribute());
		AddMapping(LabGameplayTags::Status_Agility_AttackSpeedLevel, UBasicAttributeSet::GetAttackSpeedLevelAttribute());
		AddMapping(LabGameplayTags::Status_Agility_MovementSpeed, UBasicAttributeSet::GetMovementSpeedAttribute());
		AddMapping(LabGameplayTags::Status_Agility_MovementSpeedLevel, UBasicAttributeSet::GetMovementSpeedLevelAttribute());
		AddMapping(LabGameplayTags::Status_Agility_Arcane, UBasicAttributeSet::GetArcaneAttribute());
		AddMapping(LabGameplayTags::Status_Agility_ArcaneLevel, UBasicAttributeSet::GetArcaneLevelAttribute());

		DefaultAttributeConfigHandle =
			AbilitySystemComponent->AddAttributeConfig(AttributeConfig);
	}

	ApplyDefaultStatDefinition();
}

bool UEnemyCombatComponent::ApplyDefaultStatDefinition()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| !AbilitySystemComponent
		|| bDefaultStatDefinitionApplied
		|| Settings.DefaultStatDefinition.IsNull())
	{
		return false;
	}

	const UStatUpgradeDefinition* LoadedStatDefinition =
		Settings.DefaultStatDefinition.Get();
	if (!LoadedStatDefinition
		|| LoadedStatDefinition->GetAttributeDefaultValues().IsEmpty())
	{
		return false;
	}

	TArray<FStatAttributeDefaultValue> OrderedDefaults =
		LoadedStatDefinition->GetAttributeDefaultValues();
	OrderedDefaults.StableSort(
		[](const FStatAttributeDefaultValue& Left,
			const FStatAttributeDefaultValue& Right)
		{
			return Left.Priority < Right.Priority;
		});

	bool bAppliedAny = false;
	for (const FStatAttributeDefaultValue& AttributeDefault : OrderedDefaults)
	{
		if (!AttributeDefault.IsValid())
		{
			continue;
		}

		FGameplayAttribute Attribute;
		if (AbilitySystemComponent->ResolveAttributeFromTag(
				AttributeDefault.StatTag,
				Attribute))
		{
			bAppliedAny |= AbilitySystemComponent->ApplyAttributeDefaultValue(
				Attribute,
				AttributeDefault.DefaultValue);
		}
	}

	for (const FPairedResourceStatTag& Pair :
		LoadedStatDefinition->GetPairedResourceStatTags())
	{
		if (!Pair.IsValid())
		{
			continue;
		}

		float ExplicitCurrentDefault = 0.0f;
		if (LoadedStatDefinition->TryGetExactAttributeDefaultValue(
				Pair.CurrentStatTag,
				ExplicitCurrentDefault))
		{
			continue;
		}

		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		if (!AbilitySystemComponent->ResolveAttributeFromTag(
				Pair.MaxStatTag,
				MaxAttribute)
			|| !AbilitySystemComponent->ResolveAttributeFromTag(
				Pair.CurrentStatTag,
				CurrentAttribute))
		{
			continue;
		}

		const float MaxValue =
			AbilitySystemComponent->GetNumericAttribute(MaxAttribute);
		if (MaxValue > UE_KINDA_SMALL_NUMBER)
		{
			bAppliedAny |= AbilitySystemComponent->ApplyAttributeDefaultValue(
				CurrentAttribute,
				MaxValue);
		}
	}

	bDefaultStatDefinitionApplied = true;
	if (bAppliedAny)
	{
		Enemy->RefreshHealthBarViewModel();
	}

	return bAppliedAny;
}

bool UEnemyCombatComponent::EquipStartingWeapon()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| Settings.StartingWeaponDefinition.IsNull())
	{
		return false;
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	if (!Equipment)
	{
		return false;
	}

	UItemDefinition* WeaponDefinition =
		Settings.StartingWeaponDefinition.Get();
	return WeaponDefinition
		&& Equipment->EquipWeaponDefinition(WeaponDefinition);
}

bool UEnemyCombatComponent::EquipEnemyWeaponDefinition(
	const UItemDefinition* WeaponDefinition)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority() || !WeaponDefinition)
	{
		return false;
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	if (!Equipment)
	{
		return false;
	}

	Enemy->InitializeAbilitySystemActorInfo();
	EnsureDefaultAttributeSetup();
	const bool bEquipped =
		Equipment->EquipWeaponDefinition(WeaponDefinition);
	if (bEquipped)
	{
		Settings.StartingWeaponDefinition =
			TSoftObjectPtr<UItemDefinition>(
				FSoftObjectPath(WeaponDefinition->GetPathName()));
	}
	return bEquipped;
}

const UItemDefinition*
UEnemyCombatComponent::GetCurrentOrStartingEnemyWeaponDefinition() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (const UEquipmentComponent* Equipment =
			Enemy ? Enemy->GetEquipmentComponent() : nullptr)
	{
		if (const UItemDefinition* WeaponDefinition =
				Equipment->GetCurrentWeaponDefinition())
		{
			return WeaponDefinition;
		}
	}

	return Settings.StartingWeaponDefinition.IsNull()
		? nullptr
		: Settings.StartingWeaponDefinition.Get();
}

void UEnemyCombatComponent::ClearStartingWeaponDefinition()
{
	Settings.StartingWeaponDefinition.Reset();
}

void UEnemyCombatComponent::ResetAttributesForRespawn()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!Enemy || !Enemy->HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	EnsureDefaultAttributeSetup();
	AbilitySystemComponent->ClearStatusEffectsForRespawn();

	UBasicAttributeSet* AttributeSet = const_cast<UBasicAttributeSet*>(
		AbilitySystemComponent->GetSet<UBasicAttributeSet>());
	if (!AttributeSet)
	{
		return;
	}

	const float MaxHealthBeforeReset =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxHealthAttribute());
	const float MaxShieldBeforeReset =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxShieldAttribute());
	const float MaxStaminaBeforeReset =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxStaminaAttribute());
	const float MaxManaBeforeReset =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetMaxManaAttribute());

	FGameplayTagContainer DeadTags;
	DeadTags.AddTag(LabGameplayTags::State_Dead);
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DeadTags);
	AbilitySystemComponent->RemoveActiveEffects(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(DeadTags));

	const auto SetAttributeBase =
		[AbilitySystemComponent, AttributeSet](
			const FGameplayAttribute& Attribute,
			const float NewValue)
		{
			AbilitySystemComponent->SetNumericAttributeBase(
				Attribute,
				NewValue);
			if (FProperty* Property = Attribute.GetUProperty())
			{
				MARK_PROPERTY_DIRTY(AttributeSet, Property);
			}
		};

	const float RespawnMaxHealth =
		FMath::Max(MaxHealthBeforeReset, 1.0f);
	const float RespawnMaxShield =
		FMath::Max(MaxShieldBeforeReset, 0.0f);
	const float RespawnMaxStamina =
		FMath::Max(MaxStaminaBeforeReset, 0.0f);
	const float RespawnMaxMana =
		FMath::Max(MaxManaBeforeReset, 0.0f);

	SetAttributeBase(
		UBasicAttributeSet::GetMaxHealthAttribute(),
		RespawnMaxHealth);
	SetAttributeBase(
		UBasicAttributeSet::GetMaxShieldAttribute(),
		RespawnMaxShield);
	SetAttributeBase(
		UBasicAttributeSet::GetMaxStaminaAttribute(),
		RespawnMaxStamina);
	SetAttributeBase(
		UBasicAttributeSet::GetMaxManaAttribute(),
		RespawnMaxMana);
	SetAttributeBase(
		UBasicAttributeSet::GetHealthAttribute(),
		RespawnMaxHealth);
	SetAttributeBase(UBasicAttributeSet::GetShieldAttribute(), 0.0f);
	SetAttributeBase(
		UBasicAttributeSet::GetStaminaAttribute(),
		RespawnMaxStamina);
	SetAttributeBase(
		UBasicAttributeSet::GetManaAttribute(),
		RespawnMaxMana);

	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DeadTags);
	AbilitySystemComponent->RemoveActiveEffects(
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(DeadTags));

	if (AbilitySystemComponent->GetTagCount(
			LabGameplayTags::State_Dead) > 0)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(
			LabGameplayTags::State_Dead,
			0,
			EGameplayTagReplicationState::CountToOwner);
	}

	AbilitySystemComponent->ForceReplication();
	Enemy->ApplyMovementSpeedFromAttribute();
}

bool UEnemyCombatComponent::TryHandleActiveAttackAbility(
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent || AbilityHandles.IsEmpty())
	{
		return false;
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		FGameplayAbilitySpec* AbilitySpec =
			AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
		if (!AbilitySpec || !AbilitySpec->IsActive())
		{
			continue;
		}

		UAttackAbility* ActiveAttackAbility =
			Cast<UAttackAbility>(AbilitySpec->GetPrimaryInstance());
		if (ActiveAttackAbility)
		{
			if (Settings.bRequestComboWhenAttackIsActive)
			{
				const FName RequestedSectionName =
					ActiveAttackAbility->GetNextAttackSectionName();
				if (!RequestedSectionName.IsNone())
				{
					ActiveAttackAbility->RequestJumpToSection(
						RequestedSectionName);
				}
			}
			return true;
		}

		const UGameplayAbility* AbilityCDO = AbilitySpec->Ability;
		const UClass* AbilityClass =
			AbilityCDO ? AbilityCDO->GetClass() : nullptr;
		if (AbilityClass
			&& (AbilityClass->IsChildOf(UAttackAbility::StaticClass())
				|| AbilityClass->IsChildOf(
					URangedAttackAbility::StaticClass())))
		{
			return true;
		}
	}

	return false;
}

bool UEnemyCombatComponent::TryActivateAttackAbility(
	TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent || AbilityHandles.IsEmpty())
	{
		return false;
	}

	Algo::RandomShuffle(AbilityHandles);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		if (AbilitySystemComponent->TryActivateAbility(
				AbilityHandle,
				true))
		{
			return true;
		}
	}
	return false;
}

bool UEnemyCombatComponent::IsAttackAbilityActive() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	return AbilitySystemComponent
		&& AbilitySystemComponent->HasActiveAbilityOfAnyClass({
			UAttackAbility::StaticClass(),
			URangedAttackAbility::StaticClass(),
			UPunchAbility::StaticClass()
		});
}

bool UEnemyCombatComponent::IsAttackInProgress() const
{
	return IsAttackAbilityActive();
}

bool UEnemyCombatComponent::MoveToAttackTarget(
	AActor* CurrentAttackTarget)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| !Settings.bMoveToTargetBeforeAttack
		|| !IsActorValidAttackTarget(CurrentAttackTarget))
	{
		return false;
	}

	AAIController* AIController =
		Cast<AAIController>(Enemy->GetController());
	if (Enemy->IsStatusFrozen())
	{
		if (AIController)
		{
			AIController->StopMovement();
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
		return true;
	}

	const bool bUsingRangedWeapon = IsUsingRangedWeapon();
	if (bUsingRangedWeapon
		&& !Settings.bMoveToTargetBeforeRangedAttack)
	{
		if (!IsUsingGunWeapon() && AIController)
		{
			AIController->StopMovement();
		}
		return false;
	}

	const UEnemyTrainingBotComponent* TrainingBot =
		Enemy->GetEnemyTrainingBotComponent();
	if (TrainingBot && TrainingBot->IsHitStunned())
	{
		if (AIController)
		{
			AIController->StopMovement();
		}
		return true;
	}

	const float Distance2D =
		GetAttackDistanceToActor(CurrentAttackTarget);
	const float RequiredAttackDistance = GetAttackStartDistance();
	if (Distance2D <= RequiredAttackDistance)
	{
		if (AIController)
		{
			AIController->StopMovement();
		}
		return false;
	}

	if (!AIController)
	{
		return false;
	}

	static_cast<void>(AIController->MoveToActor(
		CurrentAttackTarget,
		RequiredAttackDistance,
		true,
		true,
		true,
		nullptr,
		true));
	return true;
}

bool UEnemyCombatComponent::RequestMoveToAttackTarget(
	AActor* InAttackTarget)
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| !IsActorValidAttackTarget(InAttackTarget))
	{
		return false;
	}

	SetAttackTarget(InAttackTarget);
	return MoveToAttackTarget(InAttackTarget);
}

bool UEnemyCombatComponent::ValidateAttackRequest()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	const UEnemyTrainingBotComponent* TrainingBot =
		Enemy ? Enemy->GetEnemyTrainingBotComponent() : nullptr;
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| !Settings.bAttackEnabled
		|| (TrainingBot && TrainingBot->IsWeaponChangeInProgress())
		|| !AbilitySystemComponent)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead))
	{
		Enemy->GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		StopAttackMovement();
		return false;
	}

	if (TrainingBot && TrainingBot->IsHitStunned())
	{
		StopAttackMovement();
		return false;
	}

	return true;
}

void UEnemyCombatComponent::StopAttackMovement() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (AAIController* AIController =
			Enemy ? Cast<AAIController>(Enemy->GetController()) : nullptr)
	{
		AIController->StopMovement();
	}
}

void UEnemyCombatComponent::FaceAttackTarget(
	const AActor* CurrentAttackTarget)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy
		|| Enemy->IsStatusFrozen()
		|| !IsValid(CurrentAttackTarget))
	{
		return;
	}

	if (AController* Controller = Enemy->GetController())
	{
		FRotator LookAtRotation =
			(CurrentAttackTarget->GetActorLocation()
				- Enemy->GetActorLocation())
				.Rotation();
		LookAtRotation.Pitch = 0.0f;
		LookAtRotation.Roll = 0.0f;
		Controller->SetControlRotation(LookAtRotation);
		Enemy->SetActorRotation(LookAtRotation);
	}
}

void UEnemyCombatComponent::GatherAttackAbilityHandles(
	const bool bUsingRangedWeapon,
	const bool bHasEquippedWeapon,
	TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles) const
{
	OutAbilityHandles.Reset();
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag AttackAbilityTag =
		ResolveEnemyAttackAbilityTag(
			bUsingRangedWeapon,
			bHasEquippedWeapon);
	if (AttackAbilityTag.IsValid())
	{
		FGameplayTagContainer AttackAbilityTags;
		AttackAbilityTags.AddTag(AttackAbilityTag);
		AbilitySystemComponent->FindAllAbilitiesWithTags(
			OutAbilityHandles,
			AttackAbilityTags,
			false);
	}

	if (!OutAbilityHandles.IsEmpty())
	{
		return;
	}

	// Compatibility fallback for legacy Blueprint abilities without the
	// expected native attack asset tag.
	for (const FGameplayAbilitySpec& AbilitySpec :
		AbilitySystemComponent->GetActivatableAbilities())
	{
		const UGameplayAbility* AbilityCDO = AbilitySpec.Ability;
		const UClass* AbilityClass =
			AbilityCDO ? AbilityCDO->GetClass() : nullptr;
		if (IsAbilityClassCompatibleWithAttackMode(
				AbilityClass,
				bUsingRangedWeapon,
				bHasEquippedWeapon))
		{
			OutAbilityHandles.AddUnique(AbilitySpec.Handle);
		}
	}
}

void UEnemyCombatComponent::Attack()
{
	if (!ValidateAttackRequest())
	{
		return;
	}

	AActor* CurrentAttackTarget = ResolveAttackTarget();
	if (!IsActorValidAttackTarget(CurrentAttackTarget))
	{
		StopAttackMovement();
		return;
	}

	FaceAttackTarget(CurrentAttackTarget);
	if (MoveToAttackTarget(CurrentAttackTarget))
	{
		return;
	}

	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UEquipmentComponent* Equipment =
		Enemy ? Enemy->GetEquipmentComponent() : nullptr;
	const bool bUsingRangedWeapon = IsUsingRangedWeapon();
	const bool bHasEquippedWeapon = Equipment
		&& (Equipment->GetCurrentWeaponActor()
			|| Equipment->GetCurrentWeaponDefinition());

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	GatherAttackAbilityHandles(
		bUsingRangedWeapon,
		bHasEquippedWeapon,
		AbilityHandles);
	if (!TryHandleActiveAttackAbility(AbilityHandles))
	{
		TryActivateAttackAbility(AbilityHandles);
	}
}

void UEnemyCombatComponent::SetAttackEnabled(
	const bool bInAttackEnabled)
{
	if (Settings.bAttackEnabled == bInAttackEnabled)
	{
		return;
	}

	Settings.bAttackEnabled = bInAttackEnabled;
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	if (!Settings.bAttackEnabled)
	{
		Enemy->GetWorldTimerManager().ClearTimer(AttackTimerHandle);
		if (UPdAbilitySystemComponent* AbilitySystemComponent =
				Enemy->GetEnemyAbilitySystemComponent())
		{
			FGameplayTagContainer AttackTags;
			AttackTags.AddTag(LabGameplayTags::Action_Attack);
			AttackTags.AddTag(LabGameplayTags::Action_Punch);
			AttackTags.AddTag(LabGameplayTags::Action_RangedAttack);
			AbilitySystemComponent->CancelAbilities(&AttackTags);
		}
	}
	else if (!Settings.bUseBehaviorTreeCombat
		&& Settings.bStartCombatOnPossess)
	{
		StartAttackTimer();
	}
}
