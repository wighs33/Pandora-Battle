#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "Types/SlateEnums.h"
#include "TitleWidget.generated.h"

class UButton;
class UAudioVolumeSlider;
class UAudioVolumeControl;
class UEditableTextBox;
class UGuideWidget;
class UShopWidget;
class UUiSubsystem;
class UImage;
class UMaterialInterface;
class UTextBlock;
class UTexture2D;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLunaQuestionSubmitted, const FString& /*Question*/);
DECLARE_MULTICAST_DELEGATE(FOnRpgModeRequested);

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
	UTitleWidget(const FObjectInitializer& ObjectInitializer);
	void SetTitleCharacterMaterial(UMaterialInterface* Material);

	/** Shows Luna's localized line above her head. HeadTopUV is a point in Img_TitleCharacter's texture space. */
	bool ShowLunaSpeech(FName TextKey, const FVector2D& HeadTopUV);
	/** Shows text that is not a menu line (Luna's generated reply). It is kept as is when the language changes. */
	bool ShowLunaSpeechText(const FText& Text, const FVector2D& HeadTopUV);
	void HideLunaSpeech();

	/** The player pressed Enter in the question box under Luna. */
	FOnLunaQuestionSubmitted& OnLunaQuestionSubmitted() { return LunaQuestionSubmitted; }

	/** The player pressed Boss Raid. The title HUD asks the backend for a place in a raid (an rpg-mode session). */
	FOnRpgModeRequested& OnRpgModeRequested() { return RpgModeRequested; }
	/** Disabled while the backend places the player, so the request is not sent twice. */
	void SetRpgModeEnabled(bool bEnabled) const;

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
	void HandleRpgModeClicked();

	UFUNCTION()
	void HandleWebsiteClicked();

private:
	void HandleQuickMatchRequestComplete(uint64 RequestId, bool bWasSuccessful, bool bCreatedRoom);

	UFUNCTION()
	void HandleRecordCloseClicked();

	UFUNCTION()
	void HandleLunaChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyWidgetDefinitionSettings();
	void BuildLunaSpeechBubble();
	void BuildLunaChatInput();
	void BuildRpgModeButton();
	void RefreshRpgModeText();
	void BuildWebsiteButton();
	void RefreshWebsiteText();
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

	/**
	 * Boss Raid: joins an rpg-mode GameLift session on a dedicated server. When WBP_Title has no such button, it is built
	 * at runtime and the training row is split in two to make room.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_RpgMode;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI")
	TSoftObjectPtr<UTexture2D> RpgModeIcon;

	/** Frame art for the half-width training and boss raid buttons. */
	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI")
	TSoftObjectPtr<UTexture2D> HalfRowFrame;

	/**
	 * Opens the official website in the system browser. When WBP_Title has no such button, it is built at runtime under
	 * the game settings button with the same frame. Platforms that cannot open a browser do not show it.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Website;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby|UI")
	TSoftObjectPtr<UTexture2D> WebsiteIcon;

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

	/** Label of the runtime-built boss raid button. WBP_Title's own labels are localized through MenuTextBindings. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RpgModeLabel;

	/** Label of the runtime-built website button. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WebsiteLabel;

	FName LunaSpeechKey;
	FOnLunaQuestionSubmitted LunaQuestionSubmitted;
	FOnRpgModeRequested RpgModeRequested;
};
