#include "Weapon/WeaponBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ArrowProjectileBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "TimerManager.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

namespace
{
	constexpr float AttackTraceInterpolationDistance = 5.0f;

	const FName SkillTrailComponentName(TEXT("SkillTrailComponent"));
	const FName SkillTrailComponentDisplayName(TEXT("Skill Trail Component"));
	const FName SkillTrailName(TEXT("SkillTrail"));

	bool IsNamedSkillTrailComponent(const UNiagaraComponent* NiagaraComponent)
	{
		if (!NiagaraComponent)
		{
			return false;
		}

		return NiagaraComponent->GetFName() == SkillTrailComponentName
			|| NiagaraComponent->GetFName() == SkillTrailComponentDisplayName
			|| NiagaraComponent->GetFName() == SkillTrailName
			|| NiagaraComponent->ComponentHasTag(SkillTrailComponentName)
			|| NiagaraComponent->ComponentHasTag(SkillTrailComponentDisplayName)
			|| NiagaraComponent->ComponentHasTag(SkillTrailName);
	}

}

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AttackTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("TraceStart"));
	AttackTraceStart->SetupAttachment(WeaponMesh);
	AttackTraceStart->bEditableWhenInherited = true;

	AttackTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("TraceEnd"));
	AttackTraceEnd->SetupAttachment(WeaponMesh);
	AttackTraceEnd->bEditableWhenInherited = true;
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(AWeaponBase, SourceItemDefinition, Params);
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttackTrace();
	ClearSkillSlash();
	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::SetBeginOverlapEnabled(bool bEnabled)
{
	if (bEnabled)
	{
		StartAttackTrace();
	}
	else
	{
		StopAttackTrace();
	}
}

void AWeaponBase::StartAttackTrace()
{
	TrackedAttackSectionName = NAME_None;
	StartAttackTraceInternal(true);
}

void AWeaponBase::StartAttackTraceForSection(const FName AttackSectionName)
{
	if (AttackSectionName.IsNone())
	{
		return;
	}

	const bool bEnteredNewAttackSection = TrackedAttackSectionName != AttackSectionName;
	if (bEnteredNewAttackSection)
	{
		TrackedAttackSectionName = AttackSectionName;
		HitActorsInCurrentAttack.Reset();
		bHasPreviousAttackTraceSegment = false;
	}

	StartAttackTraceInternal(false);
}

void AWeaponBase::ResetAttackHitTracking()
{
	TrackedAttackSectionName = NAME_None;
	HitActorsInCurrentAttack.Reset();
}

void AWeaponBase::StartAttackTraceInternal(const bool bResetHitActors)
{
	if (!HasAuthority() || !IsCurrentWeaponForOwner())
	{

		return;
	}

	if (bAttackTraceActive)
	{

		return;
	}

	bAttackTraceActive = true;
	if (bResetHitActors)
	{
		HitActorsInCurrentAttack.Reset();
	}
	bHasPreviousAttackTraceSegment = false;

	PerformAttackTrace();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AttackTraceTimerHandle,
			this,
			&ThisClass::PerformAttackTrace,
			FMath::Max(AttackTraceInterval, UE_SMALL_NUMBER),
			true);
	}
}

void AWeaponBase::StopAttackTrace()
{
	const bool bWasAttackTraceActive = bAttackTraceActive;
	bAttackTraceActive = false;
	bHasPreviousAttackTraceSegment = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTraceTimerHandle);
	}

	AttackTraceTimerHandle.Invalidate();
}

bool AWeaponBase::PlayWeaponAttackMontage(FName StartingSection)
{
	return PlayConfiguredWeaponMontage(StartingSection, GetWeaponAttackSpeedPlayRate());
}

bool AWeaponBase::PlayConfiguredWeaponMontage(FName StartingSection, float PlayRate)
{
	if (!WeaponMesh)
	{

		return false;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{

		return false;
	}

	const float SafePlayRate = FMath::Max(0.01f, PlayRate);
	if (AnimInstance->Montage_Play(Montage, SafePlayRate) <= 0.0f)
	{
		return false;
	}

	if (!StartingSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartingSection, Montage);
	}

	return true;
}

bool AWeaponBase::JumpToWeaponMontageSectionAndResume(FName SectionName)
{
	if (!WeaponMesh)
	{

		return false;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{

		return false;
	}

	if (!SectionName.IsNone())
	{
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}

	AnimInstance->Montage_Resume(Montage);
	return true;
}

void AWeaponBase::StopWeaponMontage(float BlendOutTime)
{
	if (!WeaponMesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{
		return;
	}

	AnimInstance->Montage_Stop(BlendOutTime, Montage);
}

bool AWeaponBase::HasSkillWeaponTrailComponent() const
{
	UNiagaraComponent* TrailComponent = ResolveSkillTrailComponent();
	return TrailComponent != nullptr;
}

bool AWeaponBase::StartSkillWeaponTrail(UNiagaraSystem* TrailSystem)
{
	if (!TrailSystem)
	{
		return false;
	}

	ActiveSkillTrailSystem = TrailSystem;
	UNiagaraComponent* TrailComponent = ResolveSkillTrailComponent();
	if (!TrailComponent)
	{
		return false;
	}

	if (HasAuthority())
	{
		MulticastStartSkillWeaponTrail(TrailSystem);
		return true;
	}

	return ApplySkillWeaponTrailVisual(true);
}

bool AWeaponBase::StartSkillWeaponTrail()
{
	return StartSkillWeaponTrail(ActiveSkillTrailSystem);
}

void AWeaponBase::StopSkillWeaponTrail()
{
	if (HasAuthority())
	{
		MulticastStopSkillWeaponTrail();
		return;
	}

	ApplySkillWeaponTrailVisual(false);
}

void AWeaponBase::PlayComboWindowStartEffect(UNiagaraSystem* EffectSystem)
{
	if (!EffectSystem || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ACharacterBase* OwningCharacter = GetOwningCharacter();
	if (!OwningCharacter
		|| !OwningCharacter->IsPlayerControlled()
		|| !OwningCharacter->IsLocallyControlled())
	{
		return;
	}

	USceneComponent* EffectAttachComponent = OwningCharacter->GetRootComponent();
	if (!EffectAttachComponent)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAttached(
		EffectSystem,
		EffectAttachComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

void AWeaponBase::ConfigureSkillSlash(
	UNiagaraSystem* SlashSystem,
	const FVector& SlashScale,
	const FVector& SlashSpawnLocationOffset,
	const FName SlashSpawnSocketName,
	const FRotator& SlashSpawnRotationOffset,
	float AttackTraceEndMultiplier,
	bool bEnableHitTrace,
	TSubclassOf<UGameplayEffect> AdditionalDamageEffectClass,
	FGameplayTag AdditionalDamageDataTag,
	float AdditionalDamageMagnitude,
	int32 AdditionalDamageLevel,
	UObject* AdditionalDamageSourceObject,
	float AdditionalDamageDelay)
{
	ActiveSkillSlashSystem = SlashSystem;
	ActiveSkillSlashScale = SlashScale.IsNearlyZero() ? FVector::OneVector : SlashScale;
	ActiveSkillSlashSpawnLocationOffset = SlashSpawnLocationOffset;
	ActiveSkillSlashSpawnSocketName = SlashSpawnSocketName;
	ActiveSkillSlashSpawnRotationOffset = SlashSpawnRotationOffset;
	ActiveSkillAttackTraceEndMultiplier = FMath::Max(AttackTraceEndMultiplier, 1.0f);
	bSkillSlashHitTraceEnabled = bEnableHitTrace;
	ClearActiveSkillAdditionalDamage();
	if (AdditionalDamageEffectClass && AdditionalDamageMagnitude > 0.0f)
	{
		ActiveSkillAdditionalDamageEffectClass = AdditionalDamageEffectClass;
		ActiveSkillAdditionalDamageDataTag = AdditionalDamageDataTag;
		ActiveSkillAdditionalDamageMagnitude = FMath::Max(AdditionalDamageMagnitude, 0.0f);
		ActiveSkillAdditionalDamageLevel = FMath::Max(AdditionalDamageLevel, 1);
		ActiveSkillAdditionalDamageSourceObject = AdditionalDamageSourceObject;
		ActiveSkillAdditionalDamageDelay = FMath::Max(AdditionalDamageDelay, 0.0f);
	}

}

void AWeaponBase::PlaySkillSlashVisual()
{
	const ACharacterBase* OwningCharacter = GetOwningCharacter();
	const bool bCanPredictForOwningClient = !HasAuthority()
		&& OwningCharacter
		&& OwningCharacter->IsLocallyControlled();
	if ((!HasAuthority() && !bCanPredictForOwningClient)
		|| !IsCurrentWeaponForOwner()
		|| !bSkillSlashHitTraceEnabled
		|| !ActiveSkillSlashSystem)
	{
		return;
	}

	SpawnSkillSlashNiagara();
}

void AWeaponBase::ClearSkillSlash()
{
	if (HasAuthority())
	{
		StopAttackTrace();
	}
	else
	{
		bAttackTraceActive = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AttackTraceTimerHandle);
		}
		AttackTraceTimerHandle.Invalidate();
	}
	ActiveSkillSlashSystem = nullptr;
	ActiveSkillSlashScale = FVector::OneVector;
	ActiveSkillSlashSpawnLocationOffset = FVector::ZeroVector;
	ActiveSkillSlashSpawnSocketName = NAME_None;
	ActiveSkillSlashSpawnRotationOffset = FRotator::ZeroRotator;
	ActiveSkillAttackTraceEndMultiplier = 1.0f;
	bSkillSlashHitTraceEnabled = false;
	ClearActiveSkillAdditionalDamage();
}

void AWeaponBase::SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, const float Multiplier)
{
	if (!SourceObject)
	{
		return;
	}

	const FObjectKey SourceKey(SourceObject);
	const float ClampedMultiplier = FMath::Max(Multiplier, 1.0f);
	if (ClampedMultiplier <= 1.0f)
	{
		TemporaryAttackTraceEndZMultipliers.Remove(SourceKey);
		return;
	}

	TemporaryAttackTraceEndZMultipliers.Add(SourceKey, ClampedMultiplier);


}

void AWeaponBase::ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject)
{
	if (!SourceObject)
	{
		return;
	}

	const int32 RemovedCount = TemporaryAttackTraceEndZMultipliers.Remove(FObjectKey(SourceObject));
}

float AWeaponBase::GetTemporaryAttackTraceEndZMultiplier() const
{
	float ActiveMultiplier = 1.0f;
	for (const TPair<FObjectKey, float>& Entry : TemporaryAttackTraceEndZMultipliers)
	{
		ActiveMultiplier = FMath::Max(ActiveMultiplier, Entry.Value);
	}
	return ActiveMultiplier;
}

void AWeaponBase::InitializeFromItemDefinition(const UItemDefinition* InItemDefinition)
{
	SourceItemDefinition = const_cast<UItemDefinition*>(InItemDefinition);
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AWeaponBase, SourceItemDefinition, this);
	}
}

bool AWeaponBase::SupportsAimInput() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.bSupportsInput;
	}

	return false;
}

FGameplayTag AWeaponBase::GetAimCrosshairWidgetTag() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.CrosshairWidgetTag;
	}

	return FGameplayTag();
}

const FWeaponAimCameraSettings& AWeaponBase::GetAimCameraSettings() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.CameraSettings;
	}

	static const FWeaponAimCameraSettings DefaultAimCameraSettings;
	return DefaultAimCameraSettings;
}

bool AWeaponBase::ShouldTriggerHitReactOnDamage() const
{
	return true;
}

bool AWeaponBase::HandleAimStart(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return false;
	}

	PlayerCharacter->SetWeaponAimActive(true, GetAimCameraSettings());
	return true;
}

void AWeaponBase::HandleAimEnd(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->SetWeaponAimActive(false, GetAimCameraSettings());
}

bool AWeaponBase::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	static_cast<void>(PlayerCharacter);
	return false;
}

bool AWeaponBase::HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
	static_cast<void>(AttackingCharacter);
	static_cast<void>(TargetActor);
	return false;
}

bool AWeaponBase::HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation)
{
	static_cast<void>(AttackingCharacter);
	static_cast<void>(TargetActor);
	static_cast<void>(TargetLocation);
	return false;
}

bool AWeaponBase::SupportsAutomaticFire() const
{
	return false;
}

float AWeaponBase::GetAutomaticFireInterval() const
{
	return 0.0f;
}

bool AWeaponBase::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
	static_cast<void>(PlayerCharacter);
	if (NotifyName == WeaponAnimNotifyNames::StartSkillTrail())
	{
		const bool bStartedTrail = ActiveSkillTrailSystem ? StartSkillWeaponTrail() : false;
		if (bSkillSlashHitTraceEnabled)
		{
			StartAttackTrace();
		}
		return bStartedTrail || bSkillSlashHitTraceEnabled;
	}

	if (NotifyName == WeaponAnimNotifyNames::StopSkillTrail())
	{
		if (bSkillSlashHitTraceEnabled)
		{
			StopAttackTrace();
		}
		if (ActiveSkillTrailSystem)
		{
			StopSkillWeaponTrail();
		}
		return true;
	}

	if (NotifyName == WeaponAnimNotifyNames::SpawnSkillSlash())
	{
		PlaySkillSlashVisual();
		return bSkillSlashHitTraceEnabled;
	}

	return false;
}

const UItemDefinition* AWeaponBase::GetSourceItemDefinition() const
{
	return SourceItemDefinition.Get();
}

bool AWeaponBase::TryGetOwnerMeshSocketLocation(const ACharacterBase* Character, FName SocketName, FVector& OutLocation) const
{
	const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh || SocketName.IsNone() || !CharacterMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	OutLocation = CharacterMesh->GetSocketLocation(SocketName);
	return true;
}

bool AWeaponBase::ResolveServerAimViewPoint(
	const APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	FVector& OutViewLocation,
	FVector& OutViewDirection) const
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const FVector RequestedDirection = RequestedViewDirection.GetSafeNormal();
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const float MaxAcceptedViewDistance = ItemDefinition ? ItemDefinition->WeaponData.Aim.MaxAcceptedServerViewDistance : 0.0f;
	if (!RequestedDirection.IsNearlyZero()
		&& MaxAcceptedViewDistance > 0.0f
		&& FVector::DistSquared(RequestedViewLocation, PlayerCharacter->GetActorLocation()) <= FMath::Square(MaxAcceptedViewDistance))
	{
		OutViewLocation = RequestedViewLocation;
		OutViewDirection = RequestedDirection;
		return true;
	}

	return PlayerCharacter->GetWeaponAimViewPoint(OutViewLocation, OutViewDirection);
}

bool AWeaponBase::ResolveAimTargetBeyondLaunchPoint(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	const FVector& LaunchStartLocation,
	float TraceRange,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore,
	EDrawDebugTrace::Type DebugDrawType,
	FVector& OutTargetLocation) const
{
	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero() || TraceRange <= 0.0f)
	{
		return false;
	}

	// The camera ray and the launch socket are usually not collinear. A simple distance
	// offset can therefore still select a wall that is between the camera and the weapon.
	// Re-trace past every invalid obstruction until the first candidate that is actually
	// in front of the launch socket along the requested aim direction.
	const float CameraToLaunchDistance = FVector::Distance(ViewLocation, LaunchStartLocation);
	const FVector AimTraceEnd =
		ViewLocation + (SafeViewDirection * (CameraToLaunchDistance + TraceRange));
	FVector AimTraceStart = ViewLocation;
	TArray<AActor*> AimActorsToIgnore = ActorsToIgnore;

	constexpr int32 MaxSkippedAimObstructions = 16;
	constexpr float AimTraceAdvanceDistance = 2.0f;
	for (int32 AttemptIndex = 0; AttemptIndex < MaxSkippedAimObstructions; ++AttemptIndex)
	{
		FHitResult HitResult;
		const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			AimTraceStart,
			AimTraceEnd,
			ObjectTypes,
			false,
			AimActorsToIgnore,
			DebugDrawType,
			HitResult,
			true,
			FLinearColor::Red,
			FLinearColor::Green,
			5.0f);
		if (!bHit)
		{
			OutTargetLocation = AimTraceEnd;
			return true;
		}

		const FVector HitLocation = HitResult.Location;
		const float HitForwardDistanceFromLaunch =
			FVector::DotProduct(HitLocation - LaunchStartLocation, SafeViewDirection);
		const bool bHitIsBehindLaunchPlane =
			HitForwardDistanceFromLaunch <= AimTraceAdvanceDistance;
		const bool bCharacterNonMeshHit =
			PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(
				HitResult.GetActor(),
				HitResult.GetComponent());
		if (!bHitIsBehindLaunchPlane && !bCharacterNonMeshHit)
		{
			OutTargetLocation = HitLocation;
			return true;
		}

		if (bHitIsBehindLaunchPlane && IsValid(HitResult.GetActor()))
		{
			// Ignore this actor only while acquiring the aim target. The authoritative
			// launch-socket trace still includes it and will block a genuinely forward shot.
			AimActorsToIgnore.AddUnique(HitResult.GetActor());
		}

		const float RemainingTraceDistance =
			FVector::DotProduct(AimTraceEnd - HitLocation, SafeViewDirection);
		if (RemainingTraceDistance <= AimTraceAdvanceDistance)
		{
			break;
		}

		AimTraceStart = HitLocation + (SafeViewDirection * AimTraceAdvanceDistance);
	}

	OutTargetLocation = AimTraceEnd;
	return true;
}

float AWeaponBase::GetWeaponAttackSpeedPlayRate() const
{
	const ACharacterBase* CharacterOwner = Cast<ACharacterBase>(GetOwner());
	const UPdAbilitySystemComponent* AbilitySystemComponent = CharacterOwner
		? Cast<UPdAbilitySystemComponent>(CharacterOwner->GetAbilitySystemComponent())
		: nullptr;
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;

	const float AttackSpeedPercent = AttributeSet ? FMath::Max(AttributeSet->GetAttackSpeed(), 0.0f) : 0.0f;
	return FMath::Max(0.01f, 1.0f + AttackSpeedPercent * 0.01f);
}

UAnimMontage* AWeaponBase::GetConfiguredWeaponMontage() const
{
	return nullptr;
}

FName AWeaponBase::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const
{
	return NAME_None;
}

void AWeaponBase::MulticastStartSkillWeaponTrail_Implementation(UNiagaraSystem* TrailSystem)
{
	ActiveSkillTrailSystem = TrailSystem;
	ApplySkillWeaponTrailVisual(true);
}

void AWeaponBase::MulticastStopSkillWeaponTrail_Implementation()
{
	ApplySkillWeaponTrailVisual(false);
}

void AWeaponBase::MulticastSpawnSkillSlashNiagara_Implementation(
	UNiagaraSystem* SlashSystem,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	const FVector& SpawnScale)
{
	if (!SlashSystem
		|| ConsumeMatchingPredictedSkillSlash(SlashSystem, SpawnLocation))
	{
		return;
	}

	SpawnSkillSlashNiagaraLocal(
		SlashSystem,
		SpawnLocation,
		SpawnRotation,
		SpawnScale);
}

ACharacterBase* AWeaponBase::GetOwningCharacter() const
{
	if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	return Cast<ACharacterBase>(GetAttachParentActor());
}

bool AWeaponBase::IsCurrentWeaponForOwner() const
{
	const ACharacterBase* SourceCharacter = GetOwningCharacter();
	const UEquipmentComponent* EquipmentComponent = SourceCharacter
		? SourceCharacter->GetEquipmentComponent()
		: nullptr;
	return EquipmentComponent && EquipmentComponent->GetCurrentWeaponActor() == this;
}

bool AWeaponBase::CanDamageTracedHit(const FHitResult& HitResult) const
{
	const ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(
		HitResult.GetActor(),
		HitResult.GetComponent());
	const ACharacterBase* SourceCharacter = GetOwningCharacter();
	if (!TargetCharacter || !SourceCharacter || TargetCharacter == SourceCharacter)
	{
		return false;
	}

	if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{

		return false;
	}

	return !HitActorsInCurrentAttack.Contains(TargetCharacter);
}

FVector AWeaponBase::GetAttackTraceEndLocation(const FVector& TraceStartLocation) const
{
	const FVector RawTraceEndLocation = AttackTraceEnd ? AttackTraceEnd->GetComponentLocation() : TraceStartLocation;
	const float TraceEndZMultiplier = GetTemporaryAttackTraceEndZMultiplier();

	FVector TraceVector = RawTraceEndLocation - TraceStartLocation;
	if (!TraceVector.IsNearlyZero() && TraceEndZMultiplier > 1.0f)
	{
		TraceVector.Z *= TraceEndZMultiplier;
	}

	const FVector ZAdjustedTraceEndLocation = TraceStartLocation + TraceVector;
	if (!bSkillSlashHitTraceEnabled || ActiveSkillAttackTraceEndMultiplier <= 1.0f)
	{
		return ZAdjustedTraceEndLocation;
	}

	const FVector SkillTraceVector = ZAdjustedTraceEndLocation - TraceStartLocation;
	if (SkillTraceVector.IsNearlyZero())
	{
		return ZAdjustedTraceEndLocation;
	}

	return TraceStartLocation + (SkillTraceVector * ActiveSkillAttackTraceEndMultiplier);
}

bool AWeaponBase::IsAttackDebugVisualizationEnabled() const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return false;
#else
	if (!bDrawAttackTraceDebug)
	{
		return false;
	}

	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	return SettingDefinition && SettingDefinition->bDrawAttackDebugVisualization;
#endif
}

void AWeaponBase::PerformAttackTrace()
{
	if (!HasAuthority() || !AttackTraceStart || !AttackTraceEnd)
	{
		return;
	}

	if (!IsCurrentWeaponForOwner())
	{
		StopAttackTrace();
		return;
	}

	const FVector TraceStartLocation = AttackTraceStart->GetComponentLocation();
	const FVector TraceEndLocation = GetAttackTraceEndLocation(TraceStartLocation);
	if (TraceStartLocation.Equals(TraceEndLocation, KINDA_SMALL_NUMBER))
	{
		return;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel1));
	TArray<TEnumAsByte<EObjectTypeQuery>> EnemyCapsuleObjectTypes;
	EnemyCapsuleObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	if (AActor* ParentActor = GetAttachParentActor())
	{
		ActorsToIgnore.Add(ParentActor);
	}
	if (APawn* OwnerInstigator = GetInstigator())
	{
		ActorsToIgnore.Add(OwnerInstigator);
	}
	if (ACharacterBase* SourceCharacter = GetOwningCharacter())
	{
		ActorsToIgnore.Add(SourceCharacter);
	}

	TArray<FVector> DebugStartLocations;
	TArray<FVector> DebugEndLocations;
	TArray<FHitResult> DebugHitResults;

	auto TraceAttackLine = [this, &ObjectTypes, &EnemyCapsuleObjectTypes, &ActorsToIgnore, &DebugStartLocations, &DebugEndLocations, &DebugHitResults](
		const FVector& LineStart,
		const FVector& LineEnd)
	{
		TArray<FHitResult> HitResults;

		const float TraceRadius = FMath::Clamp(AttackTraceRadius, 0.0f, 20.0f);
		if (TraceRadius > UE_SMALL_NUMBER)
		{
			UKismetSystemLibrary::SphereTraceMultiForObjects(
				this,
				LineStart,
				LineEnd,
				TraceRadius,
				ObjectTypes,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				HitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				0.1f);
		}
		else
		{
			UKismetSystemLibrary::LineTraceMultiForObjects(
				this,
				LineStart,
				LineEnd,
				ObjectTypes,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				HitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				0.1f);
		}

		TArray<FHitResult> CapsuleHitResults;
		if (TraceRadius > UE_SMALL_NUMBER)
		{
			UKismetSystemLibrary::SphereTraceMultiForObjects(
				this,
				LineStart,
				LineEnd,
				TraceRadius,
				EnemyCapsuleObjectTypes,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				CapsuleHitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				0.1f);
		}
		else
		{
			UKismetSystemLibrary::LineTraceMultiForObjects(
				this,
				LineStart,
				LineEnd,
				EnemyCapsuleObjectTypes,
				false,
				ActorsToIgnore,
				EDrawDebugTrace::None,
				CapsuleHitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				0.1f);
		}
		HitResults.Append(CapsuleHitResults);

		DebugStartLocations.Add(LineStart);
		DebugEndLocations.Add(LineEnd);
		DebugHitResults.Append(HitResults);

		for (const FHitResult& HitResult : HitResults)
		{
			ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(
				HitResult.GetActor(),
				HitResult.GetComponent());
			if (!TargetCharacter || !CanDamageTracedHit(HitResult))
			{
				continue;
			}

			HitActorsInCurrentAttack.Add(TargetCharacter);
			DebugSuccessfulHit(HitResult);
			ApplyDamageFromAuthoritativeTrace(HitResult);
		}
	};

	if (!bHasPreviousAttackTraceSegment)
	{
		TraceAttackLine(TraceStartLocation, TraceEndLocation);

		if (IsAttackDebugVisualizationEnabled())
		{
			MulticastDrawInterpolatedAttackTraceDebug(DebugStartLocations, DebugEndLocations, DebugHitResults);
		}

		PreviousAttackTraceStartLocation = TraceStartLocation;
		PreviousAttackTraceEndLocation = TraceEndLocation;
		bHasPreviousAttackTraceSegment = true;
		return;
	}

	const float StartTravelDistance = FVector::Distance(PreviousAttackTraceStartLocation, TraceStartLocation);
	const float EndTravelDistance = FVector::Distance(PreviousAttackTraceEndLocation, TraceEndLocation);
	const float MaxTravelDistance = FMath::Max(StartTravelDistance, EndTravelDistance);
	const int32 InterpolationCount = FMath::Max(1, FMath::CeilToInt(MaxTravelDistance / AttackTraceInterpolationDistance));

	for (int32 InterpolationIndex = 1; InterpolationIndex <= InterpolationCount; ++InterpolationIndex)
	{
		const float Alpha = static_cast<float>(InterpolationIndex) / static_cast<float>(InterpolationCount);
		const FVector InterpolatedStartLocation = FMath::Lerp(PreviousAttackTraceStartLocation, TraceStartLocation, Alpha);
		const FVector InterpolatedEndLocation = FMath::Lerp(PreviousAttackTraceEndLocation, TraceEndLocation, Alpha);
		TraceAttackLine(InterpolatedStartLocation, InterpolatedEndLocation);
	}

	if (IsAttackDebugVisualizationEnabled())
	{
		MulticastDrawInterpolatedAttackTraceDebug(DebugStartLocations, DebugEndLocations, DebugHitResults);
	}

	PreviousAttackTraceStartLocation = TraceStartLocation;
	PreviousAttackTraceEndLocation = TraceEndLocation;
	bHasPreviousAttackTraceSegment = true;
}

void AWeaponBase::DebugSuccessfulHit(const FHitResult& HitResult) const
{
	static_cast<void>(HitResult);
}

bool AWeaponBase::HasActiveSkillAdditionalDamage() const
{
	return ActiveSkillAdditionalDamageEffectClass && ActiveSkillAdditionalDamageMagnitude > 0.0f;
}

void AWeaponBase::ApplyActiveSkillAdditionalDamageToTarget(ACharacterBase* TargetCharacter)
{
	if (!HasAuthority() || !HasActiveSkillAdditionalDamage())
	{
		return;
	}

	const TWeakObjectPtr<ACharacterBase> TargetWeak(TargetCharacter);
	const TSubclassOf<UGameplayEffect> DamageEffectClass = ActiveSkillAdditionalDamageEffectClass;
	const FGameplayTag DamageDataTag = ActiveSkillAdditionalDamageDataTag;
	const float DamageMagnitude = ActiveSkillAdditionalDamageMagnitude;
	const int32 DamageLevel = ActiveSkillAdditionalDamageLevel;
	const TWeakObjectPtr<UObject> DamageSourceObjectWeak(ActiveSkillAdditionalDamageSourceObject.Get());
	const float DamageDelay = FMath::Max(ActiveSkillAdditionalDamageDelay, 0.0f);
	if (DamageDelay > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle DelayHandle;
			World->GetTimerManager().SetTimer(
				DelayHandle,
				FTimerDelegate::CreateWeakLambda(this,
					[this, TargetWeak, DamageEffectClass, DamageDataTag, DamageMagnitude, DamageLevel, DamageSourceObjectWeak]()
					{
						ApplySkillAdditionalDamageToTarget(
							TargetWeak.Get(),
							DamageEffectClass,
							DamageDataTag,
							DamageMagnitude,
							DamageLevel,
							DamageSourceObjectWeak.Get());
					}),
				DamageDelay,
				false);

			return;
		}
	}

	ApplySkillAdditionalDamageToTarget(
		TargetCharacter,
		DamageEffectClass,
		DamageDataTag,
		DamageMagnitude,
		DamageLevel,
		DamageSourceObjectWeak.Get());
}

void AWeaponBase::ApplySkillAdditionalDamageToTarget(
	ACharacterBase* TargetCharacter,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	FGameplayTag DamageDataTag,
	float DamageMagnitude,
	int32 DamageLevel,
	UObject* DamageSourceObject)
{
	if (!HasAuthority() || !DamageEffectClass || DamageMagnitude <= 0.0f)
	{
		return;
	}

	ACharacterBase* SourceCharacter = GetOwningCharacter();
	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
	{
		return;
	}

	UPdAbilitySystemComponent* SourceASC = SourceCharacter->GetPdAbilitySystemComponent();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter->GetPdAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{

		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(SourceCharacter, this);
	EffectContext.AddSourceObject(DamageSourceObject ? DamageSourceObject : static_cast<UObject*>(this));

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		FMath::Max(DamageLevel, 1),
		EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{

		return;
	}

	if (!DamageDataTag.IsValid())
	{
		SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageDataTag);
	}

	if (!DamageDataTag.IsValid())
	{

		return;
	}

	DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);
	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);


}

void AWeaponBase::ClearActiveSkillAdditionalDamage()
{
	ActiveSkillAdditionalDamageEffectClass = nullptr;
	ActiveSkillAdditionalDamageDataTag = FGameplayTag();
	ActiveSkillAdditionalDamageMagnitude = 0.0f;
	ActiveSkillAdditionalDamageLevel = 1;
	ActiveSkillAdditionalDamageSourceObject = nullptr;
	ActiveSkillAdditionalDamageDelay = 0.12f;
}

bool AWeaponBase::ApplySkillWeaponTrailVisual(bool bActivate)
{
	UNiagaraComponent* TrailComponent = ResolveSkillTrailComponent();
	if (!TrailComponent)
	{

		return false;
	}

	if (bActivate && !ActiveSkillTrailSystem)
	{

		return false;
	}

	if (bActivate)
	{
		TrailComponent->SetAsset(ActiveSkillTrailSystem, false);
		TrailComponent->SetAutoActivate(true);
		TrailComponent->SetVisibility(true, true);
		TrailComponent->Activate(true);
	}
	else
	{
		TrailComponent->Deactivate();
		TrailComponent->SetVisibility(false, true);
		TrailComponent->SetAutoActivate(false);
		ActiveSkillTrailSystem = nullptr;
	}

	return true;
}

void AWeaponBase::SpawnSkillSlashNiagara()
{
	if (!bSkillSlashHitTraceEnabled)
	{

		return;
	}

	if (!ActiveSkillSlashSystem)
	{

		return;
	}

	const bool bHasTraceComponents = AttackTraceStart && AttackTraceEnd;
	const FVector TraceStartLocation = AttackTraceStart ? AttackTraceStart->GetComponentLocation() : FVector::ZeroVector;
	const FVector RawTraceEndLocation = AttackTraceEnd ? AttackTraceEnd->GetComponentLocation() : TraceStartLocation;
	const FVector TraceEndLocation = bHasTraceComponents ? GetAttackTraceEndLocation(TraceStartLocation) : RawTraceEndLocation;

	FRotator SlashRotation = ActiveSkillSlashSpawnRotationOffset;
	FVector SlashBaseLocation = RawTraceEndLocation;
	FVector SlashLocation = SlashBaseLocation + SlashRotation.RotateVector(ActiveSkillSlashSpawnLocationOffset);
	FVector SlashScale = ActiveSkillSlashScale;
	bool bUsedSocketTransform = false;
	if (!ActiveSkillSlashSpawnSocketName.IsNone())
	{
		ACharacterBase* OwnerCharacter = GetOwningCharacter();
		USkeletalMeshComponent* OwnerMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
		if (OwnerMesh && OwnerMesh->DoesSocketExist(ActiveSkillSlashSpawnSocketName))
		{
			const FTransform SocketTransform = OwnerMesh->GetSocketTransform(ActiveSkillSlashSpawnSocketName, RTS_World);
			SlashBaseLocation = SocketTransform.GetLocation();
			SlashLocation = SocketTransform.TransformPosition(ActiveSkillSlashSpawnLocationOffset);
			SlashRotation = (SocketTransform.GetRotation().Rotator() + ActiveSkillSlashSpawnRotationOffset).GetNormalized();
			SlashScale = ActiveSkillSlashScale * SocketTransform.GetScale3D();
			bUsedSocketTransform = true;
		}
	}

	if (!bUsedSocketTransform)
	{
		if (!bHasTraceComponents)
		{

			return;
		}

		FVector TraceVector = RawTraceEndLocation - TraceStartLocation;
		if (TraceVector.IsNearlyZero())
		{
			TraceVector = TraceEndLocation - TraceStartLocation;
		}
		if (TraceVector.IsNearlyZero())
		{

			return;
		}

		SlashRotation = (TraceVector.GetSafeNormal().Rotation() + ActiveSkillSlashSpawnRotationOffset).GetNormalized();
		SlashBaseLocation = RawTraceEndLocation;
		SlashLocation = SlashBaseLocation + SlashRotation.RotateVector(ActiveSkillSlashSpawnLocationOffset);
	}



	if (HasAuthority())
	{
		MulticastSpawnSkillSlashNiagara(
			ActiveSkillSlashSystem,
			SlashLocation,
			SlashRotation,
			SlashScale);
	}
	else
	{
		const bool bSpawnedPredictedVisual = SpawnSkillSlashNiagaraLocal(
			ActiveSkillSlashSystem,
			SlashLocation,
			SlashRotation,
			SlashScale);
		if (bSpawnedPredictedVisual)
		{
			PredictedSkillSlashSystem = ActiveSkillSlashSystem;
			PredictedSkillSlashLocation = SlashLocation;
			PredictedSkillSlashWorldTime = GetWorld()
				? GetWorld()->GetTimeSeconds()
				: -1.0;
			bHasPendingPredictedSkillSlash = true;
		}
	}
}

bool AWeaponBase::SpawnSkillSlashNiagaraLocal(
	UNiagaraSystem* SlashSystem,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	const FVector& SpawnScale)
{
	if (!SlashSystem || GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const FVector EffectiveScale = SpawnScale.IsNearlyZero()
		? FVector::OneVector
		: SpawnScale;
	return IsValid(UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		SlashSystem,
		SpawnLocation,
		SpawnRotation,
		EffectiveScale,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true));
}

bool AWeaponBase::ConsumeMatchingPredictedSkillSlash(
	UNiagaraSystem* SlashSystem,
	const FVector& SpawnLocation)
{
	if (HasAuthority() || !bHasPendingPredictedSkillSlash)
	{
		return false;
	}

	const ACharacterBase* OwningCharacter = GetOwningCharacter();
	const UWorld* World = GetWorld();
	const double CurrentWorldTime = World ? World->GetTimeSeconds() : -1.0;
	const bool bPredictionStillRecent = CurrentWorldTime >= 0.0
		&& PredictedSkillSlashWorldTime >= 0.0
		&& CurrentWorldTime - PredictedSkillSlashWorldTime <= 1.0;
	const bool bMatchesPrediction = OwningCharacter
		&& OwningCharacter->IsLocallyControlled()
		&& bPredictionStillRecent
		&& PredictedSkillSlashSystem.Get() == SlashSystem
		&& PredictedSkillSlashLocation.Equals(SpawnLocation, 100.0);

	if (bMatchesPrediction || !bPredictionStillRecent)
	{
		bHasPendingPredictedSkillSlash = false;
		PredictedSkillSlashSystem.Reset();
		PredictedSkillSlashWorldTime = -1.0;
	}

	return bMatchesPrediction;
}

UNiagaraComponent* AWeaponBase::ResolveSkillTrailComponent() const
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents(NiagaraComponents);

	UNiagaraComponent* SingleNiagaraComponent = nullptr;
	UNiagaraComponent* SingleNiagaraWithAsset = nullptr;
	int32 NiagaraComponentCount = 0;
	int32 NiagaraWithAssetCount = 0;

	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		++NiagaraComponentCount;
		if (NiagaraComponentCount == 1)
		{
			SingleNiagaraComponent = NiagaraComponent;
		}

		if (NiagaraComponent->GetAsset())
		{
			++NiagaraWithAssetCount;
			if (NiagaraWithAssetCount == 1)
			{
				SingleNiagaraWithAsset = NiagaraComponent;
			}
		}

		if (IsNamedSkillTrailComponent(NiagaraComponent))
		{
			return NiagaraComponent;
		}
	}

	if (NiagaraWithAssetCount == 1)
	{
		return SingleNiagaraWithAsset;
	}

	if (NiagaraComponentCount == 1)
	{
		return SingleNiagaraComponent;
	}

	return nullptr;
}

void AWeaponBase::MulticastDrawInterpolatedAttackTraceDebug_Implementation(
	const TArray<FVector>& StartLocations,
	const TArray<FVector>& EndLocations,
	const TArray<FHitResult>& Hits)
{
	DrawInterpolatedAttackTraceDebug(StartLocations, EndLocations, Hits);
}

void AWeaponBase::DrawInterpolatedAttackTraceDebug(
	const TArray<FVector>& StartLocations,
	const TArray<FVector>& EndLocations,
	const TArray<FHitResult>& Hits) const
{
	UWorld* World = GetWorld();
	if (!IsAttackDebugVisualizationEnabled() || !World)
	{
		return;
	}

	const FColor TraceColor = AttackTraceDebugTraceColor.ToFColor(true);
	const FColor HitColor = AttackTraceDebugHitColor.ToFColor(true);
	const FColor SweepColor = Hits.IsEmpty() ? TraceColor : HitColor;
	const float DrawTime = FMath::Max(0.0f, AttackTraceDebugDrawTime);
	const int32 LineCount = FMath::Min(StartLocations.Num(), EndLocations.Num());

	for (int32 LineIndex = 0; LineIndex < LineCount; ++LineIndex)
	{
		DrawDebugLine(World, StartLocations[LineIndex], EndLocations[LineIndex], SweepColor, false, DrawTime, 0, 2.0f);
	}

	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.GetActor())
		{
			continue;
		}

		const FVector HitLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
		DrawDebugSphere(World, HitLocation, 8.0f, 12, HitColor, false, DrawTime, 0, 3.0f);
	}
}

bool AWeaponBase::ApplyDamageFromAuthoritativeTrace(const FHitResult& HitResult)
{
	if (!HasAuthority() || !IsCurrentWeaponForOwner())
	{
		return false;
	}

	ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(
		HitResult.GetActor(),
		HitResult.GetComponent());
	return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
}

bool AWeaponBase::ApplyDamageFromAuthoritativeProjectileImpact(
	AActor* HitActor,
	const UPrimitiveComponent* HitComponent,
	const AArrowProjectileBase* ProjectileSource)
{
	if (!HasAuthority() || !IsCurrentWeaponForOwner() || !IsValid(ProjectileSource))
	{
		return false;
	}

	const ACharacterBase* SourceCharacter = GetOwningCharacter();
	const bool bOwnedBySourceCharacter =
		ProjectileSource->GetOwner() == SourceCharacter
		|| ProjectileSource->GetInstigator() == SourceCharacter;
	if (!SourceCharacter || !bOwnedBySourceCharacter)
	{
		return false;
	}

	ACharacterBase* TargetCharacter =
		PdCharacterHitValidation::ResolveWeaponDamageHit(HitActor, HitComponent);
	return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
}

bool AWeaponBase::ApplyDamageToTarget(AActor* TargetActor)
{
	if (!HasAuthority())
	{
		return false;
	}

	ACharacterBase* SourceCharacter = GetOwningCharacter();
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
	{
		return false;
	}

	UCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return false;
	}

	const bool bAppliedDamage = CombatComponent->ApplyWeaponDamageToTarget(TargetCharacter);
	if (bAppliedDamage)
	{
		ApplyActiveSkillAdditionalDamageToTarget(TargetCharacter);
	}
	return bAppliedDamage;
}
