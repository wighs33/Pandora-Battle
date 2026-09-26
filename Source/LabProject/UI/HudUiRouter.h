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
 * are created here. Input and focus belong to the active CommonUI screen.
 */
UCLASS()
class LABPROJECT_API UHudUiRouter : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(APdHUD* InOwnerHud);
	void Shutdown();

	bool AddDefinitionRequest(UWidgetClassDefinition* Definition);
	bool RemoveDefinitionRequest(const UWidgetClassDefinition* Definition);
	UWidgetClassDefinition* GetActiveDefinition() const { return ActiveDefinition; }

	void EnsureCoreLayers();
	void EnsureInfoLayers();
	void ReleaseInfoLayers();
	void ResetLayers();

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
	bool IsInfoOpen() const;
	bool IsInfoClosing() const;
	bool IsPandoraTreeClosing() const;
	bool ShouldScreenLayerSuppressPlayerHud() const;
	void RefreshTrainingRoomPause(const UUserWidget* IgnoredWidget = nullptr);
	void ScheduleTrainingRoomPause(float DelaySeconds);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition);
	class UUiSubsystem* ResolveUiSubsystem() const;
	class APdPlayerController* ResolvePlayerController() const;

private:
	friend class UHudScreenLayer;

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

	bool bEnsuringCoreLayers = false;
	bool bEnsuringInfoLayers = false;
};
