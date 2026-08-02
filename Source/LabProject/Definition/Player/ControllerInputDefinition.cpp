#include "Definition/Player/ControllerInputDefinition.h"

#include "InputAction.h"
#include "Definition/Player/CharacterActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerInputDefinition)

namespace
{
	constexpr const TCHAR* DefaultInputDefinitionPathName = TEXT("/Game/Input/DA_Input.DA_Input");

	TSoftObjectPtr<UInputAction> MakeControllerInputActionReference(const TCHAR* Path)
	{
		return TSoftObjectPtr<UInputAction>(FSoftObjectPath(Path));
	}

	void AddInputActionIcon(
		TArray<FPdInputActionIconMapping>& Mappings,
		const TCHAR* InputActionPath)
	{
		FPdInputActionIconMapping& Mapping = Mappings.AddDefaulted_GetRef();
		Mapping.InputAction = MakeControllerInputActionReference(InputActionPath);
	}

	bool DoesMappingMatchInputAction(const FPdInputActionIconMapping& Mapping, const UInputAction* InputAction)
	{
		if (!InputAction || Mapping.InputAction.IsNull())
		{
			return false;
		}

		if (Mapping.InputAction.Get() == InputAction)
		{
			return true;
		}

		return Mapping.InputAction.ToSoftObjectPath().ToString() == InputAction->GetPathName();
	}

#if WITH_EDITOR
	void MarkControllerInputInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	FText ControllerInputFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	void ValidateRequiredInputAction(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSoftObjectPtr<UInputAction>& InputAction,
		const TCHAR* FieldName)
	{
		if (InputAction.IsNull())
		{
			MarkControllerInputInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ControllerInputDefinition", "MissingInputAction", "{0} is required."),
				ControllerInputFieldText(FieldName)));
		}
	}
#endif
}

UControllerInputDefinition::UControllerInputDefinition()
{
	PaintInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Action/IA_Paint.IA_Paint")));
	Skill4InputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Action/IA_Skill4.IA_Skill4")));
	OpenSettingUiInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Action/IA_OpenSettingUI.IA_OpenSettingUI")));
	EscapeInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Action/IA_Escape.IA_Escape")));
	OpenLobbyInputAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Action/IA_OpenLobby.IA_OpenLobby")));
	CharacterActionDefinition = TSoftObjectPtr<UCharacterActionDefinition>(
		FSoftObjectPath(TEXT("/Game/Data/DA_CharacterAction.DA_CharacterAction")));

	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Move.IA_Move"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Look.IA_Look"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Jump.IA_Jump"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Crouch.IA_Crouch"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Interact.IA_Interact"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Attack.IA_Attack"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Aim.IA_Aim"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Paint.IA_Paint"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Grapple.IA_Grapple"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Skill1.IA_Skill1"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Skill2.IA_Skill2"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Skill3.IA_Skill3"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Skill4.IA_Skill4"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_QuickSlot1.IA_QuickSlot1"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_QuickSlot2.IA_QuickSlot2"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_QuickSlot3.IA_QuickSlot3"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_QuickSlot4.IA_QuickSlot4"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_GestureSlot1.IA_GestureSlot1"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_GestureSlot2.IA_GestureSlot2"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_GestureSlot3.IA_GestureSlot3"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_GestureSlot4.IA_GestureSlot4"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_TargetConfirm.IA_TargetConfirm"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_OpenInfoUI_Item.IA_OpenInfoUI_Item"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_OpenSettingUI.IA_OpenSettingUI"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_Escape.IA_Escape"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_OpenLobby.IA_OpenLobby"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_SelectPandora.IA_SelectPandora"));
	AddInputActionIcon(InputActionIconMappings, TEXT("/Game/Input/Action/IA_OpenPandoraTree.IA_OpenPandoraTree"));
}

FPrimaryAssetId UControllerInputDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ControllerInputDefinition"), GetFName());
}

FSoftObjectPath UControllerInputDefinition::GetDefaultInputDefinitionPath()
{
	return FSoftObjectPath(DefaultInputDefinitionPathName);
}

UObject* UControllerInputDefinition::ResolveInputActionIconObject(const UInputAction* InputAction) const
{
	for (const FPdInputActionIconMapping& Mapping : InputActionIconMappings)
	{
		if (!DoesMappingMatchInputAction(Mapping, InputAction))
		{
			continue;
		}

		return Mapping.Icon.Get();
	}

	return nullptr;
}

void UControllerInputDefinition::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	const auto AddSoftPath = [&OutAssetPaths](const auto& SoftObject)
	{
		if (!SoftObject.IsNull())
		{
			OutAssetPaths.AddUnique(SoftObject.ToSoftObjectPath());
		}
	};

	AddSoftPath(InputMapping);
	AddSoftPath(MoveInputAction);
	AddSoftPath(LookInputAction);
	AddSoftPath(JumpInputAction);
	AddSoftPath(CrouchInputAction);
	AddSoftPath(InteractInputAction);
	AddSoftPath(AttackInputAction);
	AddSoftPath(AimInputAction);
	AddSoftPath(PaintInputAction);
	AddSoftPath(GrappleInputAction);
	AddSoftPath(Skill1InputAction);
	AddSoftPath(Skill2InputAction);
	AddSoftPath(Skill3InputAction);
	AddSoftPath(Skill4InputAction);
	AddSoftPath(QuickSlot1InputAction);
	AddSoftPath(QuickSlot2InputAction);
	AddSoftPath(QuickSlot3InputAction);
	AddSoftPath(QuickSlot4InputAction);
	AddSoftPath(Gesture1InputAction);
	AddSoftPath(Gesture2InputAction);
	AddSoftPath(Gesture3InputAction);
	AddSoftPath(Gesture4InputAction);
	AddSoftPath(TargetConfirmInputAction);
	AddSoftPath(OpenInfoProfileInputAction);
	AddSoftPath(OpenInfoItemInputAction);
	AddSoftPath(OpenInfoSkinInputAction);
	AddSoftPath(OpenInfoPandoraInputAction);
	AddSoftPath(OpenInfoMapInputAction);
	AddSoftPath(OpenSettingUiInputAction);
	AddSoftPath(EscapeInputAction);
	AddSoftPath(OpenLobbyInputAction);
	AddSoftPath(SelectPandoraInputAction);
	AddSoftPath(PandoraTreeInputAction);
	AddSoftPath(CharacterActionDefinition);

	for (const FPdInputActionIconMapping& Mapping : InputActionIconMappings)
	{
		AddSoftPath(Mapping.InputAction);
		AddSoftPath(Mapping.Icon);
	}
}

bool UControllerInputDefinition::IsOpenLobbyInputAllowedForMap(const FString& LevelName) const
{
	if (LevelName.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	for (const FName AllowedMapName : OpenLobbyAllowedMapNames)
	{
		if (AllowedMapName.IsNone())
		{
			continue;
		}

		if (LevelName.Equals(AllowedMapName.ToString(), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
EDataValidationResult UControllerInputDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (InputMapping.IsNull())
	{
		MarkControllerInputInvalid(Context, Result, NSLOCTEXT("ControllerInputDefinition", "MissingInputMapping", "InputMapping is required."));
	}

	ValidateRequiredInputAction(Context, Result, MoveInputAction, TEXT("MoveInputAction"));
	ValidateRequiredInputAction(Context, Result, LookInputAction, TEXT("LookInputAction"));
	ValidateRequiredInputAction(Context, Result, JumpInputAction, TEXT("JumpInputAction"));
	ValidateRequiredInputAction(Context, Result, CrouchInputAction, TEXT("CrouchInputAction"));
	ValidateRequiredInputAction(Context, Result, InteractInputAction, TEXT("InteractInputAction"));
	ValidateRequiredInputAction(Context, Result, AttackInputAction, TEXT("AttackInputAction"));
	ValidateRequiredInputAction(Context, Result, AimInputAction, TEXT("AimInputAction"));
	ValidateRequiredInputAction(Context, Result, EscapeInputAction, TEXT("EscapeInputAction"));

	const bool bHasNativeInputAction =
		!MoveInputAction.IsNull()
		|| !LookInputAction.IsNull()
		|| !JumpInputAction.IsNull()
		|| !CrouchInputAction.IsNull()
		|| !InteractInputAction.IsNull()
		|| !AttackInputAction.IsNull()
		|| !AimInputAction.IsNull()
		|| !PaintInputAction.IsNull()
		|| !GrappleInputAction.IsNull()
		|| !Skill1InputAction.IsNull()
		|| !Skill2InputAction.IsNull()
		|| !Skill3InputAction.IsNull()
		|| !Skill4InputAction.IsNull()
		|| !QuickSlot1InputAction.IsNull()
		|| !QuickSlot2InputAction.IsNull()
		|| !QuickSlot3InputAction.IsNull()
		|| !QuickSlot4InputAction.IsNull()
		|| !Gesture1InputAction.IsNull()
		|| !Gesture2InputAction.IsNull()
		|| !Gesture3InputAction.IsNull()
		|| !Gesture4InputAction.IsNull()
		|| !TargetConfirmInputAction.IsNull()
		|| !OpenInfoProfileInputAction.IsNull()
		|| !OpenInfoItemInputAction.IsNull()
		|| !OpenInfoSkinInputAction.IsNull()
		|| !OpenInfoPandoraInputAction.IsNull()
		|| !OpenInfoMapInputAction.IsNull()
		|| !OpenSettingUiInputAction.IsNull()
		|| !EscapeInputAction.IsNull()
		|| !OpenLobbyInputAction.IsNull()
		|| !SelectPandoraInputAction.IsNull()
		|| !PandoraTreeInputAction.IsNull();

	if (!bHasNativeInputAction)
	{
		Context.AddWarning(NSLOCTEXT("ControllerInputDefinition", "NoInputActions", "ControllerInputDefinition has no input actions."));
	}

	if (!OpenLobbyInputAction.IsNull() && OpenLobbyAllowedMapNames.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT(
			"ControllerInputDefinition",
			"OpenLobbyNoAllowedMaps",
			"OpenLobbyInputAction is set, but OpenLobbyAllowedMapNames is empty."));
	}

	TSet<FSoftObjectPath> IconMappedActions;
	for (int32 Index = 0; Index < InputActionIconMappings.Num(); ++Index)
	{
		const FPdInputActionIconMapping& Mapping = InputActionIconMappings[Index];
		if (Mapping.InputAction.IsNull())
		{
			MarkControllerInputInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ControllerInputDefinition", "EmptyIconInputAction", "InputActionIconMappings[{0}].InputAction must be set."),
				FText::AsNumber(Index)));
			continue;
		}

		const FSoftObjectPath ActionPath = Mapping.InputAction.ToSoftObjectPath();
		if (IconMappedActions.Contains(ActionPath))
		{
			MarkControllerInputInvalid(Context, Result, FText::Format(
				NSLOCTEXT("ControllerInputDefinition", "DuplicateIconInputAction", "InputActionIconMappings contains duplicate InputAction: {0}"),
				FText::FromString(Mapping.InputAction.ToString())));
		}
		IconMappedActions.Add(ActionPath);
	}

	return Result;
}
#endif
