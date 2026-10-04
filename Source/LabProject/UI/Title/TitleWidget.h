#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "Types/SlateEnums.h"
#include "TitleWidget.generated.h"

class UButton;
class UAudioVolumeSlider;
struct FPdButtonClickBinding;
class UAudioVolumeControl;
class UEditableTextBox;
class UGuideWidget;
class UShopWidget;
class UUiSubsystem;
class UImage;
class UMaterialInterface;
class UTextBlock;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaQuestionSubmitted, const FString& /*Question*/);
DECLARE_MULTICAST_DELEGATE(FOnBossRaidRequested);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UTitleWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void SetTitleCharacterMaterial(UMaterialInterface* Material);

	/** Luna의 번역된 대사를 머리 위에 보여 준다. HeadTopUV는 Img_TitleCharacter 텍스처 공간의 점이다. */
	bool ShowLunaSpeech(FName TextKey, const FVector2D& HeadTopUV);
	/** 메뉴 문구가 아닌 텍스트(Luna가 만든 답)를 보여 준다. 언어가 바뀌어도 그대로 둔다. */
	bool ShowLunaSpeechText(const FText& Text, const FVector2D& HeadTopUV);
	void HideLunaSpeech();

	/** 플레이어가 Luna 아래 질문 칸에서 Enter를 눌렀다. */
	FOnLunaQuestionSubmitted& OnLunaQuestionSubmitted() { return LunaQuestionSubmitted; }

	/** 플레이어가 보스 레이드를 눌렀다. 타이틀 HUD가 백엔드에 레이드 자리(보스 레이드 세션)를 요청한다. */
	FOnBossRaidRequested& OnBossRaidRequested() { return BossRaidRequested; }
	/** 백엔드가 자리를 잡는 동안 꺼서 요청이 두 번 가지 않게 한다. */
	void SetBossRaidEnabled(bool bEnabled) const;

protected:
	virtual void OnMenuLanguageChanged() override;

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleRoomListClicked();

	UFUNCTION()
	void HandleQuickMatchClicked();

	UFUNCTION()
	void HandleTrainingModeClicked();

	UFUNCTION()
	void HandlePandoraShopClicked();

	UFUNCTION()
	void HandleGuideClicked();

	UFUNCTION()
	void HandleRecordClicked();

	UFUNCTION()
	void HandleExitClicked();

	UFUNCTION()
	void HandleBossRaidClicked();

	UFUNCTION()
	void HandleWebsiteClicked();

private:
	void HandleQuickMatchRequestComplete(uint64 RequestId, bool bWasSuccessful, bool bCreatedRoom);

	UFUNCTION()
	void HandleRecordCloseClicked();

	UFUNCTION()
	void HandleLunaChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	TArray<FPdButtonClickBinding, TInlineAllocator<10>> GetMenuButtonBindings() const;
	void ApplyWidgetDefinitionSettings();
	void BuildLunaSpeechBubble();
	void BuildLunaChatInput();
	FString GetResolvedLobbyTravelMapName() const;
	FString GetResolvedRoomTravelMapName() const;
	FString GetResolvedTrainingRoomTravelMapName() const;
	void OpenRoomList();
	void OpenTrainingRoom();
	void OpenShop();
	void OpenGuide();
	void OpenRecord();
	void BindRecordCloseButton();
	void UnbindRecordCloseButton();
	UButton* FindRecordCloseButton() const;
	// 팝업이 켜진 화면에 아직 없으면 입력을 막는 메뉴 화면에 담아 올린다.
	void PresentMenuPopup(UUserWidget* Popup, FSimpleDelegate OnBack);
	// 팝업을 담은 화면을 내리고 위젯을 뗀다.
	void DismissMenuPopup(UUserWidget* Popup) const;

	TSubclassOf<UShopWidget> ResolveShopWidgetClass() const;
	TSubclassOf<UUserWidget> ResolveGuideWidgetClass() const;
	TSubclassOf<UUserWidget> ResolveRecordWidgetClass() const;
	void LoadLocalProfile() const;
	void StartQuickMatch();
	void OpenLobbyAsListenServer() const;
	void SetQuickMatchEnabled(bool bEnabled) const;
	void ClearQuickMatchDelegates();

	UUiSubsystem* GetUiSubsystem() const;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_TitleCharacter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_RoomList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_QuickMatch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_TrainingMode;

	/** 보스 레이드: 전용 서버의 GameLift 보스 레이드 세션에 들어간다. WBP_Title에서 훈련장 줄을 함께 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_BossRaid;

	/** 공식 웹사이트를 시스템 브라우저로 연다. 브라우저를 열 수 없는 플랫폼에서는 보이지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Website;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI")
	FString OfficialWebsiteUrl = TEXT("https://pandora-archive.vercel.app/");

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_PandoraShop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Guide;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Record;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Tutorial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UAudioVolumeSlider> AudioVolumeSlider_;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UUserWidget> GuideWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UUserWidget> RecordWidgetClass;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxSearchResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxPublicConnections = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	FString QuickMatchRoomName = TEXT("Quick Match");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	bool bQuickMatchLAN = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	bool bQuickMatchUseLobbies = true;

	FDelegateHandle QuickMatchRequestCompleteHandle;
	uint64 ActiveQuickMatchRequestId = 0;

	UPROPERTY(Transient)
	TObjectPtr<UShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RecordWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> LunaSpeechBubble;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LunaSpeechText;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> LunaChatInput;

	FName LunaSpeechKey;
	FOnLunaQuestionSubmitted LunaQuestionSubmitted;
	FOnBossRaidRequested BossRaidRequested;
};
