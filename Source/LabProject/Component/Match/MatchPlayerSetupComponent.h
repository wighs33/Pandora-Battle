#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "MatchPlayerSetupComponent.generated.h"

class AController;
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
	bool IsPlayerReadyForGameplay(AController* Controller) const;

	void InitializeRuntime();
	void InitializeLoggedInPlayer(APlayerController* NewPlayer);
	/** 사람과 대기실에서 넣은 봇 모두 받는다(봇은 AI 조종기가 PlayerState를 가진다). */
	void InitializeMatchIdentity(AController* NewPlayer);
	void PreparePlayerForGameplay(AController* NewPlayer);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	bool IsTrainingRoomMap() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleSkinContentPreloaded();
	void HandlePlayerProvisioned(AController* Controller);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BeginSkinContentPreload();
	void ReleaseSkinContentPreload();
	void FlushPendingGameplayProvisions();
	void PreparePlayerForGameplayInternal(AController* NewPlayer);
	void ApplyLobbySkinEquipment(AController* NewPlayer) const;

public:
	FSimpleMulticastDelegate OnPlayerGameplayReady;

private:
	// 블루프린트 컴포넌트 인스턴스가 템플릿의 변경 가능한 지급 상태를 공유하면 안 된다.
	UPROPERTY(Transient, Instanced)
	TObjectPtr<UDefaultPlayerProvisioner>
		DefaultPlayerProvisioner;

	TArray<TWeakObjectPtr<AController>> PendingGameplayPlayers;
	TMap<TObjectKey<AController>, TWeakObjectPtr<APawn>> ReadyGameplayPawns;
	TSharedPtr<FStreamableHandle> SkinContentLoadHandle;
	bool bSkinContentLoadPending = false;
	bool bSkinContentReady = false;
};
