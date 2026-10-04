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
class UStatUpgradeComponent;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDefaultPlayerProvisioned, APlayerController*);

/**
 * 플레이어에게 DA_DefaultProvision의 기본 지급 설정을 적용한다.
 *
 * 소유자가 정의 에셋을 로드하고 플레이 공간마다 지급 처리 객체 하나를 초기화한다.
 * 정의와 모드는 Shutdown까지 고정하며, 반복 초기화로 기존 지급 상태를 초기화하지 않는다.
 *
 * 지급은 능력치·아이템·판도라·제스처 네 단계다. 한 단계라도 끝나지 않으면 기다릴 비동기 준비의 완료 알림을 받아
 * 다시 시도하고, 이미 끝난 단계는 플레이어별 기록을 보고 건너뛴다.
 */
UCLASS(Transient)
class LABPROJECT_API UDefaultPlayerProvisioner : public UObject
{
	GENERATED_BODY()

public:
	FOnDefaultPlayerProvisioned OnPlayerProvisioned;

private:
	enum class EContentState : uint8 { NotStarted, Loading, Ready, Failed };

	// 지급 단계 하나의 결과. Waiting은 비동기 준비(아이템 로딩, 능력치 정의, Pawn)가 끝나면 다시 시도할 수 있고,
	// Failed는 구성 요소나 정의가 빠져 다시 시도해도 끝나지 않는다.
	enum class EProvisionStepResult : uint8 { Done, Waiting, Failed };

	// 인벤토리 지급을 마친 인벤토리. 같은 인벤토리면 다시 맞추지 않고, 인벤토리가 바뀌면 처음부터 다시 지급한다.
	struct FInventoryProvisionState
	{
		TWeakObjectPtr<UInventoryComponent> Inventory;
		bool bCompleted = false;
	};

	// 지급을 마치지 못한 플레이어가 기다리는 비동기 준비. 완료 알림이 오면 다음 틱에 다시 지급한다.
	struct FProvisionWait
	{
		TWeakObjectPtr<UInventoryComponent> Inventory;
		FDelegateHandle InventoryHandle;
		TWeakObjectPtr<UStatUpgradeComponent> StatUpgrade;
		FDelegateHandle StatUpgradeHandle;
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle PawnHandle;
	};

	// 판도라 지급을 마친 컴포넌트 쌍. 둘 다 같으면 다시 지급하지 않는다.
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
	void ClearRuntimeStateForController(AController* Controller, APlayerState* PlayerState);
	void Shutdown();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleContentLoaded();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	const UDefaultProvisionDefinition* GetDefinition() const;
	bool EnsureContentLoaded(APlayerController* PlayerController);
	EProvisionStepResult TryProvisionPlayer(APlayerController* PlayerController);
	EProvisionStepResult ApplyModeValues(APdPlayerState* PlayerState);
	EProvisionStepResult ApplyItems(APdPlayerState* PlayerState);
	EProvisionStepResult ApplyPandoras(APdPlayerState* PlayerState);
	EProvisionStepResult ApplyGestures(APlayerController* PlayerController, APdPlayerState* PlayerState);
	void WaitForProvisionInputs(APlayerController* PlayerController, EProvisionStepResult Result);
	void StopWaitingForProvisionInputs(TObjectKey<APlayerController> ControllerKey);
	void ScheduleProvisionAttempt(APlayerController* PlayerController);
	void ClearScheduledProvisionAttempt(APlayerController* PlayerController);

private:
	UPROPERTY(Transient)
	TObjectPtr<const UDefaultProvisionDefinition> ProvisionDefinition;

	// 플레이 공간의 모든 플레이어가 함께 기다리는 지급 콘텐츠(아이템·판도라·제스처 정의) 로딩
	TArray<FPrimaryAssetId> RequiredContentIds;
	TSharedPtr<FStreamableHandle> ContentLoadHandle;
	EContentState ContentState = EContentState::NotStarted;
	TArray<TWeakObjectPtr<APlayerController>> PendingContentControllers;

	// 다시 지급할 플레이어의 다음 틱 예약과 기다리는 완료 알림
	TMap<TObjectKey<APlayerController>, FTimerHandle> PendingProvisionAttempts;
	TMap<TObjectKey<APlayerController>, FProvisionWait> ProvisionWaits;

	// 단계별로 지급을 마친 플레이어. 로그아웃하면 지운다.
	TSet<TObjectKey<APlayerState>> InitializedModeValues;
	TMap<TObjectKey<APlayerState>, FInventoryProvisionState> InventoryStates;
	TMap<TObjectKey<APlayerState>, FInitializedPandoraState> InitializedPandoras;
	TMap<TObjectKey<APlayerState>, TWeakObjectPtr<USkinEquipmentComponent>> InitializedGestureEquipment;

	EDefaultProvisionMode Mode = EDefaultProvisionMode::Lobby;
	bool bShuttingDown = false;
};
