#include "Skill/Actions/SkillAuraAction.h"

#include "Skill/Actors/SkillEffectArea.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
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
#include "Pandora/PandoraSkillSource.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillAuraAction)

namespace
{

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
}

USkillAuraAction::USkillAuraAction()
{
	Healing.TeamHealEffect.MagnitudeDataTag = LabGameplayTags::Data_Heal;
}

void USkillAuraAction::OnStart()
{
	const auto Handle = GetAbility()->GetCurrentAbilitySpecHandle();
	const auto* ActorInfo = GetAbility()->GetCurrentActorInfo();
	const auto ActivationInfo = GetAbility()->GetCurrentActivationInfo();
	const auto* TriggerEventData = &GetContext().EventData;
	static_cast<void>(TriggerEventData);

	ActiveAuraSkillDataAsset = nullptr;
	ActiveAuraSourceCharacter.Reset();
	MovementSpeedEffectHandle.Invalidate();
	ActiveAuraEffectAreas.Reset();
	ActiveHealFieldOrigin = FVector::ZeroVector;
	ActiveHealFieldRadius = 0.0f;
	ActiveInteractionHealEffectHandles.Reset();

	USkillDefinition* SkillDataAsset = GetAbility()->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{

		Finish(!(true));
		return;
	}

	if (!GetAbility()->CommitSkill())
	{

		Finish(!(true));
		return;
	}

	const bool bPressSkill = SkillDataAsset->SkillType == ESkillType::Press;
	ActiveAuraSkillDataAsset = SkillDataAsset;
	ActiveAuraSourceCharacter = GetAbility()->GetPdCharacterFromActorInfo();
	GetAbility()->StartDurationMovementLock();
	GetAbility()->SpawnConfiguredCharacterDecal();
	GetAbility()->StartConfiguredDefaultFX();
	GetAbility()->StartConfiguredCharacterOverlay();
	ApplyMovementSpeedIncrease(SkillDataAsset);
	GetAbility()->StartMovementContactDamage();
	StartAuraEffectAreaSpawning(SkillDataAsset);
	StartHealFieldTeamHealing(SkillDataAsset);

	// 유지 액션의 종료 시점은 스킬 실행자가 관리한다.
	if (!GetAbility()->HasDurationDeadline() && !bPressSkill)
	{
		Finish();
	}
}

void USkillAuraAction::OnStop()
{


	StopHealFieldTeamHealing();
	StopAuraEffectAreaSpawning();
	RemoveMovementSpeedIncrease();
	ActiveAuraSkillDataAsset = nullptr;
	ActiveAuraSourceCharacter.Reset();
}

void USkillAuraAction::StartAuraEffectAreaSpawning(USkillDefinition* SkillDataAsset)
{
	ActiveAuraSkillDataAsset = SkillDataAsset;
	ActiveAuraSourceCharacter = GetAbility()->GetPdCharacterFromActorInfo();
	SpawnAuraEffectArea(SkillDataAsset, TEXT("initial"));

	const FAuraSkillConfig* AuraConfig = &Settings;
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

void USkillAuraAction::StopAuraEffectAreaSpawning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuraEffectAreaSpawnTimerHandle);
	}
	AuraEffectAreaSpawnTimerHandle.Invalidate();

	for (const TWeakObjectPtr<ASkillEffectArea>& AreaPtr : ActiveAuraEffectAreas)
	{
		if (ASkillEffectArea* Area = AreaPtr.Get())
		{
			Area->Destroy();
		}
	}
	ActiveAuraEffectAreas.Reset();
}

void USkillAuraAction::HandleRepeatedAuraEffectAreaSpawn()
{
	const ACharacterBase* Character = ActiveAuraSourceCharacter.Get();

SpawnAuraEffectArea(ActiveAuraSkillDataAsset.Get(), TEXT("repeat"));
}

ACharacterBase* USkillAuraAction::ResolveAuraSourceCharacter() const
{
	if (ACharacterBase* Character = ActiveAuraSourceCharacter.Get())
	{
		return Character;
	}

	return GetAbility()->GetPdCharacterFromActorInfo();
}

void USkillAuraAction::SpawnAuraEffectArea(const USkillDefinition* SkillDataAsset, const TCHAR* SpawnReason)
{
	ACharacterBase* Character = ResolveAuraSourceCharacter();
	UWorld* World = GetWorld();
	const FAuraSkillConfig* AuraConfig = &Settings;
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
	ASkillEffectArea* SpawnedArea = World->SpawnActorDeferred<ASkillEffectArea>(
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
		SpawnedArea->SetSourcePandoraLoadoutDirection(
			GetAbility()->GetPandoraSkillSource()->GetLoadoutDirection());

		if (AuraConfig->EffectAreaLifeSpan > 0.0)
		{
			SpawnedArea->SetLifeSpan(static_cast<float>(AuraConfig->EffectAreaLifeSpan));
		}

		SpawnedArea->FinishSpawning(SpawnTransform);
		SpawnedArea->ForceNetUpdate();
		ActiveAuraEffectAreas.Add(SpawnedArea);
	}

}

void USkillAuraAction::ApplyMovementSpeedIncrease(const USkillDefinition* SkillDataAsset)
{
	const FAuraSkillConfig* AuraConfig = &Settings;
	ACharacterBase* Character = GetAbility()->GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
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

	if (!SkillDataAsset->Movement.bOverrideMovementSpeedWhileActive || SkillDataAsset->Movement.MovementSpeedBonusPercent <= 0.0)
	{
		return;
	}

	FGameplayEffectSpecHandle MovementSpeedSpec =
		GetAbility()->MakeOutgoingGameplayEffectSpec(
			GetAbility()->GetCurrentAbilitySpecHandle(),
			GetAbility()->GetCurrentActorInfo(),
			GetAbility()->GetCurrentActivationInfo(),
			MovementSpeedEffectClass,
			GetAbility()->GetAbilityLevel());
	if (!MovementSpeedSpec.IsValid() || !MovementSpeedSpec.Data.IsValid())
	{
		return;
	}

	MovementSpeedSpec.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_MovementSpeed,
		static_cast<float>(SkillDataAsset->Movement.MovementSpeedBonusPercent));
	MovementSpeedEffectHandle = GetAbility()->ApplyGameplayEffectSpecToOwner(
		GetAbility()->GetCurrentAbilitySpecHandle(),
		GetAbility()->GetCurrentActorInfo(),
		GetAbility()->GetCurrentActivationInfo(),
		MovementSpeedSpec);
}

void USkillAuraAction::RemoveMovementSpeedIncrease()
{
	if (!MovementSpeedEffectHandle.IsValid())
	{
		return;
	}

	ACharacterBase* Character = GetAbility()->GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetAbility()->GetPdAbilitySystemComponentFromActorInfo();
	if (Character && Character->HasAuthority() && AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(
			MovementSpeedEffectHandle,
			1);
	}

	MovementSpeedEffectHandle.Invalidate();
}

void USkillAuraAction::StartHealFieldTeamHealing(USkillDefinition* SkillDataAsset)
{
	const FAuraSkillConfig* AuraConfig = &Settings;
	ACharacterBase* Character = ResolveAuraSourceCharacter();
	UWorld* World = GetWorld();
	const bool bHealEnabled = SkillDataAsset && Healing.bEnabled;
	if (!World || !AuraConfig || !Character || !Character->HasAuthority() || !bHealEnabled)
	{
		return;
	}

	TSubclassOf<UGameplayEffect> HealEffectClass = Healing.TeamHealEffect.GameplayEffectClass;
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

	const double ConfiguredInterval = Healing.HealInterval;
	const FGameplayTag HealMagnitudeTag = Healing.TeamHealEffect.MagnitudeDataTag;
	const double HealMagnitude = Healing.TeamHealEffect.Magnitude;
	const float Interval = static_cast<float>(FMath::Max(ConfiguredInterval, 0.05));
	World->GetTimerManager().SetTimer(
		HealFieldTeamHealTimerHandle,
		this,
		&ThisClass::HandleHealFieldTeamHealTick,
		Interval,
		true);

}

void USkillAuraAction::StopHealFieldTeamHealing()
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

void USkillAuraAction::HandleHealFieldTeamHealTick()
{
	ApplyHealFieldTeamHeal(ActiveAuraSkillDataAsset.Get(), TEXT("tick"));
}

void USkillAuraAction::ApplyHealFieldTeamHeal(const USkillDefinition* SkillDataAsset, const TCHAR* HealReason)
{
	const FAuraSkillConfig* AuraConfig = &Settings;
	ACharacterBase* SourceCharacter = ResolveAuraSourceCharacter();
	const bool bHealEnabled = SkillDataAsset && Healing.bEnabled;
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

	const bool bHealSelf = Healing.bHealSelf;
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

UPrimitiveComponent* USkillAuraAction::FindInteractionHealComponent(ACharacterBase* Character, const FName ComponentName) const
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

float USkillAuraAction::ResolveHealFieldRadius(ACharacterBase* SourceCharacter, const USkillDefinition* SkillDataAsset) const
{
	if (SkillDataAsset && Healing.HealRadius > 0.0)
	{
		return static_cast<float>(Healing.HealRadius);
	}

	const FAuraSkillConfig* AuraConfig = &Settings;
	UPrimitiveComponent* HealComponent = AuraConfig
		? FindInteractionHealComponent(SourceCharacter, Healing.HealingInteractionComponentName)
		: nullptr;
	return HealComponent ? FMath::Max(HealComponent->Bounds.SphereRadius, 0.0f) : 0.0f;
}

FVector USkillAuraAction::ResolveHealFieldOrigin(ACharacterBase* SourceCharacter) const
{
	if (!SourceCharacter)
	{
		return FVector::ZeroVector;
	}

	const FVector GroundLocation = GetAbility()->ResolveConfiguredCharacterDecalLocation(SourceCharacter);
	return GroundLocation.IsNearlyZero() ? SourceCharacter->GetActorLocation() : GroundLocation;
}

bool USkillAuraAction::IsCharacterInsideActiveHealField(const ACharacterBase* Character) const
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

bool USkillAuraAction::ShouldHealInteractionTarget(
	const ACharacterBase* SourceCharacter,
	const ACharacterBase* TargetCharacter,
	const USkillDefinition* SkillDataAsset) const
{
	const FAuraSkillConfig* AuraConfig = &Settings;
	if (!AuraConfig || !SourceCharacter || !TargetCharacter)
	{
		return false;
	}

	if (TargetCharacter == SourceCharacter)
	{
		return Healing.bHealSelf;
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

FActiveGameplayEffectHandle USkillAuraAction::ApplyTeamHealEffectToTarget(
	ACharacterBase* SourceCharacter,
	ACharacterBase* TargetCharacter,
	const USkillDefinition* SkillDataAsset,
	const TCHAR* HealReason) const
{
	const FAuraSkillConfig* AuraConfig = &Settings;
	if (!AuraConfig || !SourceCharacter || !TargetCharacter)
	{
		return FActiveGameplayEffectHandle();
	}

	TSubclassOf<UGameplayEffect> HealEffectClass = Healing.TeamHealEffect.GameplayEffectClass;
	if (!HealEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayTag HealMagnitudeTag = Healing.TeamHealEffect.MagnitudeDataTag;
	const double HealMagnitude = Healing.TeamHealEffect.Magnitude;

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
		FMath::Max(static_cast<float>(GetAbility()->GetAbilityLevel()), 1.0f),
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

void USkillAuraAction::RemoveInteractionHealEffectFromTarget(
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

void USkillAuraAction::ClearInteractionHealEffects(const TCHAR* RemoveReason)
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
