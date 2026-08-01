#include "Definition/Player/CharacterActionDefinition.h"

#include "InputAction.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterActionDefinition)

namespace
{
	constexpr const TCHAR* DefaultCharacterActionDefinitionPathName =
		TEXT("/Game/Data/DA_CharacterAction.DA_CharacterAction");

	TSoftObjectPtr<UInputAction> MakeCharacterActionInputActionReference(const TCHAR* Path)
	{
		return TSoftObjectPtr<UInputAction>(FSoftObjectPath(Path));
	}

#if WITH_EDITOR
	void MarkCharacterActionInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	void ValidateCharacterActionConfig(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FCharacterActionConfig& Config,
		const FText& ActionName)
	{
		if (Config.DisplayName.IsEmpty())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("CharacterActionDefinition", "MissingDisplayName", "{0} DisplayName is empty."),
				ActionName));
		}

		if (Config.CooldownDuration < 0.0)
		{
			MarkCharacterActionInvalid(Context, Result, FText::Format(
				NSLOCTEXT("CharacterActionDefinition", "InvalidCooldownDuration", "{0} CooldownDuration cannot be negative."),
				ActionName));
		}

		if (Config.InputAction.IsNull())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("CharacterActionDefinition", "MissingInputAction", "{0} InputAction is not set."),
				ActionName));
		}
		else if (!Config.InputAction.LoadSynchronous())
		{
			MarkCharacterActionInvalid(Context, Result, FText::Format(
				NSLOCTEXT("CharacterActionDefinition", "InvalidInputAction", "{0} InputAction could not be loaded: {1}"),
				ActionName,
				FText::FromString(Config.InputAction.ToString())));
		}

		if (!Config.IconResource.IsNull())
		{
			UObject* IconObject = Config.IconResource.LoadSynchronous();
			if (!IconObject)
			{
				MarkCharacterActionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("CharacterActionDefinition", "InvalidIconResource", "{0} IconResource could not be loaded: {1}"),
					ActionName,
					FText::FromString(Config.IconResource.ToString())));
			}
			else if (!IconObject->IsA<UTexture2D>() && !IconObject->IsA<UMaterialInterface>())
			{
				MarkCharacterActionInvalid(Context, Result, FText::Format(
					NSLOCTEXT("CharacterActionDefinition", "UnsupportedIconResource", "{0} IconResource must be a Texture2D or MaterialInterface."),
					ActionName));
			}
		}
	}

	void ValidateGrappleAimCameraSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FWeaponAimCameraSettings& CameraSettings)
	{
		if (!FMath::IsFinite(CameraSettings.TargetFOV) || CameraSettings.TargetFOV <= 0.0f)
		{
			MarkCharacterActionInvalid(Context, Result, NSLOCTEXT(
				"CharacterActionDefinition",
				"InvalidGrappleAimTargetFOV",
				"Grapple Aim Camera TargetFOV must be a positive finite value."));
		}
		else if (CameraSettings.TargetFOV < 5.0f || CameraSettings.TargetFOV > 170.0f)
		{
			Context.AddWarning(NSLOCTEXT(
				"CharacterActionDefinition",
				"OutOfRangeGrappleAimTargetFOV",
				"Grapple Aim Camera TargetFOV will be clamped to 5..170 at runtime."));
		}

		if (CameraSettings.TargetBoomSocketOffset.ContainsNaN())
		{
			MarkCharacterActionInvalid(Context, Result, NSLOCTEXT(
				"CharacterActionDefinition",
				"InvalidGrappleAimBoomOffset",
				"Grapple Aim Camera TargetBoomSocketOffset contains NaN."));
		}

		if (CameraSettings.TargetCameraRotation.ContainsNaN())
		{
			MarkCharacterActionInvalid(Context, Result, NSLOCTEXT(
				"CharacterActionDefinition",
				"InvalidGrappleAimCameraRotation",
				"Grapple Aim Camera TargetCameraRotation contains NaN."));
		}

		if (!FMath::IsFinite(CameraSettings.InterpSpeed) || CameraSettings.InterpSpeed < 0.0f)
		{
			MarkCharacterActionInvalid(Context, Result, NSLOCTEXT(
				"CharacterActionDefinition",
				"InvalidGrappleAimInterpSpeed",
				"Grapple Aim Camera InterpSpeed must be a non-negative finite value."));
		}
		else if (CameraSettings.InterpSpeed > 100.0f)
		{
			Context.AddWarning(NSLOCTEXT(
				"CharacterActionDefinition",
				"OutOfRangeGrappleAimInterpSpeed",
				"Grapple Aim Camera InterpSpeed will be clamped to 100 at runtime."));
		}
	}
#endif
}

UObject* FCharacterActionConfig::LoadIconResource() const
{
	return IconResource.Get();
}

UInputAction* FCharacterActionConfig::LoadInputAction() const
{
	return InputAction.Get();
}

UObject* FCharacterActionConfig::GetLoadedIconResource() const
{
	return IconResource.Get();
}

UInputAction* FCharacterActionConfig::GetLoadedInputAction() const
{
	return InputAction.Get();
}

void FCharacterActionConfig::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	if (!IconResource.IsNull())
	{
		OutAssetPaths.Add(IconResource.ToSoftObjectPath());
	}
	if (!InputAction.IsNull())
	{
		OutAssetPaths.Add(InputAction.ToSoftObjectPath());
	}
}

UCharacterActionDefinition::UCharacterActionDefinition()
{
	PandoraWeaponSwap.DisplayName = NSLOCTEXT("CharacterActionDefinition", "PandoraWeaponSwapDisplayName", "Swap");
	PandoraWeaponSwap.CooldownDuration = 3.0;
	PandoraWeaponSwap.InputAction = MakeCharacterActionInputActionReference(TEXT("/Game/Input/Action/IA_SelectPandora.IA_SelectPandora"));

	GrappleHook.DisplayName = NSLOCTEXT("CharacterActionDefinition", "GrappleHookDisplayName", "Hook");
	GrappleHook.InputAction = MakeCharacterActionInputActionReference(TEXT("/Game/Input/Action/IA_Grapple.IA_Grapple"));
}

FPrimaryAssetId UCharacterActionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("CharacterActionDefinition"), GetFName());
}

FSoftObjectPath UCharacterActionDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(DefaultCharacterActionDefinitionPathName);
}

const FCharacterActionConfig& UCharacterActionDefinition::FindActionConfig(const ECharacterActionType ActionType) const
{
	switch (ActionType)
	{
	case ECharacterActionType::GrappleHook:
		return GrappleHook;
	case ECharacterActionType::PandoraWeaponSwap:
	default:
		return PandoraWeaponSwap;
	}
}

FText UCharacterActionDefinition::GetDisplayName(const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).DisplayName;
}

UObject* UCharacterActionDefinition::GetIconResource(const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).LoadIconResource();
}

UObject* UCharacterActionDefinition::GetLoadedIconResource(
	const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).GetLoadedIconResource();
}

double UCharacterActionDefinition::GetCooldownDuration(const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).CooldownDuration;
}

FWeaponAimCameraSettings UCharacterActionDefinition::GetAimCameraSettings(const ECharacterActionType ActionType) const
{
	return ActionType == ECharacterActionType::GrappleHook
		? GrappleAimCameraSettings
		: FWeaponAimCameraSettings();
}

UInputAction* UCharacterActionDefinition::LoadInputAction(const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).LoadInputAction();
}

UInputAction* UCharacterActionDefinition::GetLoadedInputAction(
	const ECharacterActionType ActionType) const
{
	return FindActionConfig(ActionType).GetLoadedInputAction();
}

void UCharacterActionDefinition::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	PandoraWeaponSwap.GetRuntimePreloadAssetPaths(OutAssetPaths);
	GrappleHook.GetRuntimePreloadAssetPaths(OutAssetPaths);
}

#if WITH_EDITOR
EDataValidationResult UCharacterActionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateCharacterActionConfig(
		Context,
		Result,
		PandoraWeaponSwap,
		NSLOCTEXT("CharacterActionDefinition", "PandoraWeaponSwap", "Pandora / Weapon Swap"));
	ValidateCharacterActionConfig(
		Context,
		Result,
		GrappleHook,
		NSLOCTEXT("CharacterActionDefinition", "GrappleHook", "Grapple Hook"));

	if (!PandoraWeaponSwap.InputAction.IsNull()
		&& !GrappleHook.InputAction.IsNull()
		&& PandoraWeaponSwap.InputAction.ToSoftObjectPath() == GrappleHook.InputAction.ToSoftObjectPath())
	{
		Context.AddWarning(NSLOCTEXT(
			"CharacterActionDefinition",
			"DuplicateActionInput",
			"Pandora / Weapon Swap and Grapple Hook use the same InputAction."));
	}

	ValidateGrappleAimCameraSettings(Context, Result, GrappleAimCameraSettings);

	return Result;
}
#endif
