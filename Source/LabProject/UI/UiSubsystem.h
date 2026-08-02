#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UiSubsystem.generated.h"

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

DECLARE_LOG_CATEGORY_EXTERN(PdUiSubsystemLog, Log, All);

UENUM(BlueprintType)
enum class EPdUiInputMode : uint8
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
enum class EPdUiInputRestorePolicy : uint8
{
	PreviousState,
	Gameplay
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdUiModalInputConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Input")
	EPdUiInputMode InputMode = EPdUiInputMode::GameAndUI;

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
	EPdUiInputRestorePolicy RestorePolicy = EPdUiInputRestorePolicy::PreviousState;
};

UCLASS(Config = Game)
class LABPROJECT_API UUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool RefreshStatusViewModel();

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	bool ApplyStatusViewModelToWidgetTree(UUserWidget* RootWidget);

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const { return StatusViewModel; }

	//------------------------------------------------------------------------------------------------------------------
	//--- Definition
	/** Stores the active UI composition once per LocalPlayer. */
	void SetWidgetClassDefinition(UWidgetClassDefinition* InWidgetClassDefinition);
	void ClearWidgetClassDefinition(const UWidgetClassDefinition* ExpectedWidgetClassDefinition);
	UWidgetClassDefinition* GetWidgetClassDefinition() const { return WidgetClassDefinition; }
#if WITH_EDITOR
	static UWidgetClassDefinition* LoadConfiguredEditorWidgetClassDefinition();
#endif

	//------------------------------------------------------------------------------------------------------------------
	//--- Modal Input
	/**
	 * Adds a modal input owner to the stack and returns the token required to update or release it.
	 * The first modal snapshots the current input/focus state; only the top modal controls input.
	 */
	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	FGuid AcquireModalInput(
		UObject* Owner,
		UWidget* FocusWidget,
		const FPdUiModalInputConfig& InputConfig);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	bool UpdateModalInput(
		UObject* Owner,
		FGuid Token,
		UWidget* FocusWidget,
		const FPdUiModalInputConfig& InputConfig);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	bool ReleaseModalInput(UObject* Owner, FGuid Token);

	UFUNCTION(BlueprintCallable, Category = "!UI|Input")
	void ReleaseModalInputsForOwner(UObject* Owner);

	UFUNCTION(BlueprintPure, Category = "!UI|Input")
	bool IsModalInputActive(FGuid Token) const;

	/** Prunes stale world-scoped entries before gameplay decides its final input mode. */
	bool HasActiveModalInput();

	//------------------------------------------------------------------------------------------------------------------
	//--- Connecting Popup
	UFUNCTION(BlueprintCallable, Category = "!UI|Connecting")
	void SetConnectingPopupWidgetClass(TSubclassOf<UConnectingPopupWidget> InWidgetClass);

	UFUNCTION(BlueprintCallable, Category = "!UI|Connecting")
	UConnectingPopupWidget* ShowConnectingPopup(bool bEnableCancelButton = true);

	UFUNCTION(BlueprintCallable, Category = "!UI|Connecting")
	void HideConnectingPopup();

	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	UConnectingPopupWidget* ShowTravelLoadingScreen(bool bEnableCancelButton = false);

	/** Keeps the travel screen active while DA_Setting and DA_MatchRule are prepared for lobby entry. */
	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	UConnectingPopupWidget* ShowLobbyEntryLoadingScreen(bool bEnableCancelButton = false);

	UFUNCTION(BlueprintCallable, Category = "!UI|Loading")
	void HideTravelLoadingScreen();

	UFUNCTION(BlueprintPure, Category = "!UI|Loading")
	bool IsTravelLoadingScreenActive() const { return bTravelLoadingScreenActive; }

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;

	FName ResolveStatusViewModelSourceName(const UUserWidget* InWidget) const;

	APlayerController* GetLocalPlayerController() const;

	struct FModalInputEntry
	{
		FGuid Token;
		TWeakObjectPtr<UObject> Owner;
		TWeakObjectPtr<UWidget> FocusWidget;
		TWeakObjectPtr<UWorld> World;
		FPdUiModalInputConfig InputConfig;
		bool bTracksFocusWidgetLifetime = false;
	};

	struct FInputStateSnapshot
	{
		TWeakObjectPtr<APlayerController> PlayerController;
		TWeakObjectPtr<UWorld> World;
		TWeakPtr<SWidget> FocusedSlateWidget;
		EPdUiInputMode InputMode = EPdUiInputMode::GameOnly;
		EMouseCaptureMode MouseCaptureMode = EMouseCaptureMode::CapturePermanently;
		EMouseLockMode MouseLockMode = EMouseLockMode::LockOnCapture;
		bool bIgnoreViewportInput = false;
		bool bHideCursorDuringCapture = false;
		bool bShowMouseCursor = false;
		bool bEnableClickEvents = false;
		bool bEnableMouseOverEvents = false;
		bool bValid = false;

		void Reset()
		{
			*this = FInputStateSnapshot();
		}
	};

	bool CaptureInputState(APlayerController* PlayerController, FInputStateSnapshot& OutSnapshot) const;
	bool ApplyInputState(const FInputStateSnapshot& Snapshot) const;
	bool ApplyModalInput(
		APlayerController* PlayerController,
		UWidget* FocusWidget,
		const FPdUiModalInputConfig& InputConfig) const;
	bool ApplyGameplayInput(APlayerController* PlayerController) const;
	void ApplyTopModalInput();
	void RestoreInputStateAfterLastModal();
	void RefreshRestorePolicyFromBottomModal();
	bool IsModalInputEntryValid(const FModalInputEntry& Entry) const;
	void PruneInvalidModalInputs();
	bool ReleaseModalInputInternal(const UObject* Owner, const FGuid& Token, bool bRequireOwnerMatch);

	void BeginConfiguredWidgetDefinitionPreload();
	void HandleConfiguredWidgetDefinitionLoaded();
	void HandleConfiguredWidgetDependenciesLoaded();
	void ReleaseConfiguredWidgetDefinitionPreload();

	TSubclassOf<UConnectingPopupWidget> ResolveConnectingPopupWidgetClass();

	UFUNCTION()
	void HandleConnectingPopupCanceled();

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Definition
	UPROPERTY(Config, EditDefaultsOnly, Category = "!UI|Definition", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UWidgetClassDefinition> DefaultWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ConfiguredWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition;

	TSharedPtr<FStreamableHandle> ConfiguredDefinitionLoadHandle;
	TSharedPtr<FStreamableHandle> ConfiguredDefinitionDependenciesHandle;
	bool bHasExternalWidgetClassDefinition = false;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;

	//------------------------------------------------------------------------------------------------------------------
	//--- Modal Input
	TArray<FModalInputEntry> ModalInputStack;
	FInputStateSnapshot InputStateBeforeModals;
	EPdUiInputRestorePolicy RestorePolicyAfterModals = EPdUiInputRestorePolicy::PreviousState;
	bool bIsDeinitializing = false;

	//------------------------------------------------------------------------------------------------------------------
	//--- Connecting Popup
	UPROPERTY(Transient)
	TSubclassOf<UConnectingPopupWidget> ConnectingPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UConnectingPopupWidget> ActiveConnectingPopupWidget;

	FGuid ConnectingPopupModalToken;

	UPROPERTY(Transient)
	bool bTravelLoadingScreenActive = false;

	UPROPERTY(Transient)
	bool bTravelLoadingScreenCancelEnabled = false;
};
