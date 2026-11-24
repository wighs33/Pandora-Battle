#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "UI/Core/ScreenWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "Types/SlateEnums.h"
#include "Online/Backend/BackendClientSubsystem.h"
#include "UI/Common/MenuHoverHighlight.h"
#include "UI/Title/TitleCharacterOutfit.h"
#include "TitleWidget.generated.h"

class UButton;
class UAudioVolumeSlider;
class UProgressBar;
class UCanvasPanel;
class UCanvasPanelSlot;
struct FButtonClickBinding;
class UAudioVolumeControl;
class UEditableTextBox;
class UGuideWidget;
class URecordWidget;
class UShopWidget;
class UUiSubsystem;
class UImage;
class UMaterialInterface;
class UTextBlock;
class UWidget;
enum class ELunaServerState : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaQuestionSubmitted, const FString& /*Question*/);
DECLARE_MULTICAST_DELEGATE(FOnLunaDlcRequested);
DECLARE_MULTICAST_DELEGATE(FOnBossRaidRequested);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaOutfitSelected, int32 /*OutfitIndex*/);

class UTitleWidget;

/** Luna 옷 버튼 하나의 클릭을 그 옷 번호와 함께 타이틀에 넘긴다. UButton의 클릭 알림에는 어느 버튼인지가 없다. */
UCLASS()
class UTitleOutfitButtonRelay : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UTitleWidget& InTitle, int32 InOutfitIndex);

	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UTitleWidget> Title;
	int32 OutfitIndex = INDEX_NONE;
};

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UTitleWidget : public UScreenWidget
{
	GENERATED_BODY()

public:
	UTitleWidget(const FObjectInitializer& ObjectInitializer);

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Public API ------------------------------------------------------------------------------------------------------
	void SetTitleCharacterMaterial(UMaterialInterface* Material);

	/** 메뉴 버튼이 마우스 오버로 강조된 정도(0~1). */
	float GetMenuHoverProgress(const UWidget* Button) const;

	/** Luna의 번역된 대사를 머리 위에 보여 준다. HeadTopUV는 Img_TitleCharacter 텍스처 공간의 점이다. */
	bool ShowLunaSpeech(FName TextKey, const FVector2D& HeadTopUV);
	/** 메뉴 문구가 아닌 텍스트(Luna가 만든 답)를 보여 준다. 언어가 바뀌어도 그대로 둔다. */
	bool ShowLunaSpeechText(const FText& Text, const FVector2D& HeadTopUV);
	void HideLunaSpeech();

	/** 플레이어가 Luna 아래 질문 칸에서 Enter를 눌렀다. */
	FOnLunaQuestionSubmitted& OnLunaQuestionSubmitted() { return LunaQuestionSubmitted; }

	/** 플레이어가 질문 칸 옆의 DLC 받기 버튼을 눌렀다. */
	FOnLunaDlcRequested& OnLunaDlcRequested() { return LunaDlcRequested; }

	/** 모델 서버가 준비된 동안에만 질문 칸을 연다. 막은 동안은 칸의 안내 문구가 이유를 알리고, DLC가 없으면 받기 버튼을 보여 준다. */
	void SetLunaServerState(ELunaServerState State);

	/** 플레이어가 보스 레이드를 눌렀다. 타이틀 HUD가 백엔드에 레이드 자리(보스 레이드 세션)를 요청한다. */
	FOnBossRaidRequested& OnBossRaidRequested() { return BossRaidRequested; }
	/** 백엔드가 알려 준 개최 여부. 닫혀 있으면 버튼을 끄고, 마우스를 올리면 아직 열리지 않았다고 알린다. */
	void SetBossRaidOpen(bool bOpen);
	/** 백엔드가 자리를 잡는 동안 버튼을 꺼서 요청이 두 번 가지 않게 한다. */
	void SetBossRaidJoining(bool bJoining);
	/**
	 * 지금 회차의 전역 체력과 내 기록. 툴팁에 체력, 오늘 남은 도전, 내 순위와 상위 순위를 덧붙이고,
	 * 오늘 도전을 다 썼으면 버튼을 끈다.
	 */
	void SetBossRaidDetails(const FBackendBossRaidStatus& Status, const FBackendBossRaidStanding& Standing);
	/** 보상 골드처럼 타이틀 밖에서 바뀐 로컬 프로필을 다시 보여 준다. */
	void RefreshLocalProfile() const { LoadLocalProfile(); }

	/** Luna 초상화 오른쪽에 옷 버튼을 Outfits 순서대로 세운다. 타이틀 HUD가 한 번 부른다. */
	void SetLunaOutfits(TConstArrayView<FTitleCharacterOutfit> Outfits);
	/** 지금 입은 옷의 버튼을 굵은 금색 테두리로 표시한다. */
	void SetSelectedLunaOutfit(int32 OutfitIndex);
	/** 플레이어가 옷 버튼을 눌렀다. 타이틀 HUD가 Luna에게 그 옷을 입힌다. */
	FOnLunaOutfitSelected& OnLunaOutfitSelected() { return LunaOutfitSelected; }
	void SelectLunaOutfit(int32 OutfitIndex) { LunaOutfitSelected.Broadcast(OutfitIndex); }

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
	void HandleBossRaidPracticeClicked();

	UFUNCTION()
	void HandleWebsiteClicked();

private:
	void HandleQuickMatchRequestComplete(uint64 RequestId, bool bWasSuccessful, bool bCreatedRoom);

	UFUNCTION()
	void HandleRecordCloseClicked();

	UFUNCTION()
	void HandleLunaChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleLunaDlcClicked();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	TArray<FButtonClickBinding, TInlineAllocator<11>> GetMenuButtonBindings() const;
	void ApplyWidgetDefinitionSettings();
	void BuildLunaSpeechBubble();
	void BuildLunaChatInput();
	void BuildLunaDlcButton(UCanvasPanel& Canvas, const UCanvasPanelSlot& InputSlot);
	void PlaceLunaOutfitButtons();
	void ApplyLunaOutfitStyles();
	void ApplyLunaOutfitTooltips();
	void ApplyLunaChatAvailability();
	void BuildVersionLabel();
	void BuildMenuHoverHighlight();
	void BuildBossRaidSummary();
	void ApplyBossRaidSummary();
	void ApplyBossRaidAvailability();
	FText BuildBossRaidDetails() const;
	FString GetResolvedLobbyTravelMapName() const;
	FString GetResolvedRoomTravelMapName() const;
	FString GetResolvedTrainingRoomTravelMapName() const;
	void OpenRoomList();
	void OpenTrainingRoom();
	void OpenBossRaidPractice();
	void OpenShop();
	void OpenGuide();
	void OpenRecord();
	// 팝업 화면이 아직 떠 있지 않으면 메뉴 층에 올린다. 뒤로 가기는 OnBack이 맡는다.
	void PresentMenuPopup(UScreenWidget* Popup, FSimpleDelegate OnBack);
	void DismissMenuPopup(UScreenWidget* Popup) const;

	TSubclassOf<UShopWidget> ResolveShopWidgetClass() const;
	TSubclassOf<UGuideWidget> ResolveGuideWidgetClass() const;
	TSubclassOf<URecordWidget> ResolveRecordWidgetClass() const;
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

	/** 보스 레이드 연습: 백엔드 없이 이 PC에서 보스 레이드 맵을 열어 혼자 보스와 싸운다. 레이드가 닫혀 있어도 열려 있다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_BossRaidPractice;

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
	TSubclassOf<UGuideWidget> GuideWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<URecordWidget> RecordWidgetClass;

	/** 판 없는 글자 목록 메뉴에 마우스를 올렸을 때의 강조 모양 */
	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI")
	FMenuHoverHighlightStyle MenuHoverStyle;

	/** 메뉴 뒤 봉인(마법진)의 바깥 글자 띠. 반시계 방향으로 아주 느리게 돈다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Img_SealRunes;

	/** 봉인의 안쪽 글자 고리. 바깥 띠와 반대 방향으로 돈다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Img_SealInner;

	/** 바깥 글자 띠가 한 바퀴 도는 시간(초). Docs/UI_Design_Plan.md 5장. */
	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI", meta = (ClampMin = "1.0"))
	float SealRunesTurnSeconds = 240.f;

	/** 안쪽 글자 고리가 한 바퀴 도는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI", meta = (ClampMin = "1.0"))
	float SealInnerTurnSeconds = 120.f;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxSearchResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxPublicConnections = PdGameSession::MaxPlayerCount;

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
	TObjectPtr<UGuideWidget> GuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<URecordWidget> RecordWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> LunaSpeechBubble;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LunaSpeechText;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> LunaChatInput;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LunaDlcButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LunaDlcLabel;

	UPROPERTY(Transient)
	TObjectPtr<UMenuHoverHighlight> MenuHoverHighlight;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> LunaOutfitButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTitleOutfitButtonRelay>> LunaOutfitButtonRelays;

	/** 옷 버튼마다 마우스를 올리면 보이는 옷 이름(MenuText 키) */
	TArray<FName> LunaOutfitNameKeys;
	int32 SelectedLunaOutfit = INDEX_NONE;
	FOnLunaOutfitSelected LunaOutfitSelected;

	/** 보스 레이드 줄 오른쪽의 보스 이름, 남은 체력 막대와 비율 */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> BossRaidSummary;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossRaidNameText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BossRaidHealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossRaidHealthText;

	FName LunaSpeechKey;
	ELunaServerState LunaServerState = {};
	FOnLunaQuestionSubmitted LunaQuestionSubmitted;
	FOnLunaDlcRequested LunaDlcRequested;
	FOnBossRaidRequested BossRaidRequested;
	bool bBossRaidOpen = false;
	bool bBossRaidJoining = false;
	FBackendBossRaidStatus BossRaidStatus;
	FBackendBossRaidStanding BossRaidStanding;
	float SealElapsedSeconds = 0.f;
};
