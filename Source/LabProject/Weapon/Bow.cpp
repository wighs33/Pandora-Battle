#include "Weapon/Bow.h"

#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Item/ArrowProjectileBase.h"
#include "Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Bow)

DEFINE_LOG_CATEGORY_STATIC(LogBowWeaponBase, Log, All);

namespace
{
const TArray<TEnumAsByte<EObjectTypeQuery>>& GetDefaultBowTraceObjectTypes()
{
	static const TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes =
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic)
	};
	return ObjectTypes;
}
}

TSubclassOf<AActor> ABow::GetArrowActorClass() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.ArrowActorClass;
	}

	return nullptr;
}

FName ABow::GetArrowAttachSocketName() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.GetResolvedArrowAttachSocketName();
	}

	return FBowWeaponDefinitionData().GetResolvedArrowAttachSocketName();
}

float ABow::GetArrowTraceRange() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.TraceRange;
	}

	return 0.0f;
}

const TArray<TEnumAsByte<EObjectTypeQuery>>& ABow::GetBowTraceObjectTypes() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		if (!ItemDefinition->WeaponData.Bow.TraceObjectTypes.IsEmpty())
		{
			return ItemDefinition->WeaponData.Bow.TraceObjectTypes;
		}
	}

	return GetDefaultBowTraceObjectTypes();
}

bool ABow::HandleAimStart(APdPlayer* PlayerCharacter)
{
	const bool bWasAiming = PlayerCharacter && PlayerCharacter->IsWeaponAimActive();
	if (!Super::HandleAimStart(PlayerCharacter))
	{
		return false;
	}

	if (!bWasAiming)
	{
		bCanLaunchDrawnArrow = false;
		SpawnDrawnArrow(PlayerCharacter);
		PlayWeaponMontage();
	}

	return true;
}

void ABow::HandleAimEnd(APdPlayer* PlayerCharacter)
{
	Super::HandleAimEnd(PlayerCharacter);
	bCanLaunchDrawnArrow = false;
	DestroyDrawnArrow();
	StopWeaponMontage();

	if (!HasAuthority())
	{
		ServerHandleAimEnd();
	}
}

bool ABow::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return false;
	}

	if (!bCanLaunchDrawnArrow || !IsValid(CurrentDrawnArrow))
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!PlayerCharacter->GetWeaponAimViewPoint(ViewLocation, ViewDirection))
	{
		return false;
	}

	FVector LaunchStartLocation = FVector::ZeroVector;
	if (!TryGetArrowLaunchStartLocation(PlayerCharacter, LaunchStartLocation))
	{
		return false;
	}

	if (HasAuthority())
	{
		if (!LaunchArrowOnServer(PlayerCharacter, ViewLocation, ViewDirection, LaunchStartLocation))
		{
			return false;
		}
	}
	else
	{
		ServerLaunchArrow(ViewLocation, ViewDirection, LaunchStartLocation);
		DestroyDrawnArrow();
	}

	bCanLaunchDrawnArrow = false;

	const FName ResumeSectionName = GetConfiguredPrimaryAttackResumeWeaponMontageSectionName();
	if (!ResumeSectionName.IsNone())
	{
		JumpToWeaponMontageSectionAndResume(ResumeSectionName);
	}

	return true;
}

bool ABow::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
	if (NotifyName == WeaponAnimNotifyNames::HoldBow())
	{
		if (!SupportsAimInput() || !PlayerCharacter || !PlayerCharacter->IsWeaponAimActive() || !IsValid(CurrentDrawnArrow))
		{
			if (!PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
			{
				bCanLaunchDrawnArrow = false;
				DestroyDrawnArrow();
			}
			return false;
		}

		bCanLaunchDrawnArrow = true;
		return true;
	}

	if (NotifyName != WeaponAnimNotifyNames::RedrawBow())
	{
		return Super::OnWeaponAnimNotifyTiming(NotifyName, PlayerCharacter);
	}

	if (!SupportsAimInput() || !PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
	{
		bCanLaunchDrawnArrow = false;
		DestroyDrawnArrow();
		return false;
	}

	bCanLaunchDrawnArrow = false;
	const bool bArrowRefreshed = RefreshDrawnArrow(PlayerCharacter) != nullptr;
	const bool bMontagePlayed = PlayWeaponMontage();
	return bArrowRefreshed || bMontagePlayed;
}

void ABow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyDrawnArrow();
	Super::EndPlay(EndPlayReason);
}

UAnimMontage* ABow::GetConfiguredWeaponMontage() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		if (UAnimMontage* Montage = ItemDefinition->WeaponData.Bow.WeaponMontage.LoadSynchronous())
		{
			return Montage;
		}
	}

	return Super::GetConfiguredWeaponMontage();
}

FName ABow::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.AttackResumeSectionName;
	}

	return Super::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName();
}

AActor* ABow::SpawnDrawnArrow(APdPlayer* PlayerCharacter)
{
	if (IsValid(CurrentDrawnArrow))
	{
		return CurrentDrawnArrow;
	}

	CurrentDrawnArrow = SpawnArrowActor(PlayerCharacter, true);
	return CurrentDrawnArrow.Get();
}

AActor* ABow::SpawnArrowActor(APdPlayer* PlayerCharacter, bool bAttachToCharacter)
{
	TSubclassOf<AActor> ArrowClass = GetArrowActorClass();
	USkeletalMeshComponent* CharacterMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!PlayerCharacter || !CharacterMesh || !ArrowClass || !World)
	{
		return nullptr;
	}

	const FName AttachSocketName = ResolveArrowAttachSocketName();
	FTransform SpawnTransform = CharacterMesh->GetComponentTransform();
	if (AttachSocketName != NAME_None && CharacterMesh->DoesSocketExist(AttachSocketName))
	{
		SpawnTransform = CharacterMesh->GetSocketTransform(AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PlayerCharacter;
	SpawnParams.Instigator = PlayerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedArrow = World->SpawnActor<AActor>(ArrowClass, SpawnTransform, SpawnParams);
	if (!SpawnedArrow)
	{
		return nullptr;
	}

	SpawnedArrow->SetActorHiddenInGame(false);
	if (bAttachToCharacter)
	{
		SpawnedArrow->AttachToComponent(
			CharacterMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocketName);
	}

	return SpawnedArrow;
}

void ABow::DestroyDrawnArrow()
{
	if (!IsValid(CurrentDrawnArrow))
	{
		CurrentDrawnArrow = nullptr;
		return;
	}

	AActor* DrawnArrow = CurrentDrawnArrow.Get();
	CurrentDrawnArrow = nullptr;
	DrawnArrow->Destroy();
}

bool ABow::TryGetArrowLaunchStartLocation(const APdPlayer* PlayerCharacter, FVector& OutLocation) const
{
	const FName AttachSocketName = ResolveArrowAttachSocketName();
	if (TryGetOwnerMeshSocketLocation(PlayerCharacter, AttachSocketName, OutLocation))
	{
		return true;
	}

	if (IsValid(CurrentDrawnArrow))
	{
		OutLocation = CurrentDrawnArrow->GetActorLocation();
		return true;
	}

	OutLocation = GetActorLocation();
	return true;
}

bool ABow::ResolveServerArrowLaunchStartLocation(
	const APdPlayer* PlayerCharacter,
	const FVector& RequestedLaunchStartLocation,
	FVector& OutLocation) const
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const float MaxAcceptedLaunchStartDistance = ItemDefinition ? ItemDefinition->WeaponData.Bow.MaxAcceptedServerLaunchStartDistance : 0.0f;
	if (!RequestedLaunchStartLocation.IsNearlyZero()
		&& MaxAcceptedLaunchStartDistance > 0.0f
		&& FVector::DistSquared(RequestedLaunchStartLocation, PlayerCharacter->GetActorLocation())
			<= FMath::Square(MaxAcceptedLaunchStartDistance))
	{
		OutLocation = RequestedLaunchStartLocation;
		return true;
	}

	return TryGetArrowLaunchStartLocation(PlayerCharacter, OutLocation);
}

bool ABow::LaunchArrowOnServer(
	APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	const FVector& RequestedLaunchStartLocation)
{
	if (!HasAuthority() || !PlayerCharacter)
	{
		return false;
	}

	FVector LaunchStartLocation = FVector::ZeroVector;
	if (!ResolveServerArrowLaunchStartLocation(PlayerCharacter, RequestedLaunchStartLocation, LaunchStartLocation))
	{
		return false;
	}

	const FVector LaunchDirection = CalculateArrowLaunchDirection(
		PlayerCharacter,
		RequestedViewLocation,
		RequestedViewDirection,
		LaunchStartLocation);
	if (LaunchDirection.IsNearlyZero())
	{
		return false;
	}

	AActor* ArrowActor = CurrentDrawnArrow.Get();
	if (IsValid(ArrowActor))
	{
		ArrowActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		CurrentDrawnArrow = nullptr;
	}
	else
	{
		ArrowActor = SpawnArrowActor(PlayerCharacter, false);
	}

	if (ArrowActor)
	{
		ArrowActor->SetActorLocation(LaunchStartLocation);
		ArrowActor->SetActorRotation(LaunchDirection.Rotation());
	}

	AArrowProjectileBase* ArrowProjectile = Cast<AArrowProjectileBase>(ArrowActor);
	if (!ArrowProjectile)
	{
		return false;
	}

	return ArrowProjectile->LaunchArrowActor(LaunchDirection);
}

void ABow::ServerLaunchArrow_Implementation(
	FVector_NetQuantize RequestedViewLocation,
	FVector_NetQuantizeNormal RequestedViewDirection,
	FVector_NetQuantize RequestedLaunchStartLocation)
{
	LaunchArrowOnServer(Cast<APdPlayer>(GetOwningCharacter()), RequestedViewLocation, RequestedViewDirection, RequestedLaunchStartLocation);
}

void ABow::ServerHandleAimEnd_Implementation()
{
	bCanLaunchDrawnArrow = false;
	DestroyDrawnArrow();
	StopWeaponMontage();
}

AActor* ABow::RefreshDrawnArrow(APdPlayer* PlayerCharacter)
{
	if (!PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
	{
		return nullptr;
	}

	if (IsValid(CurrentDrawnArrow))
	{
		return CurrentDrawnArrow;
	}

	return SpawnDrawnArrow(PlayerCharacter);
}

FName ABow::ResolveArrowAttachSocketName() const
{
	return GetArrowAttachSocketName();
}

FVector ABow::CalculateArrowLaunchDirection(
	const APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	const FVector& RequestedLaunchStartLocation) const
{
	const float TraceRange = GetArrowTraceRange();
	if (!PlayerCharacter || TraceRange <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<APdPlayer*>(PlayerCharacter));
	ActorsToIgnore.Add(const_cast<ABow*>(this));
	if (IsValid(CurrentDrawnArrow))
	{
		ActorsToIgnore.Add(CurrentDrawnArrow.Get());
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!ResolveServerAimViewPoint(PlayerCharacter, RequestedViewLocation, RequestedViewDirection, ViewLocation, ViewDirection))
	{
		return FVector::ZeroVector;
	}

	const FVector ViewTraceEnd = ViewLocation + (ViewDirection.GetSafeNormal() * TraceRange);
	FHitResult ViewHitResult;
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const bool bViewHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		ViewLocation,
		ViewTraceEnd,
		GetBowTraceObjectTypes(),
		false,
		ActorsToIgnore,
		ItemDefinition ? ItemDefinition->WeaponData.Bow.AimTraceDebugDrawType.GetValue() : EDrawDebugTrace::None,
		ViewHitResult,
		true);

	const FVector AimTargetLocation = bViewHit ? ViewHitResult.Location : ViewTraceEnd;
	const FVector TraceStart = RequestedLaunchStartLocation;

	FHitResult HitResult;
	const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		TraceStart,
		AimTargetLocation,
		GetBowTraceObjectTypes(),
		false,
		ActorsToIgnore,
		ItemDefinition ? ItemDefinition->WeaponData.Bow.LaunchTraceDebugDrawType.GetValue() : EDrawDebugTrace::None,
		HitResult,
		true);

	const FVector TargetLocation = bHit ? HitResult.Location : AimTargetLocation;
	return (TargetLocation - TraceStart).GetSafeNormal();
}
