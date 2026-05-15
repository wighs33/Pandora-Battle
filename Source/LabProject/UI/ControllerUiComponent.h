#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "ControllerUiComponent.generated.h"

class APdPlayerController;
class UInfoWidget;
class UInfoUiPresenter;
class USelectPandoraWidget;
class UUiSubsystem;
class UUserWidget;
class UWidgetClassDefinition;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UControllerUiComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerUiComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "!UI")
	void CreateAllUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void OpenInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void CloseInfoUi();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void ToggleInfoUi();

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

	void OnOpenInfoUiInputStarted(const FInputActionValue& InputValue);
	void OnSelectPandoraInputStarted(const FInputActionValue& InputValue);
	void OnSelectPandoraInputEnded(const FInputActionValue& InputValue);

private:
	APdPlayerController* GetPdController() const;
	UInfoUiPresenter* GetInfoUiPresenter();
	UUiSubsystem* GetUiSubsystem() const;
	const UWidgetClassDefinition* GetWidgetClassDefinition() const;
	void ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	//------------------------------------------------------------------------------------------------------------------
	//--- UI Assets
	UPROPERTY(EditDefaultsOnly, Category = "!UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	int32 CachedDirIndex = -1;

	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime Widgets
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AimCrosshairWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoUiPresenter> CachedInfoUiPresenter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> CachedInfoUI = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USelectPandoraWidget> CachedSelectPandoraUI = nullptr;
};
