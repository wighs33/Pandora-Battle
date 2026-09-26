#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillVisualActor.generated.h"

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
 * 지속되는 능력 연출의 수명을 서버에서 관리한다.
 * 안정적인 연출 데이터만 복제하며, 각 실행 환경은 같은 상태를 바탕으로
 * 자신의 Niagara 컴포넌트와 오버레이 항목을 생성하고 제거한다.
 */
UCLASS(NotBlueprintable)
class LABPROJECT_API ASkillVisualActor : public AActor
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ASkillVisualActor();

	void InitializePresentation(
		ACharacterBase* InSourceCharacter,
		USkillDefinition* InSkillDefinition,
		ESkillPresentationFlags InFlags);

	void SetPresentationEnabled(ESkillPresentationFlags Flag, bool bEnabled);
	void SetMissileTargetActors(const TArray<AActor*>& InTargetActors);
	void SetMissileTargeting(FName InAimParameter, FName InTargetSocket);
	bool HasAnyPresentation() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_PresentationState();

	UFUNCTION()
	void HandleSourceDestroyed(AActor* DestroyedActor);

	// Internal Helpers ------------------------------------------------------------------------------------------------
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

private:
	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TObjectPtr<ACharacterBase> SourceCharacter;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TObjectPtr<USkillDefinition> SkillDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	uint8 PresentationFlags = static_cast<uint8>(ESkillPresentationFlags::None);

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	TArray<TObjectPtr<AActor>> MissileTargetActors;

	/** 미사일 액션이 전달한 연출용 표적 설정. 클라이언트는 실행 액션을 참조하지 않는다. */
	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	FName MissileAimParameter = TEXT("aim_position");
	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	FName MissileTargetSocket;

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
