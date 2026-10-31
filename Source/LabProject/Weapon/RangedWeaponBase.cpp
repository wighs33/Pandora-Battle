#include "Weapon/RangedWeaponBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Character/LagCompensationSubsystem.h"
#include "Character/PdPlayer.h"
#include "Common/PdGameplayTags.h"
#include "Common/CollisionChannels.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedWeaponBase)

bool ARangedWeaponBase::SupportsAimInput() const
{
	const FItemFragment_Weapon* WeaponData = FindItemFragment<FItemFragment_Weapon>(GetSourceItemDefinition());
	return WeaponData && WeaponData->Aim.bSupportsInput;
}

bool ARangedWeaponBase::CanUseRangedWeapon(const ACharacterBase* AttackingCharacter, const bool bRequirePlayerAim) const
{
	if (!SupportsAimInput() || !AttackingCharacter || AttackingCharacter != GetOwningCharacter()
		|| !IsCurrentWeaponForOwner() || AttackingCharacter->IsStatusFrozen())
	{
		return false;
	}

	const UPdAbilitySystemComponent* AbilitySystemComponent = AttackingCharacter->GetPdAbilitySystemComponent();
	if (!AbilitySystemComponent
		|| AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::State_Dead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::Status_Frostbite)
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::State_Movement_Airborne)
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::GameplayAbility_AOEAttack_Active)
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::GameplayAbility_ShootProjectile_Active))
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = AttackingCharacter->GetCharacterMovement();
	if (MovementComponent && MovementComponent->IsFalling())
	{
		return false;
	}

	if (!bRequirePlayerAim)
	{
		return true;
	}

	const APdPlayer* PlayerCharacter = Cast<APdPlayer>(AttackingCharacter);
	return PlayerCharacter && PlayerCharacter->IsWeaponAimActive();
}

bool ARangedWeaponBase::CanServerUseRangedWeapon(const ACharacterBase* AttackingCharacter, const bool bRequirePlayerAim) const
{
	return HasAuthority() && CanUseRangedWeapon(AttackingCharacter, bRequirePlayerAim);
}

FGameplayTag ARangedWeaponBase::GetAimCrosshairWidgetTag() const
{
	const FItemFragment_Weapon* WeaponData = FindItemFragment<FItemFragment_Weapon>(GetSourceItemDefinition());
	return WeaponData ? WeaponData->Aim.CrosshairWidgetTag : FGameplayTag();
}

const FWeaponAimCameraSettings& ARangedWeaponBase::GetAimCameraSettings() const
{
	if (const FItemFragment_Weapon* WeaponData = FindItemFragment<FItemFragment_Weapon>(GetSourceItemDefinition()))
	{
		return WeaponData->Aim.CameraSettings;
	}

	static const FWeaponAimCameraSettings DefaultAimCameraSettings;
	return DefaultAimCameraSettings;
}

bool ARangedWeaponBase::HandleAimStart(APdPlayer* PlayerCharacter)
{
	if (!CanUseRangedWeapon(PlayerCharacter, false))
	{
		return false;
	}

	PlayerCharacter->SetWeaponAimActive(true, GetAimCameraSettings());
	return true;
}

void ARangedWeaponBase::HandleAimEnd(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->SetWeaponAimActive(false, GetAimCameraSettings());
}

bool ARangedWeaponBase::ResolveServerAimViewPoint(const APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation, const FVector& RequestedViewDirection, FVector& OutViewLocation,
	FVector& OutViewDirection) const
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const FVector RequestedDirection = RequestedViewDirection.GetSafeNormal();
	const FItemFragment_Weapon* WeaponData = FindItemFragment<FItemFragment_Weapon>(GetSourceItemDefinition());
	const float MaxAcceptedViewDistance = WeaponData
		? WeaponData->Aim.MaxAcceptedServerViewDistance
		: 0.0f;

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

bool ARangedWeaponBase::ResolveAimTargetBeyondLaunchPoint(const FVector& ViewLocation, const FVector& ViewDirection,
	const FVector& LaunchStartLocation, float TraceRange, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DebugDrawType, FVector& OutTargetLocation,
	FHitResult* OutAimHitResult, const TOptional<double>& RewindTime) const
{
	if (OutAimHitResult)
	{
		*OutAimHitResult = FHitResult();
	}

	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero() || TraceRange <= 0.0f)
	{
		return false;
	}

	const float CameraToLaunchDistance = FVector::Distance(ViewLocation, LaunchStartLocation);
	const FVector AimTraceEnd = ViewLocation + SafeViewDirection * (CameraToLaunchDistance + TraceRange);
	FVector AimTraceStart = ViewLocation;
	TArray<AActor*> AimActorsToIgnore = ActorsToIgnore;

	constexpr int32 MaxSkippedAimObstructions = 16;
	constexpr float AimTraceAdvanceDistance = 2.0f;
	for (int32 AttemptIndex = 0; AttemptIndex < MaxSkippedAimObstructions; ++AttemptIndex)
	{
		FHitResult HitResult;
		const bool bHit = LineTraceSingleForRangedAim(RewindTime, AimTraceStart, AimTraceEnd, ObjectTypes,
			AimActorsToIgnore, DebugDrawType, HitResult);

		if (!bHit)
		{
			OutTargetLocation = AimTraceEnd;
			return true;
		}

		const FVector HitLocation = HitResult.Location;
		const float HitForwardDistanceFromLaunch = FVector::DotProduct(HitLocation - LaunchStartLocation, SafeViewDirection);
		const bool bHitIsBehindLaunchPlane = HitForwardDistanceFromLaunch <= AimTraceAdvanceDistance;
		const bool bCharacterNonMeshHit = CharacterHitValidation::IsCharacterRelatedNonMeshHit(HitResult.GetActor(),
			HitResult.GetComponent());

		if (!bHitIsBehindLaunchPlane && !bCharacterNonMeshHit)
		{
			OutTargetLocation = HitLocation;
			if (OutAimHitResult)
			{
				*OutAimHitResult = HitResult;
			}
			return true;
		}

		if (bHitIsBehindLaunchPlane && IsValid(HitResult.GetActor()))
		{
			AimActorsToIgnore.AddUnique(HitResult.GetActor());
		}

		const float RemainingTraceDistance = FVector::DotProduct(AimTraceEnd - HitLocation, SafeViewDirection);
		if (RemainingTraceDistance <= AimTraceAdvanceDistance)
		{
			break;
		}

		AimTraceStart = HitLocation + SafeViewDirection * AimTraceAdvanceDistance;
	}

	OutTargetLocation = AimTraceEnd;
	return true;
}

bool ARangedWeaponBase::LineTraceSingleForRangedAim(const TOptional<double>& RewindTime, const FVector& Start,
	const FVector& End, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, const TArray<AActor*>& ActorsToIgnore,
	const EDrawDebugTrace::Type DebugDrawType, FHitResult& OutHitResult) const
{
	if (const ULagCompensationSubsystem* LagCompensation = RewindTime ? ULagCompensationSubsystem::Get(this) : nullptr)
	{
		TArray<FHitResult> Hits;
		LagCompensation->TraceAtTime(*RewindTime, Start, End, 0.0f, ObjectTypes, ActorsToIgnore, DebugDrawType, Hits);
		OutHitResult = Hits.IsEmpty() ? FHitResult() : Hits[0];
		return !Hits.IsEmpty();
	}

	return UKismetSystemLibrary::LineTraceSingleForObjects(this, Start, End, ObjectTypes, false, ActorsToIgnore,
		DebugDrawType, OutHitResult, true);
}

bool ARangedWeaponBase::SphereTraceMultiForRangedShot(const TOptional<double>& RewindTime, const FVector& Start,
	const FVector& End, const float Radius, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore, const EDrawDebugTrace::Type DebugDrawType,
	TArray<FHitResult>& OutHitResults) const
{
	if (const ULagCompensationSubsystem* LagCompensation = RewindTime ? ULagCompensationSubsystem::Get(this) : nullptr)
	{
		LagCompensation->TraceAtTime(*RewindTime, Start, End, Radius, ObjectTypes, ActorsToIgnore, DebugDrawType, OutHitResults);
		return !OutHitResults.IsEmpty();
	}

	return UKismetSystemLibrary::SphereTraceMultiForObjects(this, Start, End, Radius, ObjectTypes, false,
		ActorsToIgnore, DebugDrawType, OutHitResults, true);
}

TArray<AActor*> ARangedWeaponBase::MakeShotIgnoredActors(AActor* ShooterCharacter) const
{
	return { ShooterCharacter, const_cast<ARangedWeaponBase*>(this) };
}

FVector ARangedWeaponBase::GetAITargetAimLocation(const AActor* TargetActor)
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

bool ARangedWeaponBase::ApplyDamageFromAuthoritativeRangedTrace(const FHitResult& HitResult)
{
	if (!HasAuthority() || !IsCurrentWeaponForOwner())
	{
		return false;
	}

	ACharacterBase* TargetCharacter = CharacterHitValidation::ResolveWeaponDamageHit(HitResult.GetActor(),
		HitResult.GetComponent());
	return TargetCharacter && ApplyDamageToTarget(TargetCharacter);
}

TArray<TEnumAsByte<EObjectTypeQuery>> ARangedWeaponBase::MakeRangedTraceObjectTypes(TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes)
{
	ObjectTypes.RemoveAll([](const TEnumAsByte<EObjectTypeQuery> ObjectType)
	{
		return UEngineTypes::ConvertToCollisionChannel(ObjectType) == ECC_Pawn;
	});
	ObjectTypes.AddUnique(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.AddUnique(UEngineTypes::ConvertToObjectType(CollisionChannels::HitableBody()));
	return ObjectTypes;
}
