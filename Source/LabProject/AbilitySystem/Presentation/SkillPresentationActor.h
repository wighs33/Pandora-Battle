#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillPresentationActor.generated.h"

class ACharacterBase;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USkillDefinition;

UENUM()
enum class ESkillPresentationFlags : uint8
{
	None = 0,
	DefaultFX = 1 << 0,
	CharacterOverlay = 1 << 1,
	Missile = 1 << 2,
	GroundFX = 1 << 3
};
ENUM_CLASS_FLAGS(ESkillPresentationFlags);

/**
 * Server-owned lifetime token for persistent ability presentation.
 * The actor replicates only stable presentation data; every machine creates and
 * destroys its own Niagara components and overlay entry from the same state.
 */
UCLASS(NotBlueprintable)
class LABPROJECT_API ASkillPresentationActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillPresentationActor();

	void InitializePresentation(
		ACharacterBase* InSourceCharacter,
		USkillDefinition* InSkillDefinition,
		ESkillPresentationFlags InFlags);

	void SetPresentationEnabled(ESkillPresentationFlags Flag, bool bEnabled);
	void SetMissileTargetActors(const TArray<AActor*>& InTargetActors);
	bool HasAnyPresentation() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_PresentationState();

	UFUNCTION()
	void HandleSourceDestroyed(AActor* DestroyedActor);

	void RefreshLocalPresentation();
	void CleanupLocalPresentation();
	void StartDefaultFX();
	void StopDefaultFX();
	void StartGroundFX();
	void StopGroundFX();
	void StartCharacterOverlay();
	void StopCharacterOverlay();
	void RefreshLocalMissiles();
	UNiagaraComponent* StartMissileForTarget(AActor* TargetActor);
	void StopMissileAtIndex(int32 Index);
	void StopMissiles();
	void UpdateLocalMissileTargets();
	void ApplyMissileTargetLocation(AActor* TargetActor, UNiagaraComponent* MissileComponent) const;
	bool ResolveMissileTargetLocation(const AActor* TargetActor, FVector& OutTargetLocation) const;
	ACharacterBase* ResolveSourceCharacter() const;
	FVector ResolveCharacterFloorLocation(const ACharacterBase* Character) const;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TObjectPtr<ACharacterBase> SourceCharacter;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TObjectPtr<USkillDefinition> SkillDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	uint8 PresentationFlags = static_cast<uint8>(ESkillPresentationFlags::None);

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TArray<TObjectPtr<AActor>> MissileTargetActors;

	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacterBase> LocalPresentationSourceCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LocalDefaultSocketNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LocalDefaultGroundNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LocalBorrowedSocketNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> BorrowedSocketPreviousAsset;

	UPROPERTY(Transient)
	FVector BorrowedSocketPreviousScale = FVector::OneVector;

	UPROPERTY(Transient)
	bool bBorrowedSocketWasActive = false;

	UPROPERTY(Transient)
	bool bDefaultAuraApplied = false;

	UPROPERTY(Transient)
	bool bCharacterOverlayApplied = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> LocalMissileTargetActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> LocalMissileNiagaraComponents;
};
