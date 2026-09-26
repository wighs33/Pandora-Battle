#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "MatchPlayerSetupComponent.generated.h"

class APlayerController;
class APawn;
class UDefaultPlayerProvisioner;
struct FStreamableHandle;

/**
 * 서버에서 저장 데이터 복원과 기본 지급의 순서를 연결한다.
 *
 * 프로필·로비 외형을 직접 복원하고 기본 지급은 Provisioner에 맡긴 뒤 현재 Pawn의 준비 완료를 알린다.
 */
UCLASS(ClassGroup = (Match))
class LABPROJECT_API UMatchPlayerSetupComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UMatchPlayerSetupComponent();
	bool IsPlayerReadyForGameplay(APlayerController* PlayerController) const;

	void InitializeRuntime();
	void InitializeLoggedInPlayer(APlayerController* NewPlayer);
	void InitializeMatchIdentity(APlayerController* NewPlayer);
	void PreparePlayerForGameplay(APlayerController* NewPlayer);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	bool IsTrainingRoomMap() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleSkinContentPreloaded();
	void HandlePlayerProvisioned(APlayerController* PlayerController);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BeginSkinContentPreload();
	void ReleaseSkinContentPreload();
	void FlushPendingGameplayProvisions();
	void PreparePlayerForGameplayInternal(APlayerController* NewPlayer);
	void ApplyCachedLobbySkinEquipment(APlayerController* NewPlayer) const;

public:
	FSimpleMulticastDelegate OnPlayerGameplayReady;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner>
		DefaultPlayerProvisioner;

	TArray<TWeakObjectPtr<APlayerController>> PendingGameplayPlayers;
	TMap<TObjectKey<APlayerController>, TWeakObjectPtr<APawn>> ReadyGameplayPawns;
	TSharedPtr<FStreamableHandle> SkinContentLoadHandle;
	bool bSkinContentLoadPending = false;
	bool bSkinContentReady = false;
};
