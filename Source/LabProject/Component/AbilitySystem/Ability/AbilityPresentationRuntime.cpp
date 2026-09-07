#include "Component/AbilitySystem/Ability/AbilityPresentationRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/Presentation/SkillPresentationActor.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityPresentationRuntime)

void UAbilityPresentationRuntime::StartConfiguredDefaultFX(
	UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset
		|| !Character
		|| !Character->HasAuthority())
	{
		return;
	}

	StopConfiguredDefaultFX(Ability);
	if (SkillDataAsset->Niagara.AuraNiagaraSystem
		|| SkillDataAsset->Niagara.SocketNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(
			Ability,
			static_cast<uint8>(ESkillPresentationFlags::DefaultFX),
			true);
	}
	if (SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(
			Ability,
			static_cast<uint8>(ESkillPresentationFlags::GroundFX),
			true);
	}
}

void UAbilityPresentationRuntime::StopConfiguredDefaultFX(
	UPdGameplayAbility& Ability)
{
	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::DefaultFX),
		false);
	StopConfiguredGroundFX(Ability);
}

void UAbilityPresentationRuntime::StartConfiguredGroundFX(
	UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset
		|| !Character
		|| !Character->HasAuthority()
		|| !SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		return;
	}

	StopConfiguredGroundFX(Ability);
	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::GroundFX),
		true);
}

void UAbilityPresentationRuntime::StopConfiguredGroundFX(
	UPdGameplayAbility& Ability)
{
	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::GroundFX),
		false);
}

void UAbilityPresentationRuntime::StartConfiguredCharacterOverlay(
	UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset
		|| !Character
		|| !Character->HasAuthority()
		|| !SkillDataAsset->Overlay.bUseCharacterOverlay
		|| !SkillDataAsset->Overlay.CharacterOverlayMaterial)
	{
		return;
	}

	StopConfiguredCharacterOverlay(Ability);
	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::CharacterOverlay),
		true);
}

void UAbilityPresentationRuntime::StopConfiguredCharacterOverlay(
	UPdGameplayAbility& Ability)
{
	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::CharacterOverlay),
		false);
}

void UAbilityPresentationRuntime::StartConfiguredMissilePresentation(
	UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset
		|| !Character
		|| !Character->HasAuthority()
		|| !SkillDataAsset->Missile.MissileSystem)
	{
		return;
	}

	StopConfiguredMissilePresentation(Ability);
	ASkillPresentationActor* PresentationActor =
		EnsureConfiguredPresentationActor(Ability);
	if (!PresentationActor)
	{
		return;
	}

	PresentationActor->SetPresentationEnabled(
		ESkillPresentationFlags::Missile,
		true);
}

void UAbilityPresentationRuntime::
UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors)
{
	if (ASkillPresentationActor* PresentationActor =
		ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors(TargetActors);
	}
}

void UAbilityPresentationRuntime::StopConfiguredMissilePresentation(
	UPdGameplayAbility& Ability)
{
	if (ASkillPresentationActor* PresentationActor =
		ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors({});
	}

	SetConfiguredPresentationEnabled(
		Ability,
		static_cast<uint8>(ESkillPresentationFlags::Missile),
		false);
}

void UAbilityPresentationRuntime::CleanupConfiguredPresentation()
{
	if (ASkillPresentationActor* PresentationActor =
		ActiveSkillPresentationActor.Get())
	{
		if (PresentationActor->HasAuthority())
		{
			PresentationActor->Destroy();
		}
	}

	ActiveSkillPresentationActor = nullptr;
}

// 공유 메시의 크기는 캐릭터가 합성하고, 이 객체는 자신이 적용했던 대상만 기억한다.
void UAbilityPresentationRuntime::ApplySelfBuffCharacterScale(
	UPdGameplayAbility& Ability, const FSkillSelfBuffSettings& SelfBuffSettings)
{
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterPresentationComponent* Presentation = Character ? Character->GetCharacterPresentationComponent() : nullptr;
	if (Presentation && SelfBuffSettings.CharacterScaleMultiplier > 1.0)
	{
		Presentation->SetTemporaryMeshScaleMultiplier(&Ability, static_cast<float>(SelfBuffSettings.CharacterScaleMultiplier));
		SelfBuffScaleOwner = Presentation;
	}
}

void UAbilityPresentationRuntime::RestoreSelfBuffCharacterScale(UPdGameplayAbility& Ability)
{
	if (UCharacterPresentationComponent* Presentation = SelfBuffScaleOwner.Get())
	{
		Presentation->ClearTemporaryMeshScaleMultiplier(&Ability);
	}
	SelfBuffScaleOwner.Reset();
}

void UAbilityPresentationRuntime::SpawnConfiguredCharacterDecal(
	UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority())
	{
		return;
	}

	const FSkillDecalSettings& DecalSettings =
		SkillDataAsset->CharacterDecal;
	if (!DecalSettings.DecalMaterial)
	{
		return;
	}

	const float StartSize = DecalSettings.DecalSize > 0.0
		? static_cast<float>(DecalSettings.DecalSize)
		: 512.0f;
	const float FinalSize =
		DecalSettings.bGrowDecalSize
			&& DecalSettings.FinalDecalSize > 0.0
		? static_cast<float>(DecalSettings.FinalDecalSize)
		: StartSize;
	const float GrowthDuration =
		DecalSettings.bGrowDecalSize
			&& !FMath::IsNearlyEqual(StartSize, FinalSize)
		? ResolveConfiguredCharacterDecalDuration(SkillDataAsset)
		: 0.0f;
	const float LifeSpan = FMath::Max(
		ResolveConfiguredCharacterDecalDuration(SkillDataAsset),
		GrowthDuration);

	FGameplayCueParameters DecalCueParameters;
	DecalCueParameters.Location =
		ResolveConfiguredCharacterDecalLocation(Character);
	DecalCueParameters.Instigator = Character;
	DecalCueParameters.EffectCauser = Character;
	DecalCueParameters.SourceObject = DecalSettings.DecalMaterial.Get();
	DecalCueParameters.RawMagnitude = StartSize;
	DecalCueParameters.NormalizedMagnitude = FinalSize;
	DecalCueParameters.GameplayEffectLevel =
		FMath::Max(FMath::RoundToInt(LifeSpan * 1000.0f), 0);
	DecalCueParameters.AbilityLevel =
		FMath::Max(FMath::RoundToInt(GrowthDuration * 1000.0f), 0);
	Ability.K2_ExecuteGameplayCueWithParams(
		LabGameplayTags::GameplayCue_AOEIndicator,
		DecalCueParameters);
}

FVector
UAbilityPresentationRuntime::ResolveConfiguredCharacterDecalLocation(
	const ACharacterBase* Character) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	const FVector ActorLocation = Character->GetActorLocation();
	FVector DecalLocation = ActorLocation;
	if (const UCapsuleComponent* CapsuleComponent =
		Character->GetCapsuleComponent())
	{
		DecalLocation.Z -=
			CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return DecalLocation;
	}

	const FVector TraceStart =
		ActorLocation + FVector::UpVector * 150.0f;
	const FVector TraceEnd =
		ActorLocation - FVector::UpVector * 5000.0f;
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SkillCharacterDecalGroundTrace),
		false,
		Character);
	QueryParams.AddIgnoredActor(Character);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult GroundHit;
	if (World->LineTraceSingleByObjectType(
			GroundHit,
			TraceStart,
			TraceEnd,
			ObjectParams,
			QueryParams)
		&& GroundHit.bBlockingHit)
	{
		return GroundHit.ImpactPoint + GroundHit.ImpactNormal * 2.0f;
	}

	return DecalLocation;
}

float
UAbilityPresentationRuntime::ResolveConfiguredCharacterDecalDuration(
	const USkillDefinition* SkillDataAsset) const
{
	if (SkillDataAsset
		&& SkillDataAsset->SkillType == ESkillType::Duration
		&& SkillDataAsset->Time.Duration > 0.0)
	{
		return static_cast<float>(SkillDataAsset->Time.Duration);
	}

	return 2.0f;
}

ASkillPresentationActor*
UAbilityPresentationRuntime::EnsureConfiguredPresentationActor(
	UPdGameplayAbility& Ability)
{
	if (IsValid(ActiveSkillPresentationActor))
	{
		return ActiveSkillPresentationActor;
	}

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	UWorld* World = Ability.GetWorld();
	if (!Character
		|| !Character->HasAuthority()
		|| !SkillDataAsset
		|| !World)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(
		Character->GetActorRotation(),
		Character->GetActorLocation());
	ASkillPresentationActor* PresentationActor =
		World->SpawnActorDeferred<ASkillPresentationActor>(
			ASkillPresentationActor::StaticClass(),
			SpawnTransform,
			Character,
			Cast<APawn>(Character),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!PresentationActor)
	{
		return nullptr;
	}

	PresentationActor->InitializePresentation(
		Character,
		SkillDataAsset,
		ESkillPresentationFlags::None);
	PresentationActor->FinishSpawning(SpawnTransform);
	ActiveSkillPresentationActor = PresentationActor;
	return PresentationActor;
}

void UAbilityPresentationRuntime::SetConfiguredPresentationEnabled(
	UPdGameplayAbility& Ability,
	const uint8 PresentationFlag,
	const bool bEnabled)
{
	ASkillPresentationActor* PresentationActor =
		ActiveSkillPresentationActor.Get();
	if (bEnabled && !PresentationActor)
	{
		PresentationActor = EnsureConfiguredPresentationActor(Ability);
	}

	if (!PresentationActor)
	{
		return;
	}

	PresentationActor->SetPresentationEnabled(
		static_cast<ESkillPresentationFlags>(PresentationFlag),
		bEnabled);
	if (!PresentationActor->HasAnyPresentation())
	{
		PresentationActor->Destroy();
		ActiveSkillPresentationActor = nullptr;
	}
}
