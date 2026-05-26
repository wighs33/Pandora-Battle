#include "PlayerComponent/ControllerInputDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerInputDefinition)

FPrimaryAssetId UControllerInputDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ControllerInputDefinition"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UControllerInputDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (InputMapping.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("ControllerInputDefinition", "MissingInputMapping", "InputMapping is required."));
	}

	const bool bHasNativeInputAction =
		!MoveInputAction.IsNull()
		|| !LookInputAction.IsNull()
		|| !JumpInputAction.IsNull()
		|| !CrouchInputAction.IsNull()
		|| !InteractInputAction.IsNull()
		|| !AttackInputAction.IsNull()
		|| !AimInputAction.IsNull()
		|| !Skill1InputAction.IsNull()
		|| !Skill2InputAction.IsNull()
		|| !Skill3InputAction.IsNull()
		|| !Skill4InputAction.IsNull()
		|| !TargetConfirmInputAction.IsNull()
		|| !OpenInfoUiInputAction.IsNull()
		|| !SelectPandoraInputAction.IsNull()
		|| !PandoraTreeInputAction.IsNull();

	if (!bHasNativeInputAction)
	{
		Context.AddWarning(NSLOCTEXT("ControllerInputDefinition", "NoInputActions", "ControllerInputDefinition has no input actions."));
	}

	return Result;
}
#endif
