#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "LobbyUserWidget.generated.h"

class ALobbyPlayerState;
class UBorder;
class UButton;
class UComboBoxString;
class UImage;
class UOverlay;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULobbyUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void SetInfo(ALobbyPlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void RefreshUI();

protected:
	UFUNCTION()
	void HandleKickClicked();

	UFUNCTION()
	void HandleFriendAddClicked();

	UFUNCTION()
	void HandlePlayerNameCommitted(const FText& InText, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleTeamColorSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UTextBlock> Txt_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UEditableTextBox> Editable_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_KickPlayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_FriendAdd;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UOverlay> Overlay_AddFriend;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UImage> Img_OwnerMark;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UComboBoxString> Cbb_TeamColor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UBorder> ColorBorder;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Lobby|UI")
	TObjectPtr<ALobbyPlayerState> PlayerState;

private:
	void EnsureTeamColorOptions();
	void RefreshTeamColorUI();
	void SetColorBorderByTeamColorIndex(int32 TeamColorIndex);
	bool IsRepresentingLocalPlayer() const;
	bool CanLocalPlayerKick() const;
	bool IsLobbyOwnerPlayer() const;
	FString GetRepresentedSteamIdString() const;
	bool OpenSteamFriendAddOverlay() const;
};
