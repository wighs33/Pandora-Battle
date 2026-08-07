#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"

#include "HudUiRouter.generated.h"

class APdHUD;
class UHudMenuLayer;
class UHudScoreboardLayer;
class UHudScreenLayer;
class UMenuPopupWidget;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
enum class EInfoUiSection : uint8;

/**
 * Per-HUD UI composition router.
 *
 * GameFeature definitions enter through one request stack, core widget layers
 * are created here, and one modal token is used to route HUD input through the
 * LocalPlayer UI subsystem.
 */
UCLASS()
class LABPROJECT_API UHudUiRouter : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud);
	void Shutdown();

	bool AddDefinitionRequest(UWidgetClassDefinition* Definition);
	bool RemoveDefinitionRequest(const UWidgetClassDefinition* Definition);
	UWidgetClassDefinition* GetActiveDefinition() const { return ActiveDefinition; }

	void EnsureCoreLayers();
	void EnsureInfoLayers();
	void ReleaseInfoLayers();
	void ResetLayers();

	void RouteInput(UWidget* FocusWidget, bool bPreserveGameplayInputMode, bool bCenterCursor);
	void ReleaseInput();

	bool OpenSettingsMenu();
	bool ToggleSettingsMenu();
	bool CloseSettingsMenu();
	bool IsSettingsMenuOpen() const;
	UMenuPopupWidget* GetSettingsMenuWidget() const;

	void ShowScoreboard();
	void HideScoreboard();
	void RefreshScoreboard();
	bool IsScoreboardOpen() const;

	void ShowAimCrosshair(FGameplayTag DesiredCrosshairWidgetTag);
	void HideAimCrosshair();

	void OpenInfo();
	void OpenInfo(EInfoUiSection InitialSection);
	void CloseInfo(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void ToggleInfo();
	void OpenPandoraTree();
	void ClosePandoraTree(bool bSuppressCameraReturn = false, bool bImmediate = false);
	void TogglePandoraTree();
	bool IsInfoClosing() const;
	bool IsPandoraTreeClosing() const;
	bool IsScreenLayerBlockingGameplayInput() const;
	void RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget = nullptr);
	void ScheduleTrainingRoomPause(float DelaySeconds);

private:
	friend class UHudScreenLayer;

	void ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition);
	class UUiSubsystem* ResolveUiSubsystem() const;
	class APdPlayerController* ResolvePlayerController() const;

	TWeakObjectPtr<APdHUD> OwnerHud;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetClassDefinition>> DefinitionRequests;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> ActiveDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UHudMenuLayer> MenuLayer;

	UPROPERTY(Transient)
	TObjectPtr<UHudScreenLayer> ScreenLayer;

	UPROPERTY(Transient)
	TObjectPtr<UHudScoreboardLayer> ScoreboardLayer;

	FGuid ModalInputToken;
	bool bEnsuringCoreLayers = false;
	bool bEnsuringInfoLayers = false;
};
