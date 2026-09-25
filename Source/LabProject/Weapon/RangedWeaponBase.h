#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "Weapon/WeaponBase.h"
#include "RangedWeaponBase.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class LABPROJECT_API ARangedWeaponBase : public AWeaponBase
{
    GENERATED_BODY()

public:
    // Public API ------------------------------------------------------------------------------------------------------
    bool SupportsAimInput() const;
    bool CanUseRangedWeapon(const ACharacterBase* AttackingCharacter, bool bRequirePlayerAim) const;
    FGameplayTag GetAimCrosshairWidgetTag() const;
    const FWeaponAimCameraSettings& GetAimCameraSettings() const;

    // Event Handlers --------------------------------------------------------------------------------------------------
    virtual bool HandleAimStart(APdPlayer* PlayerCharacter);
    virtual void HandleAimEnd(APdPlayer* PlayerCharacter);

protected:
    // Internal Helpers ------------------------------------------------------------------------------------------------
    static TArray<TEnumAsByte<EObjectTypeQuery>> MakeRangedTraceObjectTypes(TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes);
    FVector GetAITargetAimLocation(const AActor* TargetActor) const;
    bool ApplyDamageFromAuthoritativeRangedTrace(const FHitResult& HitResult);

    bool CanServerUseRangedWeapon(const ACharacterBase* AttackingCharacter, bool bRequirePlayerAim) const;

    bool ResolveServerAimViewPoint(
        const APdPlayer* PlayerCharacter,
        const FVector& RequestedViewLocation,
        const FVector& RequestedViewDirection,
        FVector& OutViewLocation,
        FVector& OutViewDirection) const;

    bool ResolveAimTargetBeyondLaunchPoint(
        const FVector& ViewLocation,
        const FVector& ViewDirection,
        const FVector& LaunchStartLocation,
        float TraceRange,
        const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
        const TArray<AActor*>& ActorsToIgnore,
        EDrawDebugTrace::Type DebugDrawType,
        FVector& OutTargetLocation,
        FHitResult* OutAimHitResult = nullptr) const;
};
