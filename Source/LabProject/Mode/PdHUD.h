#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "PdHUD.generated.h"

class APdPlayerController;
class UInfoUiPresenter;
class UInfoWidget;
class USelectPandoraWidget;
class UPandoraTreeWidget;
class UUiSubsystem;
class UUserWidget;
class UWidgetClassDefinition;

UCLASS()
class LABPROJECT_API APdHUD : public AHUD
{
	GENERATED_BODY()

public:
	APdHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void InitializeUi(UWidgetClassDefinition* InWidgetClassDefinition);
	void DeinitializeUi(const UWidgetClassDefinition* InWidgetClassDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void CreateAllUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void OpenInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void CloseInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void ToggleInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void OpenPandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ClosePandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void TogglePandoraTreeUi();

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void ToggleUiMode(bool bOn);

	UFUNCTION(BlueprintPure, Category = "!UI")
	bool IsGameplayInputBlockedByUi() const;

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void OpenSelectPandoraUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void CloseSelectPandoraUi();

	void UpdateSelectPandoraDirectionFromMouse();
	void ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag);
	void HideAimCrosshair();
	void RefreshUiBindings();

	UInfoWidget* GetInfoWidget() const { return CachedInfoUI; }
	USelectPandoraWidget* GetSelectPandoraWidget() const { return CachedSelectPandoraUI; }
	UUserWidget* GetPlayerHudWidget() const { return CachedPlayerHUD; }

	void OnOpenInfoUiInputStarted(const FInputActionValue& InputValue);
	void OnSelectPandoraInputStarted(const FInputActionValue& InputValue);
	void OnSelectPandoraInputEnded(const FInputActionValue& InputValue);
	void OnPandoraTreeInputStarted(const FInputActionValue& InputValue);

private:
	APdPlayerController* GetPdController() const;
	UInfoUiPresenter* GetInfoUiPresenter();
	UUiSubsystem* GetUiSubsystem() const;
	bool ApplyStatusViewModelToWidget(UUserWidget* InWidget);
	bool ApplyStatusViewModelToPlayerHud();
	void ApplyStatusViewModelToPlayerHudRecursive(UUserWidget* RootWidget, bool& bFoundPlayerVitals, bool& bAppliedViewModel);
	void RetryApplyStatusViewModelToPlayerHud();
	void RemoveAllUiWidgets();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	int32 CachedDirIndex = -1;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AimCrosshairWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CachedPlayerHUD = nullptr;

	FTimerHandle PlayerHudStatusViewModelRetryTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UInfoUiPresenter> CachedInfoUiPresenter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> CachedInfoUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USelectPandoraWidget> CachedSelectPandoraUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeWidget> CachedPandoraTreeUI = nullptr;
};
