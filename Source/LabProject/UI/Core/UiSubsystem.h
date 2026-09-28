#pragma once

#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "CoreMinimal.h"
#include "Definition/UI/UiContentGroup.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UiSubsystem.generated.h"

struct FWorldContext;
class UNetDriver;
class UUiLayerRoot;
class UUiScreen;
enum class EUiScreenLayer : uint8 { Screen, Overlay, Menu, Modal };

class UCommonActivatableWidget;
class UAbilitySystemComponent;
class APlayerController;
class UConnectingPopupWidget;
class UGameSettingsWidget;
struct FStreamableHandle;
class UStatusViewModel;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
class UWorld;
class FContentLease;

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

	/** Keeps one explicit-definition UI group resident for the lease lifetime. */
	TSharedPtr<FContentLease> AcquireUiContent(
		UWidgetClassDefinition* Definition,
		EUiContentGroup Group,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/** Queues the request while the configured DA_Widget root is still loading. */
	TSharedPtr<FContentLease> AcquireConfiguredUiContent(
		EUiContentGroup Group,
		FSimpleDelegate OnComplete = FSimpleDelegate());
#if WITH_EDITOR
	static UWidgetClassDefinition* LoadConfiguredEditorWidgetClassDefinition();
#endif

	void PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer = EUiScreenLayer::Menu);
	void OpenGameSettings(UUserWidget* OwnerMenu);
	bool CloseGameSettings(const UUserWidget* ExpectedOwner = nullptr);
	bool HasBlockingWait() const { return !ActiveWaitReasons.IsEmpty(); }
	/** Server travel preparation, paired by LobbyTravelCoordinator's existing client RPCs. */
	void SetGameStartPreparationPending(bool bPending);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleConfiguredWidgetDefinitionLoaded();
	void RefreshConfiguredWidgetContentState();
	/** Rebuilds only the view of existing work. Never begins a wait. */
	void RefreshLoadingScreen();
	bool TickLoadingWork(float DeltaTime);
	void HandlePreClientTravel(const FString& URL, ETravelType TravelType, bool bSeamless);
	void HandlePreLoadMap(const FWorldContext& Context, const FString& MapName);
	void HandleSeamlessTravelStart(UWorld* World, const FString& URL);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type Type, const FString& Error);
	void BeginTravel(UWorld* SourceWorld, const FString& URL);
	void AbortTravel();
	bool IsDestinationPresentationReady() const;
	UConnectingPopupWidget* ShowConnectingPopup(bool bEnableCancelButton);
	void HideConnectingPopup();

	UFUNCTION()
	void HandleConnectingPopupCanceled();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	bool BindStatusViewModelToWidget(UUserWidget* InWidget);

	FName ResolveStatusViewModelSourceName(const UUserWidget* InWidget) const;

	APlayerController* GetLocalPlayerController() const;

	void BeginConfiguredWidgetDefinitionPreload();
	void ReleaseConfiguredWidgetDefinitionPreload();

	void BindPendingConfiguredUiContent();
	void FailPendingConfiguredUiContent();

	TSubclassOf<UConnectingPopupWidget> ResolveConnectingPopupWidgetClass();

private:
	UPROPERTY(Config, EditDefaultsOnly, Category = "!UI|Definition", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UWidgetClassDefinition> DefaultWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ConfiguredWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition;

	TSharedPtr<FStreamableHandle> ConfiguredDefinitionLoadHandle;
	TSharedPtr<FContentLease> ConfiguredCoreContentLease;
	TArray<TPair<EUiContentGroup, TWeakPtr<FContentLease>>> PendingConfiguredUiContent;
	bool bHasExternalWidgetClassDefinition = false;
	bool bConfiguredWidgetContentPreloadPending = false;
	bool bConfiguredWidgetContentReady = false;

	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UUiLayerRoot> ScreenRoot;

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingsWidget> ActiveGameSettings;

	TWeakObjectPtr<UUserWidget> SettingsOwner;

	bool bIsDeinitializing = false;

	UPROPERTY(Transient)
	TSubclassOf<UConnectingPopupWidget> ConnectingPopupWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UConnectingPopupWidget> ActiveConnectingPopupWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> ConnectingScreen;

	// A snapshot of work owned by content/session systems, plus the cross-world travel transaction.
	enum class EWaitReason : uint8
	{
		StartupContent, LobbyEntryContent, GameEntryContent, SessionRequest,
		SessionLifecycle, GameStartPreparation, Travel, PipelineCompile
	};
	TSet<EWaitReason> ActiveWaitReasons;
	bool bTravelPending = false;
	bool bGameStartPreparationPending = false;
	TWeakObjectPtr<UWorld> TravelSourceWorld;
	FString TravelDestinationMap;
	uint64 CancelableSessionRequestId = 0;
	FTSTicker::FDelegateHandle LoadingWorkTickerHandle;
	FDelegateHandle PreClientTravelHandle;
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
