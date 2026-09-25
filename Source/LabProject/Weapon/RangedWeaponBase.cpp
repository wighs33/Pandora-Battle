#include "Weapon/RangedWeaponBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Common/CollisionChannels.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RangedWeaponBase)

bool ARangedWeaponBase::SupportsAimInput() const
{
    const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
    return ItemDefinition && ItemDefinition->WeaponData.Aim.bSupportsInput;
}

bool ARangedWeaponBase::CanUseRangedWeapon(const ACharacterBase* AttackingCharacter, const bool bRequirePlayerAim) const
{
    if (!SupportsAimInput()
        || !AttackingCharacter
        || AttackingCharacter != GetOwningCharacter()
        || !IsCurrentWeaponForOwner()
        || AttackingCharacter->IsStatusFrozen())
    {
        return false;
    }

    const UPdAbilitySystemComponent* AbilitySystemComponent = AttackingCharacter->GetPdAbilitySystemComponent();
    if (!AbilitySystemComponent
        || AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.0f
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Status_Frostbite)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Movement_Airborne)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_AOEAttack_Active)
        || AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_ShootProjectile_Active))
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
    const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
    return ItemDefinition ? ItemDefinition->WeaponData.Aim.CrosshairWidgetTag : FGameplayTag();
}

const FWeaponAimCameraSettings& ARangedWeaponBase::GetAimCameraSettings() const
{
    if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
    {
        return ItemDefinition->WeaponData.Aim.CameraSettings;
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

bool ARangedWeaponBase::ResolveServerAimViewPoint(
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
    const float MaxAcceptedViewDistance = ItemDefinition
        ? ItemDefinition->WeaponData.Aim.MaxAcceptedServerViewDistance
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

bool ARangedWeaponBase::ResolveAimTargetBeyondLaunchPoint(
    const FVector& ViewLocation,
    const FVector& ViewDirection,
    const FVector& LaunchStartLocation,
    float TraceRange,
    const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
    const TArray<AActor*>& ActorsToIgnore,
    EDrawDebugTrace::Type DebugDrawType,
    FVector& OutTargetLocation,
    FHitResult* OutAimHitResult) const
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
        const float HitForwardDistanceFromLaunch = FVector::DotProduct(HitLocation - LaunchStartLocation, SafeViewDirection);
        const bool bHitIsBehindLaunchPlane = HitForwardDistanceFromLaunch <= AimTraceAdvanceDistance;
        const bool bCharacterNonMeshHit = PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(
            HitResult.GetActor(),
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

FVector ARangedWeaponBase::GetAITargetAimLocation(const AActor* TargetActor) const
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

    ACharacterBase* TargetCharacter = PdCharacterHitValidation::ResolveWeaponDamageHit(
        HitResult.GetActor(),
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
    ObjectTypes.AddUnique(UEngineTypes::ConvertToObjectType(LabCollisionChannels::HitableBody()));
    return ObjectTypes;
}
