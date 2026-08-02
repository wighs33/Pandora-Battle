#include "Definition/UI/WidgetDefinitionFragments.h"

#include "Common/LabGameplayTags.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "UI/InfoUiPresenter.h"
#include "UI/Widget/ActionSlotEntryWidget.h"
#include "UI/Widget/KillBoxWidget.h"
#include "UI/Widget/QuickSlotEntryWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetDefinitionFragments)

namespace
{
	TSoftObjectPtr<UInputAction> MakeWidgetInputActionReference(const TCHAR* Path)
	{
		return TSoftObjectPtr<UInputAction>(FSoftObjectPath(Path));
	}

	TSoftObjectPtr<UObject> MakeInputKeyIconReference(const TCHAR* AssetName)
	{
		return TSoftObjectPtr<UObject>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/UI/Asset/Images/InputKeys/%s.%s"),
			AssetName,
			AssetName)));
	}

	void AddInputKeyIcon(
		TMap<FString, TSoftObjectPtr<UObject>>& Mapping,
		const FString& KeyName,
		const TCHAR* AssetName)
	{
		Mapping.Add(KeyName, MakeInputKeyIconReference(AssetName));
	}

	void InitializeDefaultInputKeyIcons(FPdInputKeyIconSettings& Settings)
	{
		TMap<FString, TSoftObjectPtr<UObject>>& Mapping = Settings.StringToIconMapping;
		AddInputKeyIcon(Mapping, TEXT("1"), TEXT("1-key"));
		AddInputKeyIcon(Mapping, TEXT("One"), TEXT("1-key"));
		AddInputKeyIcon(Mapping, TEXT("2"), TEXT("2-key"));
		AddInputKeyIcon(Mapping, TEXT("Two"), TEXT("2-key"));
		AddInputKeyIcon(Mapping, TEXT("3"), TEXT("3-key"));
		AddInputKeyIcon(Mapping, TEXT("Three"), TEXT("3-key"));
		AddInputKeyIcon(Mapping, TEXT("4"), TEXT("4-key"));
		AddInputKeyIcon(Mapping, TEXT("Four"), TEXT("4-key"));
		AddInputKeyIcon(Mapping, TEXT("5"), TEXT("5-key"));
		AddInputKeyIcon(Mapping, TEXT("Five"), TEXT("5-key"));
		AddInputKeyIcon(Mapping, TEXT("6"), TEXT("6-key"));
		AddInputKeyIcon(Mapping, TEXT("Six"), TEXT("6-key"));
		AddInputKeyIcon(Mapping, TEXT("7"), TEXT("7-key"));
		AddInputKeyIcon(Mapping, TEXT("Seven"), TEXT("7-key"));
		AddInputKeyIcon(Mapping, TEXT("8"), TEXT("8-key"));
		AddInputKeyIcon(Mapping, TEXT("Eight"), TEXT("8-key"));
		AddInputKeyIcon(Mapping, TEXT("C"), TEXT("C-key"));
		AddInputKeyIcon(Mapping, TEXT("E"), TEXT("E-key"));
		AddInputKeyIcon(Mapping, TEXT("F"), TEXT("F-key"));
		AddInputKeyIcon(Mapping, TEXT("Q"), TEXT("Q-key"));
		AddInputKeyIcon(Mapping, TEXT("R"), TEXT("R-key"));
		AddInputKeyIcon(Mapping, TEXT("Shift"), TEXT("shift"));
		AddInputKeyIcon(Mapping, TEXT("LeftShift"), TEXT("shift"));
		AddInputKeyIcon(Mapping, TEXT("Left Shift"), TEXT("shift"));
		AddInputKeyIcon(Mapping, TEXT("RightShift"), TEXT("shift"));
		AddInputKeyIcon(Mapping, TEXT("Right Shift"), TEXT("shift"));
		AddInputKeyIcon(Mapping, TEXT("V"), TEXT("V-key"));
		AddInputKeyIcon(Mapping, TEXT("X"), TEXT("X-key"));
		AddInputKeyIcon(Mapping, TEXT("Z"), TEXT("Z-key"));
	}
}

UWidgetUIClassesDefinition::UWidgetUIClassesDefinition()
{
	PlayerHudWidgetTag = LabGameplayTags::UI_Widget_PlayerHUD;
	InfoWidgetTag = LabGameplayTags::UI_Widget_Info;
	InfoPresenterClass = UInfoUiPresenter::StaticClass();
	SelectPandoraWidgetTag = LabGameplayTags::UI_Widget_SelectPandora;
	AimCrosshairWidgetTag = LabGameplayTags::UI_Widget_AimCrosshair;
	PandoraTreeWidgetTag = LabGameplayTags::UI_Widget_PandoraTree;
	KillBoxEntryWidgetClass = TSoftClassPtr<UKillBoxWidget>(
		FSoftClassPath(TEXT("/Game/UI/Widget/HUD/WBP_KillBox.WBP_KillBox_C")));
	QuickSlotEntryWidgetClass = TSoftClassPtr<UQuickSlotEntryWidget>(
		FSoftClassPath(TEXT("/Game/UI/Widget/HUD/WBP_QuickSlotEntry.WBP_QuickSlotEntry_C")));
	ActionSlotEntryWidgetClass = TSoftClassPtr<UActionSlotEntryWidget>(
		FSoftClassPath(TEXT("/Game/UI/Widget/HUD/WBP_ActionSlotEntry.WBP_ActionSlotEntry_C")));
}

FPrimaryAssetId UWidgetUIClassesDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetUIClassesDefinition"), GetFName());
}

UWidgetStyleDefinition::UWidgetStyleDefinition()
{
	ChatWidgetSettings.ChatScrollBoxCandidateNames =
	{
		TEXT("ScrollBox_ChatMessages"),
		TEXT("ScrollBox_MessageList"),
		TEXT("ChatScrollBox"),
		TEXT("MessageScrollBox"),
		TEXT("ScrollBox")
	};
	ChatWidgetSettings.ChatInputCandidateNames =
	{
		TEXT("TxtBox_ChatInput"),
		TEXT("EditableText_ChatInput"),
		TEXT("Input_Chat"),
		TEXT("ChatInput"),
		TEXT("InputMessage"),
		TEXT("EditableTextBox"),
		TEXT("EditableText")
	};
}

FPrimaryAssetId UWidgetStyleDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetStyleDefinition"), GetFName());
}

UWidgetInputIconsDefinition::UWidgetInputIconsDefinition()
{
	InitializeDefaultInputKeyIcons(AbilitySlotWidgetSettings.InputKeyIconSettings);
	AbilitySlotWidgetSettings.Skill1InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_Skill1.IA_Skill1"));
	AbilitySlotWidgetSettings.Skill2InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_Skill2.IA_Skill2"));
	AbilitySlotWidgetSettings.Skill3InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_Skill3.IA_Skill3"));
	AbilitySlotWidgetSettings.Skill4InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_Skill4.IA_Skill4"));

	InitializeDefaultInputKeyIcons(QuickSlotWidgetSettings.InputKeyIconSettings);
	QuickSlotWidgetSettings.SlotCount = 8;
	QuickSlotWidgetSettings.QuickSlot1InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_QuickSlot1.IA_QuickSlot1"));
	QuickSlotWidgetSettings.QuickSlot2InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_QuickSlot2.IA_QuickSlot2"));
	QuickSlotWidgetSettings.QuickSlot3InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_QuickSlot3.IA_QuickSlot3"));
	QuickSlotWidgetSettings.QuickSlot4InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_QuickSlot4.IA_QuickSlot4"));
	QuickSlotWidgetSettings.GestureSlot1InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_GestureSlot1.IA_GestureSlot1"));
	QuickSlotWidgetSettings.GestureSlot2InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_GestureSlot2.IA_GestureSlot2"));
	QuickSlotWidgetSettings.GestureSlot3InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_GestureSlot3.IA_GestureSlot3"));
	QuickSlotWidgetSettings.GestureSlot4InputAction = MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_GestureSlot4.IA_GestureSlot4"));

	InitializeDefaultInputKeyIcons(ActionSlotWidgetSettings.InputKeyIconSettings);
	ActionSlotWidgetSettings.CharacterActionDefinition = TSoftObjectPtr<UCharacterActionDefinition>(
		FSoftObjectPath(TEXT("/Game/Data/DA_CharacterAction.DA_CharacterAction")));
	ActionSlotWidgetSettings.PandoraWeaponSwapInputAction =
		MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_SelectPandora.IA_SelectPandora"));
	ActionSlotWidgetSettings.GrappleHookInputAction =
		MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_Grapple.IA_Grapple"));
	TogglePandoraTreeInputAction =
		MakeWidgetInputActionReference(TEXT("/Game/Input/Action/IA_OpenPandoraTree.IA_OpenPandoraTree"));
}

FPrimaryAssetId UWidgetInputIconsDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetInputIconsDefinition"), GetFName());
}

UWidgetMapUIDefinition::UWidgetMapUIDefinition()
{
	MapWidgetSettings.AreaMapTexture =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Map/Map.Map")));
	MapWidgetSettings.WindmillMapTexture =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Map/overmap.overmap")));
	MapWidgetSettings.DomeMapTexture =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Map/undermap.undermap")));
	MapWidgetSettings.TempleMapTexture = MapWidgetSettings.WindmillMapTexture;
	TrainingRoomMapNames.Add(TEXT("LV_TrainingRoom"));
}

FPrimaryAssetId UWidgetMapUIDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetMapUIDefinition"), GetFName());
}
