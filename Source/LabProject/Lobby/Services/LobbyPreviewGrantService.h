#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "LobbyPreviewGrantService.generated.h"

class AController;
class ALobbyGameMode;
class APlayerController;
class APdPlayerState;
class UInventoryComponent;
class ULobbyPreviewDefinition;
class UPandoraDefinition;
struct FLobbyPreviewGestureSlotGrant;

UCLASS(Transient)
class LABPROJECT_API ULobbyPreviewGrantService : public UObject
{
	GENERATED_BODY()

public:
	void ScheduleGrant(APlayerController* PlayerController, int32 RemainingAttempts = INDEX_NONE);
	void HandlePlayerLogout(AController* ExitingController);
	void Shutdown();

private:
	ALobbyGameMode* GetLobbyGameMode() const;
	const ULobbyPreviewDefinition* GetPreviewDefinition() const;
	bool TryGrant(APlayerController* PlayerController);
	void GrantItemStacks(
		UInventoryComponent* InventoryComponent,
		const ULobbyPreviewDefinition& PreviewDefinition);
	void GrantWeapons(UInventoryComponent* InventoryComponent);
	void GrantSavedSkins(APlayerController* PlayerController, APdPlayerState* PdPlayerState);
	void GrantGestures(
		APlayerController* PlayerController,
		APdPlayerState* PdPlayerState,
		const ULobbyPreviewDefinition& PreviewDefinition);
	void ApplyLoadedGestures(
		APlayerController* PlayerController,
		APdPlayerState* PdPlayerState,
		const TArray<FLobbyPreviewGestureSlotGrant>& GestureSlotGrants);
	void GrantPandoras(APdPlayerState* PdPlayerState);
	bool IsPandoraAllowed(const UPandoraDefinition* PandoraDefinition) const;
	void CleanupLoadHandles();
	void CancelContentLoads();
	void ClearRetryTimer(APlayerController* PlayerController);

	TSet<FObjectKey> RequestedPlayerStates;
	TSet<FObjectKey> PendingGestureGrantPlayerStates;
	TArray<TSharedPtr<FStreamableHandle>> PendingLoadHandles;
	TMap<TObjectKey<APlayerController>, FTimerHandle> PendingRetryTimers;
};
