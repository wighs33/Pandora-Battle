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
	void SetTitleMorph(FName MorphName, float Weight);
	void BeginTitleBlink();
	void ShowTitleSpeech();
	void HideTitleSpeech();
	FVector2D GetTitleCharacterHeadTopUV() const;

	// Luna 대화: 플레이어의 질문은 로컬 LLM으로 가고, 스트리밍된 답이 돌아가며 보여 주던 팁을 대신한다.
	void BindLunaChat();
	void UnbindLunaChat();
	void HandleLunaQuestion(const FString& Question);
	void HandleLunaReplyUpdated(const FString& ReplySoFar);
	void HandleLunaReplyFinished(bool bSucceeded, const FString& Reply);
	void ShowLunaChatText(const FText& Text, bool bSpeaking);
	FText GetLunaChatLine(FName Key, const FText& Fallback) const;
	/** true인 동안 팁 순환을 멈춘다. 다음 팁은 SpeechHiddenInterval 대신 ChatTipResumeDelay만큼 기다린다. */
	bool bLunaChatActive = false;
	FDelegateHandle LunaReplyUpdatedHandle;
	FDelegateHandle LunaReplyFinishedHandle;

	// 보스 레이드: Luna가 진행 상황을 알리는 동안 백엔드가 플레이어를 GameLift 보스 레이드 세션에 넣는다.
	void BindBossRaid();
	void UnbindBossRaid();
	void HandleBossRaidRequested();
	UFUNCTION()
	void HandleBossRaidJoinFinished(bool bSucceeded, const FString& ErrorMessage);
	/** 백엔드는 PvP 참가(pd.Backend.JoinMatch)도 알리므로, 여기서 보낸 요청에만 답한다. */
	bool bBossRaidJoinPending = false;
	FTimerHandle TitleBlinkTimer;
	float TitleBlinkElapsed = -1.f;
	FTimerHandle TitleSpeechTimer;
	float TitleMouthElapsed = -1.f;
	bool bTitleMouthAvailable = false;
	/** 인사말을 보여 주기 전에는 INDEX_NONE이고, 그 뒤에는 마지막 팁 번호다(0 = 인사말). */
	int32 LastTitleTip = INDEX_NONE;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.1", Units = "s"))
	float BlinkIntervalMin = 2.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.1", Units = "s"))
	float BlinkIntervalMax = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Blink", meta = (ClampMin = "0.05", Units = "s"))
	float BlinkDuration = 0.2f;

	/** Luna 안내 말풍선: 줄마다 보이는 시간과 다음 줄까지의 간격. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.5", Units = "s"))
	float SpeechDisplayDuration = 6.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.1", Units = "s"))
	float SpeechHiddenInterval = 3.f;
	/** 말풍선이 보이는 동안 입이 0 -> 1 -> 0으로 한 번 움직이는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Speech", meta = (ClampMin = "0.05", Units = "s"))
	float MouthCycleDuration = 0.3f;

	/** Luna의 답은 최소 이 시간만큼, 긴 답이면 더 오래(글자당 읽는 시간) 보인다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Chat", meta = (ClampMin = "0.5", Units = "s"))
	float ChatReplyMinDuration = 5.f;
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Chat", meta = (ClampMin = "0.0", Units = "s"))
	float ChatReplySecondsPerCharacter = 0.08f;
	/** 대화가 끝나면 팁이 더 오래 기다려, 플레이어가 끊기지 않고 다음 질문을 할 수 있게 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "!Title|Character|Chat", meta = (ClampMin = "0.1", Units = "s"))
	float ChatTipResumeDelay = 15.f;

	/** 표시 전용이다. 메시와 상대 변환은 BP_TitleHUD에서 조정한다. */
	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<USkeletalMeshComponent> TitleCharacterMesh;

	/** 상대 위치·회전과 FOV로 초상화 구도를 정한다. */
	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<USceneCaptureComponent2D> TitleCharacterCapture;

	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<UPointLightComponent> TitleCharacterKeyLight;

	UPROPERTY(VisibleAnywhere, Category = "!Title|Character")
	TObjectPtr<UPointLightComponent> TitleCharacterFillLight;

	/** UI 합성용 머티리얼이다. 메시 자체의 머티리얼은 바꾸지 않는다. */
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
