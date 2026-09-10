#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/Presentation/SkillPresentationActor.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityPresentationManager)

// 서버에서 몸·소켓·지면 이펙트 상태를 초기화한 뒤 스킬에 설정된 연출을 시작한다.
void UAbilityPresentationManager::StartConfiguredDefaultFX(UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority())
	{
		return;
	}

	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::DefaultFX, false);
	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::GroundFX, false);
	if (SkillDataAsset->Niagara.AuraNiagaraSystem || SkillDataAsset->Niagara.SocketNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::DefaultFX, true);
	}
	if (SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::GroundFX, true);
	}
}

// 서버에서 설정된 지면 이펙트만 다시 시작한다.
void UAbilityPresentationManager::StartConfiguredGroundFX(UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Niagara.GroundNiagaraSystem)
	{
		return;
	}

	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::GroundFX, false);
	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::GroundFX, true);
}

// 서버에서 이 스킬이 요청하는 캐릭터 오버레이를 다시 시작한다.
void UAbilityPresentationManager::StartConfiguredCharacterOverlay(UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Overlay.bUseCharacterOverlay
		|| !SkillDataAsset->Overlay.CharacterOverlayMaterial)
	{
		return;
	}

	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::CharacterOverlay, false);
	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::CharacterOverlay, true);
}

// 기존 미사일 대상과 연출을 정리한 뒤 미사일 연출을 시작한다.
void UAbilityPresentationManager::StartConfiguredMissilePresentation(UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !Character || !Character->HasAuthority() || !SkillDataAsset->Missile.MissileSystem)
	{
		return;
	}

	StopConfiguredMissilePresentation(Ability);
	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::Missile, true);
}

// 활성 미사일 연출에 현재 추적할 대상 목록을 전달한다.
void UAbilityPresentationManager::UpdateConfiguredMissilePresentationTargets(const TArray<AActor*>& TargetActors)
{
	if (ASkillPresentationActor* PresentationActor = ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors(TargetActors);
	}
}

// 미사일 추적 대상을 비우고 미사일 연출만 중단한다.
void UAbilityPresentationManager::StopConfiguredMissilePresentation(UPdGameplayAbility& Ability)
{
	if (ASkillPresentationActor* PresentationActor = ActiveSkillPresentationActor.Get())
	{
		PresentationActor->SetMissileTargetActors({});
	}

	SetConfiguredPresentationEnabled(Ability, ESkillPresentationFlags::Missile, false);
}

// 능력 종료·강제 정리 시 서버의 연출 액터를 제거하고 보관 참조를 비운다.
void UAbilityPresentationManager::DestroyActiveSkillPresentationActor()
{
	ASkillPresentationActor* PresentationActor = ActiveSkillPresentationActor.Get();
	if (IsValid(PresentationActor) && PresentationActor->HasAuthority())
	{
		PresentationActor->Destroy();
	}

	ActiveSkillPresentationActor = nullptr;
}

// 캐릭터의 공유 크기 합성에 이 능력의 확대 요청을 등록하고 복구할 컴포넌트를 기억한다.
void UAbilityPresentationManager::ApplySelfBuffCharacterScale(UPdGameplayAbility& Ability, const FSkillSelfBuffSettings& SelfBuffSettings)
{
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterPresentationComponent* Presentation = Character ? Character->GetCharacterPresentationComponent() : nullptr;
	if (Presentation && SelfBuffSettings.CharacterScaleMultiplier > 1.0)
	{
		Presentation->SetTemporaryMeshScaleMultiplier(&Ability, static_cast<float>(SelfBuffSettings.CharacterScaleMultiplier));
		SelfBuffScaleOwner = Presentation;
	}
}

// 확대를 요청했던 컴포넌트에서 이 능력의 요청만 해제한다.
void UAbilityPresentationManager::RestoreSelfBuffCharacterScale(UPdGameplayAbility& Ability)
{
	if (UCharacterPresentationComponent* Presentation = SelfBuffScaleOwner.Get())
	{
		Presentation->ClearTemporaryMeshScaleMultiplier(&Ability);
	}
	SelfBuffScaleOwner.Reset();
}

// 위치·크기·성장 시간·수명을 계산해 캐릭터 발밑 데칼 GameplayCue를 실행한다.
void UAbilityPresentationManager::SpawnConfiguredCharacterDecal(UPdGameplayAbility& Ability)
{
	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
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
	const float LifeSpan = ResolveConfiguredCharacterDecalDuration(SkillDataAsset);
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
	Ability.K2_ExecuteGameplayCueWithParams(LabGameplayTags::GameplayCue_AOEIndicator, DecalCueParameters);
}

// 캐릭터 아래 지면을 찾고, 지면이 없으면 캡슐 하단 위치를 사용한다.
FVector UAbilityPresentationManager::ResolveConfiguredCharacterDecalLocation(const ACharacterBase* Character) const
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

// 지속형 스킬은 설정된 지속시간을, 나머지는 기본 데칼 수명을 사용한다.
float UAbilityPresentationManager::ResolveConfiguredCharacterDecalDuration(const USkillDefinition* SkillDataAsset) const
{
	if (SkillDataAsset && SkillDataAsset->SkillType == ESkillType::Duration && SkillDataAsset->Time.Duration > 0.0)
	{
		return static_cast<float>(SkillDataAsset->Time.Duration);
	}

	return 2.0f;
}

// 이 능력의 연출 액터를 재사용하거나 서버에서 생성·초기화한다.
ASkillPresentationActor* UAbilityPresentationManager::GetOrCreatePresentationActor(UPdGameplayAbility& Ability)
{
	if (IsValid(ActiveSkillPresentationActor))
	{
		return ActiveSkillPresentationActor;
	}

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	UWorld* World = Ability.GetWorld();
	if (!Character || !Character->HasAuthority() || !SkillDataAsset || !World)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(Character->GetActorRotation(), Character->GetActorLocation());
	ASkillPresentationActor* PresentationActor = World->SpawnActorDeferred<ASkillPresentationActor>(ASkillPresentationActor::StaticClass(),
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

// 연출 종류의 활성 상태를 변경한다. 켤 때만 액터를 만들고 마지막 연출이 꺼지면 제거한다.
void UAbilityPresentationManager::SetConfiguredPresentationEnabled(
	UPdGameplayAbility& Ability, const ESkillPresentationFlags PresentationFlag, const bool bEnabled)
{
	ASkillPresentationActor* PresentationActor = ActiveSkillPresentationActor.Get();
	if (bEnabled && !PresentationActor)
	{
		PresentationActor = GetOrCreatePresentationActor(Ability);
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
