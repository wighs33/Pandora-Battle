#include "Definition/Player/ControllerInputDefinition.h"

#include "InputAction.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Player/CharacterActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerInputDefinition)

namespace
{
	bool DoesMappingMatchInputAction(const FInputActionIconMapping& Mapping, const UInputAction* InputAction)
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

FPrimaryAssetId UControllerInputDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ControllerInputDefinition"), GetFName());
}

FSoftObjectPath UControllerInputDefinition::GetDefaultInputDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.ControllerInput.ToSoftObjectPath();
}

TSoftObjectPtr<UCharacterActionDefinition>
UControllerInputDefinition::GetEffectiveCharacterActionDefinition() const
{
	if (!CharacterActionDefinition.IsNull())
	{
		return CharacterActionDefinition;
	}

	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.CharacterAction;
}

UInputAction* UControllerInputDefinition::GetLoadedSkillInputAction(
	const int32 SkillSlotIndex) const
{
	switch (SkillSlotIndex)
	{
	case 0:
		return Skill1InputAction.Get();
	case 1:
		return Skill2InputAction.Get();
	case 2:
		return Skill3InputAction.Get();
	case 3:
		return Skill4InputAction.Get();
	default:
		return nullptr;
	}
}

UInputAction* UControllerInputDefinition::GetLoadedQuickSlotInputAction(
	const int32 QuickSlotIndex) const
{
	switch (QuickSlotIndex)
	{
	case 0:
		return QuickSlot1InputAction.Get();
	case 1:
		return QuickSlot2InputAction.Get();
	case 2:
		return QuickSlot3InputAction.Get();
	case 3:
		return QuickSlot4InputAction.Get();
	case 4:
		return Gesture1InputAction.Get();
	case 5:
		return Gesture2InputAction.Get();
	case 6:
		return Gesture3InputAction.Get();
	case 7:
		return Gesture4InputAction.Get();
	default:
		return nullptr;
	}
}

UObject* UControllerInputDefinition::ResolveInputActionIconObject(const UInputAction* InputAction) const
{
	for (const FInputActionIconMapping& Mapping : InputActionIconMappings)
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
	AddSoftPath(ScoreboardInputAction);
	AddSoftPath(ChatInputAction);
	AddSoftPath(ChatScrollInputAction);
	AddSoftPath(GetEffectiveCharacterActionDefinition());

	for (const FInputActionIconMapping& Mapping : InputActionIconMappings)
	{
		AddSoftPath(Mapping.InputAction);
		AddSoftPath(Mapping.Icon);
	}
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
		|| !PandoraTreeInputAction.IsNull()
		|| !ScoreboardInputAction.IsNull()
		|| !ChatInputAction.IsNull()
		|| !ChatScrollInputAction.IsNull();

	if (!bHasNativeInputAction)
	{
		Context.AddWarning(NSLOCTEXT("ControllerInputDefinition", "NoInputActions", "ControllerInputDefinition has no input actions."));
	}

	TSet<FSoftObjectPath> IconMappedActions;
	for (int32 Index = 0; Index < InputActionIconMappings.Num(); ++Index)
	{
		const FInputActionIconMapping& Mapping = InputActionIconMappings[Index];
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
