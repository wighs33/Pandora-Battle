#pragma once

#include "CoreMinimal.h"
#include "Definition/UI/UiContentGroup.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Templates/SubclassOf.h"
#include "UiSubsystem.generated.h"

class UUiLayerRoot;
class UUiScreen;
enum class EUiScreenLayer : uint8 { Screen, Overlay, Menu, Modal };

class UCommonActivatableWidget;
class UAbilitySystemComponent;
class APlayerController;
class UGameSettingDefinition;
class UGameSettingsWidget;
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

	/** 로컬 플레이어마다 현재 UI 구성을 한 번 저장한다. */
	void SetWidgetClassDefinition(UWidgetClassDefinition* InWidgetClassDefinition);
	void ClearWidgetClassDefinition(const UWidgetClassDefinition* ExpectedWidgetClassDefinition);
	UWidgetClassDefinition* GetWidgetClassDefinition() const { return WidgetClassDefinition; }
	void BeginConfiguredWidgetDefinitionPreload();
	bool IsConfiguredWidgetContentReady() const
	{
		return bConfiguredWidgetContentReady;
	}
	bool IsConfiguredWidgetContentPreloadPending() const { return bConfiguredWidgetContentPreloadPending; }
	/** 로비 진입 때 붙잡은 로비 화면 콘텐츠를 아직 불러오는 중인지. */
	bool IsLobbyContentLoading() const;

	/** 명시한 정의의 UI 묶음 하나를 lease가 살아 있는 동안 메모리에 둔다. */
	TSharedPtr<FContentLease> AcquireUiContent(
		const UWidgetClassDefinition* Definition,
		EUiContentGroup Group,
		FSimpleDelegate OnComplete = FSimpleDelegate());

	/** 설정된 DA_Widget 루트가 아직 로딩 중이면 요청을 대기열에 넣는다. */
	TSharedPtr<FContentLease> AcquireConfiguredUiContent(
		EUiContentGroup Group,
		FSimpleDelegate OnComplete = FSimpleDelegate());
#if WITH_EDITOR
	static UWidgetClassDefinition* LoadConfiguredEditorWidgetClassDefinition();
#endif

	void PushScreen(UCommonActivatableWidget* Screen, EUiScreenLayer Layer = EUiScreenLayer::Menu);
	void OpenGameSettings(UUserWidget* OwnerMenu);
	bool CloseGameSettings(const UUserWidget* ExpectedOwner = nullptr);
	/** 이 월드의 기본 화면 층에 실제 CommonUI 화면이 이미 떠 있는지. */
	bool HasActiveScreen(const UWorld* World) const;

	/** 위젯 정의나 그 핵심 콘텐츠가 바뀌면 알린다. 로딩 대기는 이 알림으로 다시 판단한다. */
	FSimpleMulticastDelegate OnWidgetContentChanged;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleConfiguredWidgetDefinitionLoaded();
	void HandleCustomMouseCursorSettingsReady(APlayerController* PlayerController, const UGameSettingDefinition& SettingDefinition);
	void HandleLobbyEntryContentPreloadRequested();
	void HandleLobbyEntryContentReleased();
	void RefreshConfiguredWidgetContentState();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	bool BindStatusViewModelToWidget(UUserWidget* InWidget);

	APlayerController* GetLocalPlayerController() const;

	void ReleaseConfiguredWidgetDefinitionPreload();

	void BindPendingConfiguredUiContent();
	void FailPendingConfiguredUiContent();

private:
	UPROPERTY(Config, EditDefaultsOnly, Category = "!UI|Definition", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UWidgetClassDefinition> DefaultWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ConfiguredWidgetClassDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition;

	TSharedPtr<FContentLease> ConfiguredDefinitionLease;
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
	FDelegateHandle CustomMouseCursorSettingsHandle;
	FDelegateHandle LobbyEntryPreloadRequestedHandle;
	FDelegateHandle LobbyEntryReleasedHandle;
	TSharedPtr<FContentLease> LobbyContentLease;

	bool bIsDeinitializing = false;
};
