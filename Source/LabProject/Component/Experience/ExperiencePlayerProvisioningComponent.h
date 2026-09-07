#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "UObject/ObjectKey.h"
#include "ExperiencePlayerProvisioningComponent.generated.h"

class APlayerController;
class APawn;
class UDefaultPlayerProvisioner;
class UExperiencePlayerProfileService;
struct FStreamableHandle;

/**
 * 서버에서 저장 데이터 복원과 기본 지급의 순서를 연결한다.
 *
 * 프로필 복원과 기본 지급은 기존 전용 객체가 담당하며, 현재 Pawn의 지급 완료를 GameMode에 알린다.
 */
UCLASS(ClassGroup = (Experience))
class LABPROJECT_API UExperiencePlayerProvisioningComponent
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UExperiencePlayerProvisioningComponent();

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------

	FSimpleMulticastDelegate OnPlayerGameplayReady;
	bool IsPlayerReadyForGameplay(APlayerController* PlayerController) const;

	void ApplySettings(
		const FExperiencePlayerProvisioningSettings& InSettings);
	void InitializeLoggedInPlayer(APlayerController* NewPlayer);
	void InitializeMatchIdentity(APlayerController* NewPlayer);
	void PreparePlayerForGameplay(APlayerController* NewPlayer);
	void ClearRuntimeStateForController(
		AController* Controller,
		APlayerState* PlayerState);
	void ApplyConfiguredStatusPointsForPlayerState(
		APlayerState* PlayerState);
	bool IsTrainingRoomMap() const;

private:
	void BeginProvisioningContentPreload();
	void HandleProvisioningContentPreloaded();
	void ReleaseProvisioningContentPreload();
	void FlushPendingGameplayProvisions();
	void PreparePlayerForGameplayInternal(APlayerController* NewPlayer);
	void HandlePlayerProvisioned(APlayerController* PlayerController);

	UPROPERTY(Transient)
	TObjectPtr<UExperiencePlayerProfileService>
		PlayerProfileService;

	UPROPERTY(Transient)
	TObjectPtr<UDefaultPlayerProvisioner>
		DefaultPlayerProvisioner;

	FExperiencePlayerProvisioningSettings CachedSettings;
	TArray<TWeakObjectPtr<APlayerController>> PendingGameplayPlayers;
	TMap<TObjectKey<APlayerController>, TWeakObjectPtr<APawn>> ReadyGameplayPawns;
	TSharedPtr<FStreamableHandle> ProvisioningContentLoadHandle;
	bool bProvisioningContentLoadPending = false;
	bool bProvisioningContentReady = false;
};
