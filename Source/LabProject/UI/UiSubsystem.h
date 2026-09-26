#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Definition/UI/WidgetContentBundle.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UiSubsystem.generated.h"

class UUiLayerRoot;
class UUiScreen;
enum class EUiScreenLayer : uint8 { Screen, Overlay, Menu, Modal };

class UCommonActivatableWidget;
class UAbilitySystemComponent;
class APlayerController;
class UConnectingPopupWidget;
struct FStreamableHandle;
class UStatusViewModel;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
class UWorld;
class FWidgetContentBundleLease;

DECLARE_LOG_CATEGORY_EXTERN(PdUiSubsystemLog, Log, All);

UCLASS(Config = Game)
class LABPROJECT_API UUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool RefreshStatusViewModel();

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget);

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const { return StatusViewModel; }

	/** Stores the active UI composition once per LocalPlayer. */
	void SetWidgetClassDefinition(UWidgetClassDefinition* InWidgetClassDefinition);
	void ClearWidgetClassDefinition(const UWidgetClassDefinition* ExpectedWidgetClassDefinition);
	UWidgetClassDefinition* GetWidgetClassDefinition() const { return WidgetClassDefinition; }
	void EnsureConfiguredWidgetContentPreload();
	bool IsConfiguredWidgetContentReady() const
	{
		return bConfiguredWidgetContentReady;
	}
	/** True after the always-needed UI and all skill definitions are resident. */
	bool IsStartupContentReady() const;

	/** Keeps one explicit-definition bundle resident for the lease lifetime. */
	TSharedPtr<FWidgetContentBundleLease> AcquireWidgetContentBundle(
		UWidgetClassDefinition* Definition,
		EWidgetContentBundle Bundle,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/** Queues the request while the configured DA_Widget root is still loading. */
	TSharedPtr<FWidgetContentBundleLease> AcquireConfiguredWidgetContentBundle(
		EWidgetContentBundle Bundle,
		FSimpleDelegate OnComplete = FSimpleDelegate());
#if WITH_EDITOR
	static UWidgetClassDefinition* LoadConfiguredEditorWidgetClassDefinition();
#endif

	void PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer = EUiScreenLayer::Menu);
	UFUNCTION(BlueprintCallable, Category = "!UI|Connecting")
	UConnectingPopupWidget* ShowConnectingPopup(bool bEnableCancelButton = true);

	UFUNCTION(BlueprintCallable, Category = "!UI|Connecting")
	void HideConnectingPopup();

	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	UConnectingPopupWidget* ShowTravelLoadingScreen(bool bEnableCancelButton = false);

	/** Keeps the travel screen active while lobby UI/data and skill definitions are prepared. */
	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	UConnectingPopupWidget* ShowLobbyEntryLoadingScreen(bool bEnableCancelButton = false);

	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	void HideTravelLoadingScreen();

	UFUNCTION(BlueprintPure, Category = "!UI|Loading")
	bool IsTravelLoadingScreenActive() const { return bTravelLoadingScreenActive; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleConfiguredWidgetDefinitionLoaded();
	void RefreshConfiguredWidgetContentState();
	bool TickStartupLoadingScreenReady(float DeltaTime);

	UFUNCTION()
	void HandleConnectingPopupCanceled();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	bool BindStatusViewModelToWidget(UUserWidget* InWidget);

	FName ResolveStatusViewModelSourceName(const UUserWidget* InWidget) const;

	APlayerController* GetLocalPlayerController() const;

	void BeginConfiguredWidgetDefinitionPreload();
	void ReleaseConfiguredWidgetDefinitionPreload();

	void BindPendingConfiguredWidgetContentBundleLeases();
	void FailPendingConfiguredWidgetContentBundleLeases();
	void StartWidgetContentBundleLease(
		const TSharedPtr<FWidgetContentBundleLease>& Lease,
		UWidgetClassDefinition* Definition);
	void BeginStartupLoadingScreen();
	void CancelStartupLoadingScreenReadyCheck();

	TSubclassOf<UConnectingPopupWidget> ResolveConnectingPopupWidgetClass();

private:
	UPROPERTY(Config, EditDefaultsOnly, Category = "!UI|Definition", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UWidgetClassDefinition> DefaultWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ConfiguredWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition;

	TSharedPtr<FStreamableHandle> ConfiguredDefinitionLoadHandle;
	TSharedPtr<FWidgetContentBundleLease> ConfiguredCoreBundleLease;
	TArray<TWeakPtr<FWidgetContentBundleLease>>
		PendingConfiguredWidgetContentBundleLeases;
	bool bHasExternalWidgetClassDefinition = false;
	bool bConfiguredWidgetContentPreloadPending = false;
	bool bConfiguredWidgetContentReady = false;

	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UUiLayerRoot> ScreenRoot;

	bool bIsDeinitializing = false;

	UPROPERTY(Transient)
	TSubclassOf<UConnectingPopupWidget> ConnectingPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UConnectingPopupWidget> ActiveConnectingPopupWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> ConnectingScreen;

	UPROPERTY(Transient)
	bool bTravelLoadingScreenActive = false;

	UPROPERTY(Transient)
	bool bTravelLoadingScreenCancelEnabled = false;

	bool bStartupLoadingScreenPending = false;
	FTSTicker::FDelegateHandle StartupLoadingScreenReadyTickerHandle;
};
