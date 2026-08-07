#include "Lobby/UI/GameConfigWidget.h"

#include "Common/GameSessionConstants.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/PdHUD.h"
#include "Mode/PdGameInstance.h"
#include "InputCoreTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameConfigWidget)

void UGameConfigWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetFocus();

	if (Btn_Back)
	{
		Btn_Back->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (ComboBox_Map)
	{
		ComboBox_Map->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleMapSelectionChanged);
	}

	const UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (PdGameInstance)
	{
		if (Editable_MaxPlayerCount)
		{
			Editable_MaxPlayerCount->SetText(FText::AsNumber(PdGameInstance->GetLobbyMaxPlayerCount()));
		}

		if (Editable_MaxBotCount)
		{
			Editable_MaxBotCount->SetText(FText::AsNumber(PdGameInstance->GetLobbyMaxBotCount()));
		}

	}

	if (ComboBox_Map)
	{
		const FName SavedMapKey = PdGameInstance ? PdGameInstance->GetLobbySelectedMapKey() : NAME_None;
		const FString SavedMapOption = SavedMapKey.ToString();
		const bool bHasSavedMapOption = !SavedMapKey.IsNone()
			&& ComboBox_Map->FindOptionIndex(SavedMapOption) != INDEX_NONE;

		if (bHasSavedMapOption)
		{
			ComboBox_Map->SetSelectedOption(SavedMapOption);
		}
		else if (ComboBox_Map->GetOptionCount() > 0)
		{
			ComboBox_Map->SetSelectedIndex(0);
		}
	}

	RefreshUI();
}

FReply UGameConfigWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (APdHUD* Hud = PlayerController->GetHUD<APdHUD>())
		{
			Hud->HandleEscapeInput();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGameConfigWidget::NativeDestruct()
{
	if (Btn_Back)
	{
		Btn_Back->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackClicked);
	}

	if (ComboBox_Map)
	{
		ComboBox_Map->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleMapSelectionChanged);
	}

	Super::NativeDestruct();
}

void UGameConfigWidget::RefreshUI()
{
	ApplySelectedMapThumbnail(GetSelectedMapKey());
}

void UGameConfigWidget::SaveConfig()
{
	ALobbyGameMode* LobbyGameMode = Cast<ALobbyGameMode>(UGameplayStatics::GetGameMode(this));
	if (!LobbyGameMode)
	{
		return;
	}

	const UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	const int32 DefaultMaxPlayers = PdGameInstance ? PdGameInstance->GetLobbyMaxPlayerCount() : LabGameSession::MaxPlayerCount;
	const int32 DefaultMaxBots = PdGameInstance ? PdGameInstance->GetLobbyMaxBotCount() : 10;

	LobbyGameMode->SaveConfig(
		GetSelectedMapKey(),
		ParseClampedInt(Editable_MaxPlayerCount, DefaultMaxPlayers, 1, LabGameSession::MaxPlayerCount),
		ParseClampedInt(Editable_MaxBotCount, DefaultMaxBots, 0, 100));
}

FName UGameConfigWidget::GetSelectedMapKey() const
{
	if (!ComboBox_Map)
	{
		return NAME_None;
	}

	const FString SelectedOption = ComboBox_Map->GetSelectedOption();
	if (!SelectedOption.IsEmpty())
	{
		return FName(*SelectedOption);
	}

	return GetFirstComboBoxMapKey();
}

void UGameConfigWidget::HandleBackClicked()
{
	SaveConfig();
	RemoveFromParent();
}

void UGameConfigWidget::HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	static_cast<void>(SelectedItem);
	static_cast<void>(SelectionType);
	RefreshUI();
	BP_OnSelectedMapChanged(GetSelectedMapKey());
}

void UGameConfigWidget::ApplySelectedMapThumbnail(const FName SelectedMapKey)
{
	const bool bShowTestThumbnail = SelectedMapKey == TestMapKey;
	const bool bShowWindNestThumbnail = SelectedMapKey == WindNestMapKey;

	if (Img_MapThumbnail_WindNest)
	{
		Img_MapThumbnail_WindNest->SetVisibility(bShowWindNestThumbnail ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Img_MapThumbnail_Test)
	{
		Img_MapThumbnail_Test->SetVisibility(bShowTestThumbnail ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FName UGameConfigWidget::GetFirstComboBoxMapKey() const
{
	if (!ComboBox_Map || ComboBox_Map->GetOptionCount() <= 0)
	{
		return NAME_None;
	}

	const FString FirstOption = ComboBox_Map->GetOptionAtIndex(0);
	return FirstOption.IsEmpty() ? NAME_None : FName(*FirstOption);
}

int32 UGameConfigWidget::ParseClampedInt(const UEditableTextBox* TextBox, const int32 DefaultValue, const int32 MinValue, const int32 MaxValue)
{
	if (!TextBox)
	{
		return FMath::Clamp(DefaultValue, MinValue, MaxValue);
	}

	const FString TextValue = TextBox->GetText().ToString();
	if (!TextValue.IsNumeric())
	{
		return FMath::Clamp(DefaultValue, MinValue, MaxValue);
	}

	return FMath::Clamp(FCString::Atoi(*TextValue), MinValue, MaxValue);
}
