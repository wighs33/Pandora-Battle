#include "Weapon/Gun.h"

#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Character/PdPlayer.h"
#include "Component/Player/CombatComponent.h"
#include "Common/CollisionChannels.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayCueManager.h"
#include "GameplayEffectTypes.h"
#include "Definition/Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Gun)

namespace
{
void AddUniqueGunTraceObjectType(TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, ECollisionChannel CollisionChannel)
{
	ObjectTypes.AddUnique(UEngineTypes::ConvertToObjectType(CollisionChannel));
}

TArray<TEnumAsByte<EObjectTypeQuery>> MakeGunTraceObjectTypes(TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes)
{
	ObjectTypes.RemoveAll(
		[](const TEnumAsByte<EObjectTypeQuery> ObjectType)
		{
			return UEngineTypes::ConvertToCollisionChannel(ObjectType) == ECC_Pawn;
		});
	AddUniqueGunTraceObjectType(ObjectTypes, ECC_WorldStatic);
	AddUniqueGunTraceObjectType(ObjectTypes, LabCollisionChannels::HitableBody());
	return ObjectTypes;
}
}

bool AGun::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	if (!CanUseRangedWeapon(PlayerCharacter, true))
	{
		return false;
	}

	const UCombatComponent* CombatComponent = PlayerCharacter->GetCombatComponent();
	if (!CombatComponent || !CombatComponent->CanAffordRangedWeaponAttackStamina())
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!PlayerCharacter->GetWeaponAimViewPoint(ViewLocation, ViewDirection))
	{
		return false;
	}

	if (!HasAuthority() && !TryConsumePrimaryAttackCooldown())
	{
		return false;
	}

	if (HasAuthority())
	{
		return HandlePrimaryAttackOnServer(PlayerCharacter, ViewLocation, ViewDirection);
	}

	ExecuteMuzzleFlashCue(PlayerCharacter);
	ServerHandlePrimaryAttack(ViewLocation, ViewDirection);
	return true;
}

bool AGun::HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| !IsValid(TargetActor))
	{

		return false;
	}

	return HandleAIPrimaryAttackOnServer(AttackingCharacter, TargetActor);
}

bool AGun::HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| TargetLocation.IsNearlyZero())
	{

		return false;
	}

	return HandleAIPrimaryAttackAtLocationOnServer(AttackingCharacter, TargetActor, TargetLocation);
}

bool AGun::HandlePrimaryAttackOnServer(
	APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection)
{
	if (!CanServerUseRangedWeapon(PlayerCharacter, true))
	{
		return false;
	}

	UCombatComponent* CombatComponent = PlayerCharacter->GetCombatComponent();
	if (!CombatComponent || !CombatComponent->CanAffordRangedWeaponAttackStamina())
	{
		return false;
	}

	if (!TryConsumePrimaryAttackCooldown())
	{
		return false;
	}

	if (!CombatComponent->TryCommitRangedWeaponAttackStamina())
	{
		return false;
	}

	MulticastExecuteMuzzleFlashCue();

	FHitResult HitResult;
	if (TraceGunShot(PlayerCharacter, RequestedViewLocation, RequestedViewDirection, HitResult))
	{
		const bool bHitFriendlyTarget =
			IsFriendlyDamageTargetActor(HitResult.GetActor(), HitResult.GetComponent());
		if (!bHitFriendlyTarget && HasConfiguredImpactDecal() && ShouldSpawnImpactDecalForHit(HitResult))
		{
			const FVector ImpactLocation = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
			const FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, -RequestedViewDirection.GetSafeNormal());
			MulticastSpawnImpactDecal(ImpactLocation, ImpactNormal, MakeImpactDecalSize());
		}

		if (!bHitFriendlyTarget)
		{
			if (ResolveDamageTargetActor(HitResult.GetActor(), HitResult.GetComponent()))
			{
				ApplyDamageFromAuthoritativeRangedTrace(HitResult);
			}
		}
	}

	return true;
}

bool AGun::HandleAIPrimaryAttackOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| !IsValid(TargetActor))
	{
		return false;
	}

	return HandleAIPrimaryAttackAtLocationOnServer(AttackingCharacter, TargetActor, GetAITargetAimLocation(TargetActor));
}

bool AGun::HandleAIPrimaryAttackAtLocationOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| TargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (!TryConsumePrimaryAttackCooldown())
	{
		return false;
	}

	MulticastExecuteMuzzleFlashCue();

	FHitResult HitResult;
	FVector ShotDirection = FVector::ForwardVector;
	if (TraceAIGunShotAtLocation(AttackingCharacter, TargetLocation, HitResult, ShotDirection))
	{
		const bool bHitFriendlyTarget =
			IsFriendlyDamageTargetActor(HitResult.GetActor(), HitResult.GetComponent());
		if (!bHitFriendlyTarget && HasConfiguredImpactDecal() && ShouldSpawnImpactDecalForHit(HitResult))
		{
			const FVector ImpactLocation = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
			const FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, -ShotDirection.GetSafeNormal());
			MulticastSpawnImpactDecal(ImpactLocation, ImpactNormal, MakeImpactDecalSize());
		}

		if (!bHitFriendlyTarget && ResolveDamageTargetActor(HitResult.GetActor(), HitResult.GetComponent()))
		{
			ApplyDamageFromAuthoritativeRangedTrace(HitResult);
		}
	}

	return true;
}

void AGun::ServerHandlePrimaryAttack_Implementation(
	FVector_NetQuantize RequestedViewLocation,
	FVector_NetQuantizeNormal RequestedViewDirection)
{
	HandlePrimaryAttackOnServer(Cast<APdPlayer>(GetOwningCharacter()), RequestedViewLocation, RequestedViewDirection);
}

void AGun::MulticastExecuteMuzzleFlashCue_Implementation()
{
	if (ShouldSkipMulticastMuzzleFlashCue())
	{
		return;
	}

	ExecuteMuzzleFlashCue(GetOwningCharacter());
}

void AGun::MulticastSpawnImpactDecal_Implementation(
	FVector_NetQuantize ImpactLocation,
	FVector_NetQuantizeNormal ImpactNormal,
	float DecalSize)
{
	SpawnImpactDecal(ImpactLocation, ImpactNormal, DecalSize);
}

void AGun::ExecuteMuzzleFlashCue(ACharacterBase* Character) const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	if (!ItemDefinition)
	{
		return;
	}

	const FGunWeaponDefinitionData& GunData = ItemDefinition->WeaponData.Gun;
	FGameplayTag MuzzleFlashCueTag = GunData.MuzzleFlashCueTag;
	if (!MuzzleFlashCueTag.IsValid())
	{
		return;
	}

	ACharacterBase* OwningCharacter = Character ? Character : GetOwningCharacter();
	if (!OwningCharacter)
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.Instigator = OwningCharacter;
	CueParameters.EffectCauser = const_cast<AGun*>(this);
	CueParameters.SourceObject = ItemDefinition;
	CueParameters.TargetAttachComponent = WeaponMesh;

	const FName MuzzleSocketName = GunData.GetResolvedMuzzleSocketName();
	if (WeaponMesh && !MuzzleSocketName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		const FTransform MuzzleTransform = WeaponMesh->GetSocketTransform(MuzzleSocketName, RTS_World);
		CueParameters.Location = MuzzleTransform.GetLocation();
		CueParameters.Normal = MuzzleTransform.GetRotation().GetForwardVector();
	}
	else if (WeaponMesh)
	{
		CueParameters.Location = WeaponMesh->GetComponentLocation();
		CueParameters.Normal = WeaponMesh->GetForwardVector();
	}
	else
	{
		CueParameters.Location = GetActorLocation();
		CueParameters.Normal = GetActorForwardVector();
	}

	UGameplayCueManager::ExecuteGameplayCue_NonReplicated(OwningCharacter, MuzzleFlashCueTag, CueParameters);
}

bool AGun::ShouldSkipMulticastMuzzleFlashCue() const
{
	const APdPlayer* OwningPlayer = Cast<APdPlayer>(GetOwningCharacter());
	return !HasAuthority() && OwningPlayer && OwningPlayer->IsLocallyControlled();
}

bool AGun::HasConfiguredImpactDecal() const
{
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	return ItemDefinition && !ItemDefinition->WeaponData.Gun.ImpactDecalMaterial.IsNull();
}

bool AGun::ShouldSpawnImpactDecalForHit(const FHitResult& HitResult) const
{
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	if (!ItemDefinition || !HitComponent)
	{
		return false;
	}

	const EObjectTypeQuery HitObjectType = UEngineTypes::ConvertToObjectType(HitComponent->GetCollisionObjectType());
	return ItemDefinition->WeaponData.Gun.ImpactDecalObjectTypes.Contains(HitObjectType);
}

float AGun::MakeImpactDecalSize() const
{
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	if (!ItemDefinition)
	{
		return 0.0f;
	}

	const FVector2D& SizeRange = ItemDefinition->WeaponData.Gun.ImpactDecalSizeRange;
	const float MinSize = FMath::Max(0.0f, FMath::Min(SizeRange.X, SizeRange.Y));
	const float MaxSize = FMath::Max(MinSize, FMath::Max(SizeRange.X, SizeRange.Y));
	return FMath::IsNearlyEqual(MinSize, MaxSize) ? MinSize : FMath::FRandRange(MinSize, MaxSize);
}

void AGun::SpawnImpactDecal(const FVector& ImpactLocation, const FVector& ImpactNormal, float DecalSize) const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	if (!ItemDefinition || ItemDefinition->WeaponData.Gun.ImpactDecalMaterial.IsNull() || DecalSize <= 0.0f)
	{
		return;
	}

	const FGunWeaponDefinitionData& GunData = ItemDefinition->WeaponData.Gun;
	UMaterialInterface* DecalMaterial = GunData.ImpactDecalMaterial.Get();
	if (!DecalMaterial)
	{
		return;
	}

	const FVector SafeImpactNormal = ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector DecalSizeVector(
		FMath::Max(0.0f, GunData.ImpactDecalDepth),
		DecalSize,
		DecalSize);
	const FRotator DecalRotation = (SafeImpactNormal.Rotation() + GunData.ImpactDecalRotationOffset).GetNormalized();
	const float LifeSpan = FMath::Max(0.0f, GunData.ImpactDecalLifeSpan);

	UGameplayStatics::SpawnDecalAtLocation(World, DecalMaterial, DecalSizeVector, ImpactLocation, DecalRotation, LifeSpan);
}

bool AGun::SupportsAutomaticFire() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Gun.bEnableAutomaticFire;
	}

	return false;
}

float AGun::GetAutomaticFireInterval() const
{
	return GetGunFireInterval();
}

bool AGun::ShouldTriggerHitReactOnDamage() const
{
	return false;
}

float AGun::GetGunFireInterval() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		const float BaseFireInterval = FMath::Max(0.01f, ItemDefinition->WeaponData.Gun.FireInterval);
		return FMath::Max(0.01f, BaseFireInterval / GetWeaponAttackSpeedPlayRate());
	}

	return 0.0f;
}

bool AGun::TryConsumePrimaryAttackCooldown()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	const float FireInterval = GetGunFireInterval();
	if (FireInterval <= 0.0f)
	{
		NextPrimaryAttackTimeSeconds = CurrentTimeSeconds;
		return true;
	}

	if (NextPrimaryAttackTimeSeconds > 0.0f && CurrentTimeSeconds + UE_KINDA_SMALL_NUMBER < NextPrimaryAttackTimeSeconds)
	{
		return false;
	}

	const bool bHasScheduledFireTime = NextPrimaryAttackTimeSeconds > 0.0f;
	const bool bPreserveCadence = bHasScheduledFireTime
		&& CurrentTimeSeconds - NextPrimaryAttackTimeSeconds <= FireInterval + UE_KINDA_SMALL_NUMBER;
	const float ScheduleBaseTime = bPreserveCadence ? NextPrimaryAttackTimeSeconds : CurrentTimeSeconds;
	NextPrimaryAttackTimeSeconds = ScheduleBaseTime + FireInterval;
	return true;
}

TArray<TEnumAsByte<EObjectTypeQuery>> AGun::GetGunTraceObjectTypes() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return MakeGunTraceObjectTypes(ItemDefinition->WeaponData.Gun.TraceObjectTypes);
	}

	return MakeGunTraceObjectTypes({});
}

float AGun::GetGunTraceRange() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Gun.TraceRange;
	}

	return 0.0f;
}

float AGun::GetGunTraceRadius() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return FMath::Max(0.0f, ItemDefinition->WeaponData.Gun.TraceRadius);
	}

	return 0.0f;
}

FVector AGun::GetGunTraceStartLocation(const ACharacterBase* Character) const
{
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const FName MuzzleSocketName = ItemDefinition ? ItemDefinition->WeaponData.Gun.GetResolvedMuzzleSocketName() : NAME_None;
	if (WeaponMesh && !MuzzleSocketName.IsNone() && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocketName);
	}

	if (ItemDefinition)
	{
		FVector AttachSocketLocation = FVector::ZeroVector;
		if (TryGetOwnerMeshSocketLocation(Character, ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName(), AttachSocketLocation))
		{
			return AttachSocketLocation;
		}
	}

	return GetActorLocation();
}

FVector AGun::GetAITargetAimLocation(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	TargetActor->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);

	FVector AimLocation = TargetActor->GetActorLocation();
	AimLocation.Z += FMath::Max(TargetHalfHeight * 0.5f, 0.0f);
	return AimLocation;
}

bool AGun::TraceAIGunShotAtLocation(
	ACharacterBase* AttackingCharacter,
	const FVector& TargetLocation,
	FHitResult& OutHitResult,
	FVector& OutShotDirection) const
{
	const float TraceRange = GetGunTraceRange();
	if (!AttackingCharacter || TargetLocation.IsNearlyZero() || TraceRange <= 0.0f)
	{
		return false;
	}

	const FVector TraceStart = GetGunTraceStartLocation(AttackingCharacter);
	OutShotDirection = (TargetLocation - TraceStart).GetSafeNormal();
	if (OutShotDirection.IsNearlyZero())
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AttackingCharacter);
	ActorsToIgnore.Add(const_cast<AGun*>(this));

	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const EDrawDebugTrace::Type ShotTraceDebugDrawType = IsAttackDebugVisualizationEnabled() && ItemDefinition
		? ItemDefinition->WeaponData.Gun.ShotTraceDebugDrawType.GetValue()
		: EDrawDebugTrace::None;

	const FVector TraceEnd = TraceStart + (OutShotDirection * TraceRange);
	const float TraceRadius = GetGunTraceRadius();
	const TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes =
		GetGunTraceObjectTypes();
	TArray<FHitResult> HitResults;
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		TraceStart,
		TraceEnd,
		TraceRadius,
		TraceObjectTypes,
		false,
		ActorsToIgnore,
		ShotTraceDebugDrawType,
		HitResults,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);
	return SelectFirstValidGunImpact(HitResults, OutHitResult);
}

bool AGun::TraceGunShot(
	APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	FHitResult& OutHitResult) const
{
	const float TraceRange = GetGunTraceRange();
	if (!PlayerCharacter || TraceRange <= 0.0f)
	{
		return false;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(PlayerCharacter);
	ActorsToIgnore.Add(const_cast<AGun*>(this));

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!ResolveServerAimViewPoint(PlayerCharacter, RequestedViewLocation, RequestedViewDirection, ViewLocation, ViewDirection))
	{
		return false;
	}

	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector TraceStart = GetGunTraceStartLocation(PlayerCharacter);
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes = GetGunTraceObjectTypes();
	const EDrawDebugTrace::Type AimTraceDebugDrawType = IsAttackDebugVisualizationEnabled() && ItemDefinition
		? ItemDefinition->WeaponData.Gun.AimTraceDebugDrawType.GetValue()
		: EDrawDebugTrace::None;
	FVector AimTargetLocation = FVector::ZeroVector;
	FHitResult AimHitResult;
	if (!ResolveAimTargetBeyondLaunchPoint(
		ViewLocation,
		SafeViewDirection,
		TraceStart,
		TraceRange,
		TraceObjectTypes,
		ActorsToIgnore,
		AimTraceDebugDrawType,
		AimTargetLocation,
		&AimHitResult))
	{
		return false;
	}

	const FVector ShotDirection = (AimTargetLocation - TraceStart).GetSafeNormal();
	if (ShotDirection.IsNearlyZero()
		|| FVector::DotProduct(ShotDirection, SafeViewDirection) <= 0.0f)
	{
		return false;
	}
	const float TargetDistance = FVector::Distance(TraceStart, AimTargetLocation);
	const float TraceDistance = FMath::Min(
		TraceRange,
		TargetDistance + GetGunTraceRadius() + 1.0f);
	const FVector TraceEnd = TraceStart + (ShotDirection * TraceDistance);

	const EDrawDebugTrace::Type ShotTraceDebugDrawType = IsAttackDebugVisualizationEnabled() && ItemDefinition
		? ItemDefinition->WeaponData.Gun.ShotTraceDebugDrawType.GetValue()
		: EDrawDebugTrace::None;

	const float TraceRadius = GetGunTraceRadius();
	TArray<FHitResult> HitResults;
	UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		TraceStart,
		TraceEnd,
		TraceRadius,
		TraceObjectTypes,
		false,
		ActorsToIgnore,
		ShotTraceDebugDrawType,
		HitResults,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);
	if (SelectFirstValidGunImpact(HitResults, OutHitResult))
	{
		return true;
	}

	// In third-person aiming, the camera ray can hit the character mesh while the
	// short muzzle-to-crosshair convergence ray narrowly misses its physics bodies.
	// Reuse only that exact primary-mesh aim hit after the muzzle trace confirms that
	// no valid world or character impact blocked the path. Capsules and interaction
	// components remain ineligible for damage.
	const ACharacterBase* AimDamageCharacter =
		PdCharacterHitValidation::ResolveWeaponDamageHit(
			AimHitResult.GetActor(),
			AimHitResult.GetComponent());
	if (!AimDamageCharacter)
	{
		return false;
	}

	const FVector AimImpactLocation = AimHitResult.ImpactPoint.IsNearlyZero()
		? AimHitResult.Location
		: AimHitResult.ImpactPoint;
	const float AimImpactForwardDistance =
		FVector::DotProduct(AimImpactLocation - TraceStart, ShotDirection);
	if (AimImpactForwardDistance <= 0.0f
		|| AimImpactForwardDistance > TraceDistance + TraceRadius + 1.0f)
	{
		return false;
	}

	OutHitResult = AimHitResult;
	return true;
}

bool AGun::SelectFirstValidGunImpact(
	const TArray<FHitResult>& HitResults,
	FHitResult& OutHitResult) const
{
	for (const FHitResult& HitResult : HitResults)
	{
		const bool bRelatedToCharacter =
			PdCharacterHitValidation::ResolveRelatedCharacter(
				HitResult.GetActor(),
				HitResult.GetComponent()) != nullptr;
		if (bRelatedToCharacter
			&& !PdCharacterHitValidation::ResolveWeaponDamageHit(
				HitResult.GetActor(),
				HitResult.GetComponent()))
		{
			continue;
		}

		OutHitResult = HitResult;
		return true;
	}

	OutHitResult = FHitResult();
	return false;
}

bool AGun::IsFriendlyDamageTargetActor(
	AActor* HitActor,
	const UPrimitiveComponent* HitComponent) const
{
	const ACharacterBase* SourceCharacter = GetOwningCharacter();
	if (!SourceCharacter || !IsValid(HitActor))
	{
		return false;
	}

	const ACharacterBase* TargetCharacter =
		PdCharacterHitValidation::ResolveWeaponDamageHit(HitActor, HitComponent);
	if (!TargetCharacter)
	{
		return false;
	}

	return !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter);
}

AActor* AGun::ResolveDamageTargetActor(
	AActor* HitActor,
	const UPrimitiveComponent* HitComponent) const
{
	if (!IsValid(HitActor))
	{
		return nullptr;
	}

	ACharacterBase* TargetCharacter =
		PdCharacterHitValidation::ResolveWeaponDamageHit(HitActor, HitComponent);
	const ACharacterBase* SourceCharacter = GetOwningCharacter();
	return TargetCharacter
		&& (!SourceCharacter || SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
			? TargetCharacter
			: nullptr;
}
