#include "AbilitySystem/Ability/SkillAbility.h"

#include "Skill/Actors/SkillVisualActor.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void USkillAbility::StartConfiguredDefaultFX()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority())
	{
		return;
	}

	SetConfiguredPresentationEnabled(ESkillPresentationFlags::DefaultFX, false);
	SetConfiguredPresentationEnabled(ESkillPresentationFlags::GroundFX, false);
	if (SkillDataAsset->Niagara.AuraNiagaraSystem || SkillDataAsset->Niagara.SocketNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(ESkillPresentationFlags::DefaultFX, true);
	}
	if (SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(ESkillPresentationFlags::GroundFX, true);
	}
}

void USkillAbility::StartConfiguredGroundFX()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		return;
	}

	SetConfiguredPresentationEnabled(ESkillPresentationFlags::GroundFX, false);
	SetConfiguredPresentationEnabled(ESkillPresentationFlags::GroundFX, true);
}

void USkillAbility::StartConfiguredCharacterOverlay()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Overlay.bUseCharacterOverlay
		|| !SkillDataAsset->Overlay.CharacterOverlayMaterial)
	{
		return;
	}

	SetConfiguredPresentationEnabled(ESkillPresentationFlags::CharacterOverlay, false);
	SetConfiguredPresentationEnabled(ESkillPresentationFlags::CharacterOverlay, true);
}

void USkillAbility::StartConfiguredMissilePresentation()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Niagara.SocketNiagaraSystem)
	{
		return;
	}

	StopConfiguredMissilePresentation();
	SetConfiguredPresentationEnabled(ESkillPresentationFlags::Missile, true);
}

void USkillAbility::SetMissileTargeting(FName AimParameter, FName TargetSocket)
{
	if (ASkillVisualActor* PresentationActor = ActiveSkillPresentationActor.Get())
		PresentationActor->SetMissileTargeting(AimParameter, TargetSocket);
}

void USkillAbility::UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors)
{
	if (ASkillVisualActor* PresentationActor = ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors(TargetActors);
	}
}

void USkillAbility::StopConfiguredMissilePresentation()
{
	if (ASkillVisualActor* PresentationActor = ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors({});
	}

	SetConfiguredPresentationEnabled(ESkillPresentationFlags::Missile, false);
}

void USkillAbility::DestroyActiveSkillPresentationActor()
{
	ASkillVisualActor* PresentationActor = ActiveSkillPresentationActor.Get();
	ActiveSkillPresentationActor = nullptr;

	if (IsValid(PresentationActor) && PresentationActor->HasAuthority())
	{
		PresentationActor->Destroy();
	}
}

void USkillAbility::SpawnConfiguredCharacterDecal()
{
	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority())
	{
		return;
	}

	const FSkillDecalSettings& DecalSettings = SkillDataAsset->CharacterDecal;
	if (!DecalSettings.DecalMaterial)
	{
		return;
	}

	const float StartSize = DecalSettings.DecalSize > 0.0 ? static_cast<float>(DecalSettings.DecalSize) : 512.0f;
	const float FinalSize =
		DecalSettings.bGrowDecalSize && DecalSettings.FinalDecalSize > 0.0 ? static_cast<float>(DecalSettings.FinalDecalSize) : StartSize;
	const float LifeSpan = HasDurationDeadline() ? GetRemainingDuration() : 2.0f;
	if (LifeSpan <= 0.0f) return;
	const float GrowthDuration = DecalSettings.bGrowDecalSize && !FMath::IsNearlyEqual(StartSize, FinalSize) ? LifeSpan : 0.0f;

	FGameplayCueParameters DecalCueParameters;
	DecalCueParameters.Location = ResolveConfiguredCharacterDecalLocation(Character);
	DecalCueParameters.Instigator = Character;
	DecalCueParameters.EffectCauser = Character;
	DecalCueParameters.SourceObject = DecalSettings.DecalMaterial.Get();
	DecalCueParameters.RawMagnitude = StartSize;
	DecalCueParameters.NormalizedMagnitude = FinalSize;
	DecalCueParameters.GameplayEffectLevel = FMath::Max(FMath::RoundToInt(LifeSpan * 1000.0f), 0);
	DecalCueParameters.AbilityLevel = FMath::Max(FMath::RoundToInt(GrowthDuration * 1000.0f), 0);
	K2_ExecuteGameplayCueWithParams(LabGameplayTags::GameplayCue_AOEIndicator, DecalCueParameters);
}

FVector USkillAbility::ResolveConfiguredCharacterDecalLocation(const ACharacterBase* Character) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	const FVector ActorLocation = Character->GetActorLocation();
	FVector DecalLocation = ActorLocation;
	if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent())
	{
		DecalLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return DecalLocation;
	}

	const FVector TraceStart = ActorLocation + FVector::UpVector * 150.0f;
	const FVector TraceEnd = ActorLocation - FVector::UpVector * 5000.0f;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SkillCharacterDecalGroundTrace), false, Character);
	QueryParams.AddIgnoredActor(Character);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult GroundHit;
	if (World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectParams, QueryParams) && GroundHit.bBlockingHit)
	{
		return GroundHit.ImpactPoint + GroundHit.ImpactNormal * 2.0f;
	}

	return DecalLocation;
}

ASkillVisualActor* USkillAbility::GetOrCreatePresentationActor()
{
	if (IsValid(ActiveSkillPresentationActor))
	{
		return ActiveSkillPresentationActor;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	UWorld* World = GetWorld();
	if (!Character || !Character->HasAuthority() || !SkillDataAsset || !World)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Character->GetActorRotation(), Character->GetActorLocation());
	ASkillVisualActor* PresentationActor = World->SpawnActorDeferred<ASkillVisualActor>(ASkillVisualActor::StaticClass(),
		SpawnTransform, Character, Cast<APawn>(Character), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PresentationActor)
	{
		return nullptr;
	}

	PresentationActor->InitializePresentation(Character, SkillDataAsset, ESkillPresentationFlags::None);
	PresentationActor->FinishSpawning(SpawnTransform);
	ActiveSkillPresentationActor = PresentationActor;
	return PresentationActor;
}

void USkillAbility::SetConfiguredPresentationEnabled(
	const ESkillPresentationFlags PresentationFlag, const bool bEnabled)
{
	ASkillVisualActor* PresentationActor = ActiveSkillPresentationActor.Get();
	if (bEnabled && !PresentationActor)
	{
		PresentationActor = GetOrCreatePresentationActor();
	}

	if (!PresentationActor)
	{
		return;
	}

	PresentationActor->SetPresentationEnabled(PresentationFlag, bEnabled);
	if (!PresentationActor->HasAnyPresentation())
	{
		PresentationActor->Destroy();
		ActiveSkillPresentationActor = nullptr;
	}
}
