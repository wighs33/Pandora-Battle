#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Definition/UI/WidgetContentBundle.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UiSubsystem.generated.h"

class UUiLayerRoot;
class UUiScreen;
enum class EUiScreenLayer : uint8 { Menu, Modal };

class UCommonActivatableWidget;
class UCommonActivatableWidgetStack;
class UAbilitySystemComponent;
class APlayerController;
class UConnectingPopupWidget;
struct FStreamableHandle;
class UStatusViewModel;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
class UWorld;
class SWidget;
class FWidgetContentBundleLease;

DECLARE_LOG_CATEGORY_EXTERN(PdUiSubsystemLog, Log, All);

UENUM(BlueprintType)
enum class EUiInputMode : uint8
{
	GameOnly,
	GameAndUI,
	UIOnly
};

/**
 * Selects the state to apply after the last modal owner leaves the stack.
 *
 * PreviousState is appropriate for temporary popups that can be shown over
 * either a menu or gameplay. Gameplay is deterministic and does not infer the
 * previous mode from viewport mouse-capture settings, which differ between PIE
 * and packaged builds.
 */
UENUM(BlueprintType)
enum class EUiInputRestorePolicy : uint8
{
	PreviousState,
	Gameplay
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FUiModalInputConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	EUiInputMode InputMode = EUiInputMode::GameAndUI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	EMouseLockMode MouseLockMode = EMouseLockMode::DoNotLock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bHideCursorDuringCapture = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bShowMouseCursor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bEnableClickEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bEnableMouseOverEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bFlushInput = false;

	/** Keeps the current Enhanced Input mode while still routing cursor state through the modal stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	bool bApplyInputMode = true;

	/** The bottom modal entry owns the state restored when the stack becomes empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	EUiInputRestorePolicy RestorePolicy = EUiInputRestorePolicy::PreviousState;
};

UCLASS(Config = Game)
class LABPROJECT_API UUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

private:
	struct FModalInputEntry
	{
		FGuid Token;
		TWeakObjectPtr<UObject> Owner;
		TWeakObjectPtr<UWidget> FocusWidget;
		TWeakObjectPtr<UWorld> World;
		FUiModalInputConfig InputConfig;
		bool bTracksFocusWidgetLifetime = false;
	};

	struct FInputStateSnapshot
	{

	public:
		TWeakObjectPtr<APlayerController> PlayerController;
		TWeakObjectPtr<UWorld> World;
		TWeakPtr<SWidget> FocusedSlateWidget;
		EUiInputMode InputMode = EUiInputMode::GameOnly;
		EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::CapturePermanently;
		EMouseLockMode MouseLockMode = EMouseLockMode::LockOnCapture;
		bool bHideCursorDuringCapture = false;
		bool bShowMouseCursor = false;
		bool bValid = false;

		// Public API --------------------------------------------------------------------------------------------------
		void Reset()
		{
			*this = FInputStateSnapshot();
		}
	};

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

	/**
	 * Adds a modal input owner to the stack and returns the token required to update or release it.
	 * The first modal snapshots the current input/focus state; only the top modal controls input.
	 */
	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	FGuid AcquireModalInput(
		UObject* Owner,
		UWidget* FocusWidget,
		const FUiModalInputConfig& InputConfig);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	bool UpdateModalInput(
		UObject* Owner,
		FGuid Token,
		UWidget* FocusWidget,
		const FUiModalInputConfig& InputConfig);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	bool ReleaseModalInput(UObject* Owner, FGuid Token);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	void ReleaseModalInputsForOwner(UObject* Owner);

	void PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer = EUiScreenLayer::Menu);
	static void SetBaseInputMode(APlayerController* Controller, EUiInputMode Mode, UWidget* FocusWidget = nullptr);

	/** Prunes stale world-scoped entries before gameplay decides its final input mode. */
	bool HasActiveModalInput();

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

	bool CaptureInputState(APlayerController* PlayerController, FInputStateSnapshot& OutSnapshot) const;
	bool ApplyInputState(const FInputStateSnapshot& Snapshot) const;
	bool ApplyModalInput(
		APlayerController* PlayerController,
		UWidget* FocusWidget,
		const FUiModalInputConfig& InputConfig) const;
	bool ApplyGameplayInput(APlayerController* PlayerController) const;
	void ApplyTopModalInput();
	void RestoreInputStateAfterLastModal();
	void RefreshRestorePolicyFromBottomModal();
	bool IsModalInputEntryValid(const FModalInputEntry& Entry) const;
	void PruneInvalidModalInputs();
	bool ReleaseModalInputInternal(const UObject* Owner, const FGuid& Token, bool bRequireOwnerMatch);

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

	TArray<FModalInputEntry> ModalInputStack;
	FInputStateSnapshot BaseInputState;
	FInputStateSnapshot InputStateBeforeModals;
	EUiInputRestorePolicy RestorePolicyAfterModals = EUiInputRestorePolicy::PreviousState;
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
