#include "Weapon/Gun.h"

#include "Character/PdCharacterBase.h"
#include "Character/PdPlayer.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayCueManager.h"
#include "GameplayEffectTypes.h"
#include "Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Gun)

DEFINE_LOG_CATEGORY_STATIC(LogGunWeaponBase, Log, All);

namespace
{
const TArray<TEnumAsByte<EObjectTypeQuery>>& GetDefaultGunTraceObjectTypes()
{
	static const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic)
	};
	return ObjectTypes;
}
}

bool AGun::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
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

bool AGun::HandlePrimaryAttackOnServer(
	APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection)
{
	if (!HasAuthority() || !SupportsAimInput() || !PlayerCharacter)
	{
		return false;
	}

	if (!TryConsumePrimaryAttackCooldown())
	{
		return false;
	}

	MulticastExecuteMuzzleFlashCue();

	FHitResult HitResult;
	if (TraceGunShot(PlayerCharacter, RequestedViewLocation, RequestedViewDirection, HitResult))
	{
		if (HasConfiguredImpactDecal() && ShouldSpawnImpactDecalForHit(HitResult))
		{
			const FVector ImpactLocation = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
			const FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, -RequestedViewDirection.GetSafeNormal());
			MulticastSpawnImpactDecal(ImpactLocation, ImpactNormal, MakeImpactDecalSize());
		}

		if (AActor* DamageTargetActor = ResolveDamageTargetActor(HitResult.GetActor()))
		{
			RequestServerApplyDamage(DamageTargetActor);
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

	ExecuteMuzzleFlashCue(Cast<APdPlayer>(GetOwningCharacter()));
}

void AGun::MulticastSpawnImpactDecal_Implementation(
	FVector_NetQuantize ImpactLocation,
	FVector_NetQuantizeNormal ImpactNormal,
	float DecalSize)
{
	SpawnImpactDecal(ImpactLocation, ImpactNormal, DecalSize);
}

void AGun::ExecuteMuzzleFlashCue(APdPlayer* PlayerCharacter) const
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

	APdCharacterBase* OwningCharacter = PlayerCharacter ? PlayerCharacter : GetOwningCharacter();
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
	UMaterialInterface* DecalMaterial = GunData.ImpactDecalMaterial.LoadSynchronous();
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

float AGun::GetGunFireInterval() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return FMath::Max(0.01f, ItemDefinition->WeaponData.Gun.FireInterval);
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

const TArray<TEnumAsByte<EObjectTypeQuery>>& AGun::GetGunTraceObjectTypes() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		if (!ItemDefinition->WeaponData.Gun.TraceObjectTypes.IsEmpty())
		{
			return ItemDefinition->WeaponData.Gun.TraceObjectTypes;
		}
	}

	return GetDefaultGunTraceObjectTypes();
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

bool AGun::TryGetGunAimTargetLocation(
	const FVector& ViewTraceStart,
	const FVector& ViewTraceDirection,
	float TraceRange,
	const TArray<AActor*>& ActorsToIgnore,
	FVector& OutTargetLocation) const
{
	if (TraceRange <= 0.0f)
	{
		return false;
	}

	const FVector SafeViewTraceDirection = ViewTraceDirection.GetSafeNormal();
	if (SafeViewTraceDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector ViewTraceEnd = ViewTraceStart + (SafeViewTraceDirection * TraceRange);

	FHitResult ViewHitResult;
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const bool bViewHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		ViewTraceStart,
		ViewTraceEnd,
		GetGunTraceObjectTypes(),
		false,
		ActorsToIgnore,
		ItemDefinition ? ItemDefinition->WeaponData.Gun.AimTraceDebugDrawType.GetValue() : EDrawDebugTrace::None,
		ViewHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);

	OutTargetLocation = bViewHit ? ViewHitResult.Location : ViewTraceEnd;
	return true;
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

	FVector AimTargetLocation = FVector::ZeroVector;
	if (!TryGetGunAimTargetLocation(ViewLocation, ViewDirection, TraceRange, ActorsToIgnore, AimTargetLocation))
	{
		return false;
	}

	FVector TraceStart = GetActorLocation();
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	if (ItemDefinition)
	{
		TryGetOwnerMeshSocketLocation(PlayerCharacter, ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName(), TraceStart);
	}

	return UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		TraceStart,
		AimTargetLocation,
		GetGunTraceRadius(),
		GetGunTraceObjectTypes(),
		false,
		ActorsToIgnore,
		ItemDefinition ? ItemDefinition->WeaponData.Gun.ShotTraceDebugDrawType.GetValue() : EDrawDebugTrace::None,
		OutHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);
}

AActor* AGun::ResolveDamageTargetActor(AActor* HitActor) const
{
	if (!IsValid(HitActor))
	{
		return nullptr;
	}

	if (Cast<APdCharacterBase>(HitActor))
	{
		return HitActor;
	}

	AActor* CurrentActor = HitActor;
	for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
	{
		AActor* OwnerActor = CurrentActor->GetOwner();
		if (Cast<APdCharacterBase>(OwnerActor))
		{
			return OwnerActor;
		}

		AActor* AttachParentActor = CurrentActor->GetAttachParentActor();
		if (Cast<APdCharacterBase>(AttachParentActor))
		{
			return AttachParentActor;
		}

		CurrentActor = OwnerActor ? OwnerActor : AttachParentActor;
	}

	return HitActor;
}
