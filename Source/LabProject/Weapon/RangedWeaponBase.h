#pragma once

#include "CoreMinimal.h"
#include "Definition/Item/WeaponDefinitionData.h"
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

    // 엔진 trace API는 비const 액터 목록만 받으므로, 사수와 무기를 무시 목록에 넣는 캐스트를 이 함수 한 곳에 둔다.
    TArray<AActor*> MakeShotIgnoredActors(AActor* ShooterCharacter) const;

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
        FHitResult* OutAimHitResult = nullptr,
        double RewindServerTime = -1.0) const;

    // 되감기 시각이 0 이상이면 기록된 캐릭터를 그 시각 위치로 판정하고, 아니면 현재 월드로 trace한다.
    bool LineTraceSingleForRangedAim(
        double RewindServerTime,
        const FVector& Start,
        const FVector& End,
        const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
        const TArray<AActor*>& ActorsToIgnore,
        EDrawDebugTrace::Type DebugDrawType,
        FHitResult& OutHitResult) const;

    bool SphereTraceMultiForRangedShot(
        double RewindServerTime,
        const FVector& Start,
        const FVector& End,
        float Radius,
        const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
        const TArray<AActor*>& ActorsToIgnore,
        EDrawDebugTrace::Type DebugDrawType,
        TArray<FHitResult>& OutHitResults) const;
};
