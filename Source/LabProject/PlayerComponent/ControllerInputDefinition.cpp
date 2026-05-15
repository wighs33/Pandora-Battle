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

	TSet<FGameplayTag> AbilityInputTags;
	for (int32 EntryIndex = 0; EntryIndex < AbilityInputActions.Num(); ++EntryIndex)
	{
		const FPdAbilityInputAction& Entry = AbilityInputActions[EntryIndex];
		if (!Entry.IsValid())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ControllerInputDefinition", "InvalidAbilityInputAction", "AbilityInputActions entry {0} requires both InputTag and InputAction."),
				FText::AsNumber(EntryIndex)));
			continue;
		}

		if (AbilityInputTags.Contains(Entry.InputTag))
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(
				NSLOCTEXT("ControllerInputDefinition", "DuplicateAbilityInputTag", "AbilityInputActions entry {0} duplicates InputTag '{1}'."),
				FText::AsNumber(EntryIndex),
				FText::FromString(Entry.InputTag.ToString())));
			continue;
		}

		AbilityInputTags.Add(Entry.InputTag);
	}

	const bool bHasNativeInputAction =
		!MoveInputAction.IsNull()
		|| !LookInputAction.IsNull()
		|| !JumpInputAction.IsNull()
		|| !CrouchInputAction.IsNull()
		|| !InteractInputAction.IsNull()
		|| !AttackInputAction.IsNull()
		|| !AimInputAction.IsNull()
		|| !OpenInfoUiInputAction.IsNull()
		|| !SelectPandoraInputAction.IsNull();

	if (!bHasNativeInputAction && AbilityInputActions.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("ControllerInputDefinition", "NoInputActions", "ControllerInputDefinition has no input actions."));
	}

	return Result;
}
#endif
