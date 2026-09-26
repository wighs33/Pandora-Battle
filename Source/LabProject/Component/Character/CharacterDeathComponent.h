#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterDeathComponent.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * 캐릭터 공통 사망·피해 연출·래그돌·디졸브·리스폰 복구 상태를 관리한다.
 * 게임별 사망 재정의는 소유 캐릭터에 유지하고, ACharacterBase를 통해 이 컴포넌트를 호출한다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UCharacterDeathComponent();

	void ApplySettings(const FCharacterDeathSettings& InSettings);
	void InitializeDeathRuntime();
	void ShutdownDeathRuntime();
	void TickRuntime(float DeltaSeconds);
	bool NeedsCharacterTick() const { return bDeathDissolveActive; }

	bool IsDeathHandled() const { return bDeathHandled; }
	void ApplyDeathPhysics();
	void ResetDeathStateForRespawn();

	void StartDeathDissolveLocal(float DurationSeconds);
	void ResetDeathDissolve();
	float GetSafeDissolveDuration(float RequestedDuration) const;
	void ClearCharacterOverlayMaterialLocal();

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleDeadTagChanged(
		int32 NewCount,
		UAbilitySystemComponent* BoundAbilitySystemComponent);
	void HandleRemoteDeath();

	void HandleDamageTaken(float DamageAmount, bool bCriticalHit);
	void HandleRemoteDamageTaken(
		float DamageAmount,
		bool bCriticalHit,
		FVector WorldLocation);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* GetCharacterOwner() const;
	void CacheInitialRespawnState();
	void InitializeDeathDissolveMaterials();
	void UpdateDeathDissolve(float DeltaSeconds);
	void SetDeathDissolveValue(float DissolveValue);
	void ConfigureWeaponDamageMesh(USkeletalMeshComponent* CharacterMesh) const;

private:
	UPROPERTY(Transient)
	FCharacterDeathSettings Settings;

	UPROPERTY(Transient)
	bool bDeathHandled = false;

	UPROPERTY(Transient)
	bool bHasCachedRespawnInitialState = false;

	UPROPERTY(Transient)
	FTransform InitialMeshRelativeTransform = FTransform::Identity;

	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> InitialCapsuleCollisionEnabled =
		ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> InitialMeshCollisionEnabled =
		ECollisionEnabled::QueryOnly;

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> InitialRespawnMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DeathDissolveMaterialInstances;

	UPROPERTY(Transient)
	bool bDeathDissolveActive = false;

	UPROPERTY(Transient)
	float DeathDissolveElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	float DeathDissolveDurationSeconds = 0.0f;
};
