#include "AbilitySystem/Ability/AuraAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffectTypes.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AuraAbility)

namespace
{
	constexpr float MinimumPressAuraActivationDuration = 0.25f;

	const FAuraSkillConfig* GetAuraConfig(const USkillDefinition* SkillDataAsset)
	{
		return SkillDataAsset && SkillDataAsset->SkillDataType == ESkillDataType::Aura
			? &SkillDataAsset->Aura
			: nullptr;
	}

	float ResolveGroundEffectZOffset(const FAuraSkillConfig& AuraConfig)
	{
		return FMath::IsFinite(AuraConfig.GroundEffectZOffset)
			? static_cast<float>(FMath::Max(AuraConfig.GroundEffectZOffset, 0.0))
			: 3.0f;
	}

	FVector ResolveCharacterStandingFloorLocation(
		ACharacterBase* Character,
		const FAuraSkillConfig& AuraConfig)
	{
		const FVector BaseLocation = Character ? Character->GetActorLocation() : FVector::ZeroVector;
		FVector GroundLocation = BaseLocation;
		bool bResolvedMovementFloor = false;

		const UCharacterMovementComponent* MovementComponent =
			Character ? Character->GetCharacterMovement() : nullptr;
		if (MovementComponent && MovementComponent->CurrentFloor.IsWalkableFloor())
		{
			const FVector FloorImpactPoint = MovementComponent->CurrentFloor.HitResult.ImpactPoint;
			if (FMath::IsFinite(FloorImpactPoint.Z))
			{
				GroundLocation.Z = FloorImpactPoint.Z;
				bResolvedMovementFloor = true;
			}
		}

		// A floor result can be temporarily unavailable immediately after spawn or while changing movement modes.
		// Falling back to the capsule bottom keeps the placement trace-free and close to the character's feet.
		if (!bResolvedMovementFloor)
		{
			if (const UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr)
			{
				GroundLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
			}
		}

		GroundLocation.Z += ResolveGroundEffectZOffset(AuraConfig);
		return GroundLocation;
	}

	FTransform ResolveAuraEffectAreaSpawnTransform(ACharacterBase* Character, const FAuraSkillConfig& AuraConfig)
	{
		FVector SpawnLocation = ResolveCharacterStandingFloorLocation(Character, AuraConfig);
		SpawnLocation += AuraConfig.EffectAreaSpawnOffset;
		FRotator SpawnRotation = Character ? Character->GetActorRotation() : FRotator::ZeroRotator;
		return FTransform(SpawnRotation, SpawnLocation);
	}

	float ResolveAuraDuration(const USkillDefinition* SkillDataAsset, const FAuraSkillConfig* AuraConfig)
	{
		if (!SkillDataAsset || SkillDataAsset->SkillType != ESkillType::Duration || !AuraConfig)
		{
			return 0.0f;
		}

		const float ConfigDuration = SkillDataAsset->Time.Duration > 0.0
			? static_cast<float>(SkillDataAsset->Time.Duration)
			: static_cast<float>(FMath::Max(AuraConfig->Duration, 0.0));
		if (!AuraConfig->bSpawnEffectArea || !AuraConfig->bRepeatEffectAreaSpawn)
		{
			return ConfigDuration;
		}

		const float Interval = static_cast<float>(FMath::Max(AuraConfig->EffectAreaSpawnInterval, 0.1));
		const float MinimumRepeatDuration = Interval * 2.0f + KINDA_SMALL_NUMBER;
		if (ConfigDuration > 0.0f)
		{
			return FMath::Max(ConfigDuration, MinimumRepeatDuration);
		}

		return FMath::Max(static_cast<float>(AuraConfig->EffectAreaLifeSpan), MinimumRepeatDuration);
	}
}

UAuraAbility::UAuraAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(LabGameplayTags::GameplayAbility_Aura);
	SetAssetTags(AssetTags);

	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Aura_Active);
}

void UAuraAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	AuraDurationTask = nullptr;
	ActiveAuraSkillDataAsset = nullptr;
	ActiveAuraSourceCharacter.Reset();
	MovementSpeedEffectHandle.Invalidate();
	ActiveAuraEffectAreas.Reset();
	ActiveHealFieldOrigin = FVector::ZeroVector;
	ActiveHealFieldRadius = 0.0f;
	ActiveInteractionHealEffectHandles.Reset();
	AuraActivationWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (SkillDataAsset->SkillDataType != ESkillDataType::Aura)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	const bool bPressSkill = SkillDataAsset->SkillType == ESkillType::Press;
	ActiveAuraSkillDataAsset = SkillDataAsset;
	ActiveAuraSourceCharacter = GetPdCharacterFromActorInfo();
	StartDurationMovementLock();
	SpawnConfiguredCharacterDecal();
	StartConfiguredDefaultFX();
	StartConfiguredCharacterOverlay();
	ApplyMovementSpeedIncrease(SkillDataAsset);
	StartMovementContactDamage();
	StartAuraEffectAreaSpawning(SkillDataAsset);
	StartHealFieldTeamHealing(SkillDataAsset);

	const float Duration = ResolveAuraDuration(SkillDataAsset, AuraConfig);

	if (Duration <= 0.0f)
	{
		if (bPressSkill)
		{

			return;
		}

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	AuraDurationTask = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	if (!AuraDurationTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AuraDurationTask->OnFinish.AddDynamic(this, &ThisClass::OnAuraDurationFinished);
	AuraDurationTask->ReadyForActivation();
}

void UAuraAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (AuraDurationTask)
	{
		AuraDurationTask->EndTask();
		AuraDurationTask = nullptr;
	}

	StopHealFieldTeamHealing();
	StopAuraEffectAreaSpawning();
	RemoveMovementSpeedIncrease();
	ActiveAuraSkillDataAsset = nullptr;
	ActiveAuraSourceCharacter.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraAbility::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	static_cast<void>(Handle);
	static_cast<void>(ActorInfo);
	static_cast<void>(ActivationInfo);

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (SkillDataAsset
		&& SkillDataAsset->SkillType == ESkillType::Press
		&& IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		UWorld* World = GetWorld();
		const double ElapsedTime = World
			? World->GetTimeSeconds() - AuraActivationWorldTime
			: MinimumPressAuraActivationDuration;
		const float RemainingMinimumDuration = static_cast<float>(
			FMath::Max(
				static_cast<double>(MinimumPressAuraActivationDuration) - ElapsedTime,
				0.0));
		if (RemainingMinimumDuration <= KINDA_SMALL_NUMBER)
		{
			K2_EndAbility();
			return;
		}

		AuraDurationTask = UAbilityTask_WaitDelay::WaitDelay(
			this,
			RemainingMinimumDuration);
		if (!AuraDurationTask)
		{
			K2_EndAbility();
			return;
		}

		AuraDurationTask->OnFinish.AddDynamic(
			this,
			&ThisClass::OnAuraPressMinimumDurationFinished);
		AuraDurationTask->ReadyForActivation();
	}
}

void UAuraAbility::OnAuraDurationFinished()
{
	AuraDurationTask = nullptr;
	FinishAbilityFromDuration();
}

void UAuraAbility::OnAuraPressMinimumDurationFinished()
{
	AuraDurationTask = nullptr;
	if (IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		K2_EndAbility();
	}
}

void UAuraAbility::StartAuraEffectAreaSpawning(USkillDefinition* SkillDataAsset)
{
	ActiveAuraSkillDataAsset = SkillDataAsset;
	ActiveAuraSourceCharacter = GetPdCharacterFromActorInfo();
	SpawnAuraEffectArea(SkillDataAsset, TEXT("initial"));

	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	UWorld* World = GetWorld();
	if (!World || !AuraConfig || !AuraConfig->bSpawnEffectArea || !AuraConfig->bRepeatEffectAreaSpawn)
	{
		return;
	}

	const float Interval = static_cast<float>(FMath::Max(AuraConfig->EffectAreaSpawnInterval, 0.1));
	World->GetTimerManager().SetTimer(
		AuraEffectAreaSpawnTimerHandle,
		this,
		&ThisClass::HandleRepeatedAuraEffectAreaSpawn,
		Interval,
		true);

}

void UAuraAbility::StopAuraEffectAreaSpawning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraEffectAreaSpawnTimerHandle);
	}
	AuraEffectAreaSpawnTimerHandle.Invalidate();

	for (const TWeakObjectPtr<AEffectAreaBase>& AreaPtr : ActiveAuraEffectAreas)
	{
		if (AEffectAreaBase* Area = AreaPtr.Get())
		{
			Area->Destroy();
		}
	}
	ActiveAuraEffectAreas.Reset();
}

void UAuraAbility::HandleRepeatedAuraEffectAreaSpawn()
{
	const ACharacterBase* Character = ActiveAuraSourceCharacter.Get();

SpawnAuraEffectArea(ActiveAuraSkillDataAsset.Get(), TEXT("repeat"));
}

ACharacterBase* UAuraAbility::ResolveAuraSourceCharacter() const
{
	if (ACharacterBase* Character = ActiveAuraSourceCharacter.Get())
	{
		return Character;
	}

	return GetPdCharacterFromActorInfo();
}

void UAuraAbility::SpawnAuraEffectArea(const USkillDefinition* SkillDataAsset, const TCHAR* SpawnReason)
{
	ACharacterBase* Character = ResolveAuraSourceCharacter();
	UWorld* World = GetWorld();
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	if (!Character || !Character->HasAuthority() || !World || !AuraConfig || !AuraConfig->bSpawnEffectArea)
	{

		return;
	}

	if (!AuraConfig->EffectAreaClass)
	{

		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Cast<APawn>(Character);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform = ResolveAuraEffectAreaSpawnTransform(Character, *AuraConfig);
	AEffectAreaBase* SpawnedArea = World->SpawnActorDeferred<AEffectAreaBase>(
		AuraConfig->EffectAreaClass,
		SpawnTransform,
		SpawnParams.Owner,
		SpawnParams.Instigator,
		SpawnParams.SpawnCollisionHandlingOverride);
	if (SpawnedArea)
	{
		SpawnedArea->SetReplicates(true);
		SpawnedArea->SetReplicateMovement(true);
		SpawnedArea->SetSourceActor(Character);
		SpawnedArea->SetIgnoreSourceActor(AuraConfig->bEffectAreaIgnoreSourceActor);
		SpawnedArea->SetAffectEnemiesOnly(AuraConfig->bEffectAreaAffectEnemiesOnly);
		if (const UPandoraSkillRuntimeContext* RuntimeContext = GetSourceSkillRuntimeContext())
		{
			SpawnedArea->SetSourcePandoraLoadoutDirection(RuntimeContext->GetLoadoutDirection());
		}

		if (AuraConfig->EffectAreaLifeSpan > 0.0)
		{
			SpawnedArea->SetLifeSpan(static_cast<float>(AuraConfig->EffectAreaLifeSpan));
		}

		SpawnedArea->FinishSpawning(SpawnTransform);
		SpawnedArea->ForceNetUpdate();
		ActiveAuraEffectAreas.Add(SpawnedArea);
	}

}

void UAuraAbility::ApplyMovementSpeedIncrease(const USkillDefinition* SkillDataAsset)
{
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> MovementSpeedEffectClass =
		SettingDefinition
			? SettingDefinition->MovementSpeedGameplayEffectClass
			: nullptr;
	if (MovementSpeedEffectHandle.IsValid()
		|| !AuraConfig
		|| !Character
		|| !Character->HasAuthority()
		|| !AbilitySystemComponent
		|| !MovementSpeedEffectClass)
	{
		return;
	}

	if (!AuraConfig->bIncreaseMovementSpeedOnActivate || AuraConfig->MovementSpeedIncrease <= 0.0)
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
		static_cast<float>(AuraConfig->MovementSpeedIncrease));
	MovementSpeedEffectHandle = ApplyGameplayEffectSpecToOwner(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		MovementSpeedSpec);
}

void UAuraAbility::RemoveMovementSpeedIncrease()
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

void UAuraAbility::StartHealFieldTeamHealing(USkillDefinition* SkillDataAsset)
{
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	ACharacterBase* Character = ResolveAuraSourceCharacter();
	UWorld* World = GetWorld();
	const bool bHealEnabled = SkillDataAsset && (SkillDataAsset->Heal.bEnabled || (AuraConfig && AuraConfig->bHealTeamInInteractionBox));
	if (!World || !AuraConfig || !Character || !Character->HasAuthority() || !bHealEnabled)
	{
		return;
	}

	TSubclassOf<UGameplayEffect> HealEffectClass = SkillDataAsset->Heal.bEnabled && SkillDataAsset->Heal.TeamHealEffect.GameplayEffectClass
		? SkillDataAsset->Heal.TeamHealEffect.GameplayEffectClass
		: AuraConfig->TeamHealEffectClass;
	if (!HealEffectClass)
	{

		return;
	}

	ActiveHealFieldOrigin = ResolveHealFieldOrigin(Character);
	ActiveHealFieldRadius = ResolveHealFieldRadius(Character, SkillDataAsset);
	if (ActiveHealFieldRadius <= UE_SMALL_NUMBER)
	{

		return;
	}

	ApplyHealFieldTeamHeal(SkillDataAsset, TEXT("initial"));

	const double ConfiguredInterval = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.HealInterval
		: AuraConfig->TeamHealInterval;
	const FGameplayTag HealMagnitudeTag = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.TeamHealEffect.MagnitudeDataTag
		: AuraConfig->TeamHealMagnitudeDataTag;
	const double HealMagnitude = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.TeamHealEffect.Magnitude
		: AuraConfig->TeamHealMagnitude;
	const float Interval = static_cast<float>(FMath::Max(ConfiguredInterval, 0.05));
	World->GetTimerManager().SetTimer(
		HealFieldTeamHealTimerHandle,
		this,
		&ThisClass::HandleHealFieldTeamHealTick,
		Interval,
		true);

}

void UAuraAbility::StopHealFieldTeamHealing()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealFieldTeamHealTimerHandle);
	}
	HealFieldTeamHealTimerHandle.Invalidate();
	ActiveHealFieldOrigin = FVector::ZeroVector;
	ActiveHealFieldRadius = 0.0f;
	ClearInteractionHealEffects(TEXT("ability ended"));
}

void UAuraAbility::HandleHealFieldTeamHealTick()
{
	ApplyHealFieldTeamHeal(ActiveAuraSkillDataAsset.Get(), TEXT("tick"));
}

void UAuraAbility::ApplyHealFieldTeamHeal(const USkillDefinition* SkillDataAsset, const TCHAR* HealReason)
{
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	ACharacterBase* SourceCharacter = ResolveAuraSourceCharacter();
	const bool bHealEnabled = SkillDataAsset && (SkillDataAsset->Heal.bEnabled || (AuraConfig && AuraConfig->bHealTeamInInteractionBox));
	if (!AuraConfig || !SourceCharacter || !SourceCharacter->HasAuthority() || !bHealEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || ActiveHealFieldRadius <= UE_SMALL_NUMBER)
	{

		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AuraHealFieldOverlap), false, SourceCharacter);
	const FCollisionShape SphereShape = FCollisionShape::MakeSphere(ActiveHealFieldRadius);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(
		OverlapResults,
		ActiveHealFieldOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		SphereShape,
		QueryParams);

	int32 AppliedCount = 0;
	TSet<ACharacterBase*> CurrentTargets;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OverlappingActor = OverlapResult.GetActor();
		ACharacterBase* TargetCharacter = Cast<ACharacterBase>(OverlappingActor);
		if (!ShouldHealInteractionTarget(SourceCharacter, TargetCharacter, SkillDataAsset)
			|| !IsCharacterInsideActiveHealField(TargetCharacter))
		{
			continue;
		}

		CurrentTargets.Add(TargetCharacter);
	}

	const bool bHealSelf = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.bHealSelf
		: AuraConfig->bHealSelfInInteractionBox;
	if (bHealSelf
		&& ShouldHealInteractionTarget(SourceCharacter, SourceCharacter, SkillDataAsset)
		&& IsCharacterInsideActiveHealField(SourceCharacter))
	{
		CurrentTargets.Add(SourceCharacter);
	}

	TArray<TWeakObjectPtr<ACharacterBase>> ActiveTargets;
	ActiveInteractionHealEffectHandles.GetKeys(ActiveTargets);
	for (const TWeakObjectPtr<ACharacterBase>& ActiveTargetPtr : ActiveTargets)
	{
		ACharacterBase* ActiveTarget = ActiveTargetPtr.Get();
		FActiveGameplayEffectHandle ActiveHandle;
		if (!ActiveInteractionHealEffectHandles.RemoveAndCopyValue(ActiveTargetPtr, ActiveHandle))
		{
			continue;
		}

		if (ActiveTarget && CurrentTargets.Contains(ActiveTarget))
		{
			ActiveInteractionHealEffectHandles.Add(ActiveTarget, ActiveHandle);
			continue;
		}

		RemoveInteractionHealEffectFromTarget(ActiveTarget, ActiveHandle, TEXT("left heal field"));
	}

	for (ACharacterBase* TargetCharacter : CurrentTargets)
	{
		const TWeakObjectPtr<ACharacterBase> TargetKey(TargetCharacter);
		if (!TargetCharacter || ActiveInteractionHealEffectHandles.Contains(TargetKey))
		{
			continue;
		}

		const FActiveGameplayEffectHandle AppliedHandle = ApplyTeamHealEffectToTarget(SourceCharacter, TargetCharacter, SkillDataAsset, HealReason);
		if (AppliedHandle.WasSuccessfullyApplied())
		{
			++AppliedCount;
			if (AppliedHandle.IsValid())
			{
				ActiveInteractionHealEffectHandles.Add(TargetKey, AppliedHandle);
			}
		}
	}

}

UPrimitiveComponent* UAuraAbility::FindInteractionHealComponent(ACharacterBase* Character, const FName ComponentName) const
{
	if (!Character)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Character->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return nullptr;
	}

	if (!ComponentName.IsNone())
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}

			if (PrimitiveComponent->GetFName() == ComponentName || PrimitiveComponent->ComponentHasTag(ComponentName))
			{
				return PrimitiveComponent;
			}
		}
	}

	return nullptr;
}

float UAuraAbility::ResolveHealFieldRadius(ACharacterBase* SourceCharacter, const USkillDefinition* SkillDataAsset) const
{
	if (SkillDataAsset && SkillDataAsset->Heal.HealRadius > 0.0)
	{
		return static_cast<float>(SkillDataAsset->Heal.HealRadius);
	}

	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	UPrimitiveComponent* HealComponent = AuraConfig
		? FindInteractionHealComponent(SourceCharacter, AuraConfig->HealingInteractionComponentName)
		: nullptr;
	return HealComponent ? FMath::Max(HealComponent->Bounds.SphereRadius, 0.0f) : 0.0f;
}

FVector UAuraAbility::ResolveHealFieldOrigin(ACharacterBase* SourceCharacter) const
{
	if (!SourceCharacter)
	{
		return FVector::ZeroVector;
	}

	const FVector GroundLocation = ResolveConfiguredCharacterDecalLocation(SourceCharacter);
	return GroundLocation.IsNearlyZero() ? SourceCharacter->GetActorLocation() : GroundLocation;
}

bool UAuraAbility::IsCharacterInsideActiveHealField(const ACharacterBase* Character) const
{
	if (!Character || ActiveHealFieldRadius <= UE_SMALL_NUMBER)
	{
		return false;
	}

	float SquaredDistance = 0.0f;
	FVector ClosestPoint = FVector::ZeroVector;
	if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent();
		CapsuleComponent
		&& CapsuleComponent->GetSquaredDistanceToCollision(
			ActiveHealFieldOrigin,
			SquaredDistance,
			ClosestPoint))
	{
		return SquaredDistance <= FMath::Square(ActiveHealFieldRadius);
	}

	return FVector::DistSquared(Character->GetActorLocation(), ActiveHealFieldOrigin)
		<= FMath::Square(ActiveHealFieldRadius);
}

bool UAuraAbility::ShouldHealInteractionTarget(
	const ACharacterBase* SourceCharacter,
	const ACharacterBase* TargetCharacter,
	const USkillDefinition* SkillDataAsset) const
{
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	if (!AuraConfig || !SourceCharacter || !TargetCharacter)
	{
		return false;
	}

	if (TargetCharacter == SourceCharacter)
	{
		return SkillDataAsset->Heal.bEnabled
			? SkillDataAsset->Heal.bHealSelf
			: AuraConfig->bHealSelfInInteractionBox;
	}

	if (SourceCharacter->IsSameTeam(TargetCharacter))
	{
		return true;
	}

	const int32 SourceFactionId = SourceCharacter->GetFactionId();
	const int32 TargetFactionId = TargetCharacter->GetFactionId();
	return SourceFactionId != 0
		&& SourceFactionId == TargetFactionId;
}

FActiveGameplayEffectHandle UAuraAbility::ApplyTeamHealEffectToTarget(
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter,
	const USkillDefinition* SkillDataAsset,
	const TCHAR* HealReason) const
{
	const FAuraSkillConfig* AuraConfig = GetAuraConfig(SkillDataAsset);
	if (!AuraConfig || !SourceCharacter || !TargetCharacter)
	{
		return FActiveGameplayEffectHandle();
	}

	TSubclassOf<UGameplayEffect> HealEffectClass = SkillDataAsset->Heal.bEnabled && SkillDataAsset->Heal.TeamHealEffect.GameplayEffectClass
		? SkillDataAsset->Heal.TeamHealEffect.GameplayEffectClass
		: AuraConfig->TeamHealEffectClass;
	if (!HealEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayTag HealMagnitudeTag = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.TeamHealEffect.MagnitudeDataTag
		: AuraConfig->TeamHealMagnitudeDataTag;
	const double HealMagnitude = SkillDataAsset->Heal.bEnabled
		? SkillDataAsset->Heal.TeamHealEffect.Magnitude
		: AuraConfig->TeamHealMagnitude;

	UAbilitySystemComponent* SourceASC = SourceCharacter->GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = TargetCharacter->GetAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(Cast<APawn>(SourceCharacter), SourceCharacter);
	EffectContext.AddSourceObject(SkillDataAsset);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		HealEffectClass,
		FMath::Max(static_cast<float>(GetAbilityLevel()), 1.0f),
		EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	if (HealMagnitudeTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(
			HealMagnitudeTag,
			static_cast<float>(FMath::Max(HealMagnitude, 0.0)));
	}

	const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	const bool bApplied = AppliedHandle.WasSuccessfullyApplied();

	return AppliedHandle;
}

void UAuraAbility::RemoveInteractionHealEffectFromTarget(
	ACharacterBase* TargetCharacter,
	const FActiveGameplayEffectHandle ActiveHandle,
	const TCHAR* RemoveReason) const
{
	if (!TargetCharacter || !ActiveHandle.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = TargetCharacter->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return;
	}

	const bool bRemoved = TargetASC->RemoveActiveGameplayEffect(ActiveHandle);

}

void UAuraAbility::ClearInteractionHealEffects(const TCHAR* RemoveReason)
{
	TArray<TWeakObjectPtr<ACharacterBase>> ActiveTargets;
	ActiveInteractionHealEffectHandles.GetKeys(ActiveTargets);
	for (const TWeakObjectPtr<ACharacterBase>& ActiveTargetPtr : ActiveTargets)
	{
		FActiveGameplayEffectHandle ActiveHandle;
		if (ActiveInteractionHealEffectHandles.RemoveAndCopyValue(ActiveTargetPtr, ActiveHandle))
		{
			RemoveInteractionHealEffectFromTarget(ActiveTargetPtr.Get(), ActiveHandle, RemoveReason);
		}
	}

	ActiveInteractionHealEffectHandles.Reset();
}
