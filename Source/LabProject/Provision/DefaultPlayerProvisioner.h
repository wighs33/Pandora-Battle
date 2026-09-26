#pragma once

#include "CoreMinimal.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "DefaultPlayerProvisioner.generated.h"

class AController;
class APlayerController;
class APlayerState;
class APdPlayerState;
class UInventoryComponent;
class UPandoraComponent;
class UPandoraTreeComponent;
class USkinEquipmentComponent;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefaultPlayerProvisioned, APlayerController*);

/**
 * 플레이어에게 DA_DefaultProvision의 기본 지급 설정을 적용한다.
 *
 * 소유자가 정의 에셋을 로드하고 플레이 공간마다 지급 처리 객체 하나를 초기화한다.
 * 정의와 모드는 Shutdown까지 고정하며, 반복 초기화로 기존 지급 상태를 초기화하지 않는다.
 */
UCLASS(Transient)
class LABPROJECT_API UDefaultPlayerProvisioner : public UObject
{
	GENERATED_BODY()

public:
	FOnDefaultPlayerProvisioned OnPlayerProvisioned;

private:
	enum class EContentState : uint8 { NotStarted, Loading, Ready, Failed };
	struct FInventoryProvisionState
	{
		TWeakObjectPtr<UInventoryComponent> Inventory;
		bool bCompleted = false;
	};

	struct FInitializedPandoraState
	{
		TWeakObjectPtr<UPandoraComponent> PandoraComponent;
		TWeakObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;
	};

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual UWorld* GetWorld() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	bool Initialize(const UDefaultProvisionDefinition* InDefinition, EDefaultProvisionMode InMode);
	bool IsInitialized() const { return ProvisionDefinition != nullptr && !bShuttingDown; }
	void ProvisionPlayer(APlayerController* PlayerController);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	void Shutdown();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleContentLoaded();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	const UDefaultProvisionDefinition* GetDefinition() const;
	bool EnsureContentLoaded(APlayerController* PlayerController);
	bool TryProvisionPlayer(APlayerController* PlayerController);
	bool ApplyItems(APdPlayerState* PlayerState);
	bool ApplyModeValues(APdPlayerState* PlayerState);
	bool ApplyPandoras(APdPlayerState* PlayerState);
	bool ApplyGestures(
		APlayerController* PlayerController,
		APdPlayerState* PlayerState);
	void ScheduleRetry(APlayerController* PlayerController);
	void ClearRetryTimer(APlayerController* PlayerController);
	int32 GetInventoryItemQuantity(
		const UInventoryComponent* InventoryComponent,
		FPrimaryAssetId ItemDefinitionId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDefaultProvisionDefinition> ProvisionDefinition;

	TMap<TObjectKey<APlayerController>, FTimerHandle> PendingRetryTimers;
	TArray<TWeakObjectPtr<APlayerController>> PendingContentControllers;
	TArray<FPrimaryAssetId> RequiredContentIds;
	TSharedPtr<FStreamableHandle> ContentLoadHandle;
	EContentState ContentState = EContentState::NotStarted;
	EDefaultProvisionMode Mode = EDefaultProvisionMode::Lobby;
	TMap<TObjectKey<APlayerState>, FInventoryProvisionState> InventoryStates;
	TSet<TObjectKey<APlayerState>> InitializedModeValues;
	TMap<TObjectKey<APlayerState>, FInitializedPandoraState>
		InitializedPandoras;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<USkinEquipmentComponent>>
		InitializedGestureEquipment;
	bool bShuttingDown = false;
};
