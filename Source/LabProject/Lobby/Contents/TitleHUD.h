#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TitleHUD.generated.h"

class UTitleWidget;
class UGameResultWidget;

class UUiScreen;
class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class UPointLightComponent;
class UMaterialInterface;
class UTextureRenderTarget2D;

UCLASS()
class LABPROJECT_API ATitleHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATitleHUD(const FObjectInitializer& ObjectInitializer);

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ShowPendingGameResult();

private:
	void InitializeTitleCharacter();
	void ScheduleTitleBlink();
	void BeginTitleBlink();
	void ShowTitleSpeech();
	void HideTitleSpeech();
	FVector2D GetTitleCharacterHeadTopUV() const;
	FTimerHandle TitleBlinkTimer;
	float TitleBlinkElapsed = -1.f;
	FTimerHandle TitleSpeechTimer;
	float TitleMouthElapsed = -1.f;
	bool bTitleMouthAvailable = false;
	/** INDEX_NONE until the greeting is shown, then the last tip number (0 = greeting). */
	int32 LastTitleTip = INDEX_NONE;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.1", Units = "s"))
	float BlinkIntervalMin = 2.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.1", Units = "s"))
	float BlinkIntervalMax = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.05", Units = "s"))
	float BlinkDuration = 0.2f;

	/** Luna's guide bubble: how long each line stays visible, and the gap before the next one. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.5", Units = "s"))
	float SpeechDisplayDuration = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.1", Units = "s"))
	float SpeechHiddenInterval = 3.f;
	/** Length of one 0 -> 1 -> 0 mouth movement while the bubble is visible. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.05", Units = "s"))
	float MouthCycleDuration = 0.3f;

	/** Presentation only. Adjust the mesh and its relative transform in BP_TitleHUD. */
	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<USkeletalMeshComponent> TitleCharacterMesh;

	/** Relative location, rotation and FOV define the portrait framing. */
	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<USceneCaptureComponent2D> TitleCharacterCapture;

	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<UPointLightComponent> TitleCharacterKeyLight;

	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<UPointLightComponent> TitleCharacterFillLight;

	/** UI composite material; the mesh's own materials remain unchanged. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character")
	TObjectPtr<UMaterialInterface> TitleCharacterDisplayMaterial;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> TitleCharacterRenderTarget;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> Screen;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Title|UI")
	TSubclassOf<UTitleWidget> TitleWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Title|UI")
	TSubclassOf<UGameResultWidget> GameResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTitleWidget> TitleWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameResultWidget> GameResultWidget;
};
