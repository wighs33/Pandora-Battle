#pragma once

#include "CoreMinimal.h"
#include "Mode/PdHUD.h"
#include "LobbyHUD.generated.h"

class ULobbyWidget;
class AGameStateBase;
class ALobbyGameState;

/**
 * 로비 상태 변경을 구독해 참가자 목록과 시작 안내를 표시하고 로컬 UI 입력을 관리한다.
 */
UCLASS()
class LABPROJECT_API ALobbyHUD : public APdHUD
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	ULobbyWidget* CreateLobbyUI();

	UFUNCTION()
	void NotifyLobbyWidgetOpened();

	UFUNCTION()
	void NotifyLobbyWidgetClosed();

	UFUNCTION(BlueprintPure, Category = "!Lobby|UI")
	ULobbyWidget* GetLobbyWidget() const { return LobbyWidget; }

	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual bool HandleEscapeInput() override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void RefreshLobbyUI();

private:
	void HandleGameStateSet(AGameStateBase* GameState);
	void RequestLobbyUIRefresh();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual bool IsPlayerHudSuppressedByUi() const override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|UI")
	TSubclassOf<ULobbyWidget> LobbyWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Lobby|UI")
	TObjectPtr<ULobbyWidget> LobbyWidget;

private:
	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> LobbyScreen;

	TWeakObjectPtr<ALobbyGameState> ObservedLobbyGameState;
	FTimerHandle LobbyUIRefreshTimerHandle;
};
