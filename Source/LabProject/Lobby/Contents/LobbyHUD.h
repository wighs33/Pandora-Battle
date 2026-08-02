#pragma once

#include "CoreMinimal.h"
#include "Mode/PdHUD.h"
#include "LobbyHUD.generated.h"

class ULobbyWidget;

UCLASS()
class LABPROJECT_API ALobbyHUD : public APdHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool HandleEscapeInput() override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	ULobbyWidget* CreateLobbyUI();

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void RefreshLobbyUI();

	UFUNCTION()
	void NotifyLobbyWidgetOpened();

	UFUNCTION()
	void NotifyLobbyWidgetClosed();

	UFUNCTION(BlueprintPure, Category = "!Lobby|UI")
	ULobbyWidget* GetLobbyWidget() const { return LobbyWidget; }

protected:
	virtual bool IsPlayerHudSuppressedByUi() const override;
	void ApplyLobbyWidgetInputMode();
	void RestoreGameInputModeIfPossible();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby|UI")
	TSubclassOf<ULobbyWidget> LobbyWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Lobby|UI")
	TObjectPtr<ULobbyWidget> LobbyWidget;
};
