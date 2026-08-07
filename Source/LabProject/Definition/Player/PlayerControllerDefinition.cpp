#include "Definition/Player/PlayerControllerDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerControllerDefinition)

FPrimaryAssetId UPlayerControllerDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PlayerControllerDefinition"), GetFName());
}

FSoftObjectPath UPlayerControllerDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.PlayerController.ToSoftObjectPath();
}

#if WITH_EDITOR
EDataValidationResult UPlayerControllerDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	auto MarkInvalid = [&Context, &Result](const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	};

	if (!FMath::IsFinite(Presentation.TravelLoadingReadyCheckInterval)
		|| Presentation.TravelLoadingReadyCheckInterval < 0.01f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidTravelLoadingInterval",
			"Presentation TravelLoadingReadyCheckInterval must be finite and at least 0.01 seconds."));
	}

	if (Presentation.TravelLoadingReadyCheckMaxAttempts < 0)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidTravelLoadingAttempts",
			"Presentation TravelLoadingReadyCheckMaxAttempts must be non-negative."));
	}

	if (!FMath::IsFinite(Presentation.RespawnStateResetRetryDelay)
		|| Presentation.RespawnStateResetRetryDelay < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidRespawnRetryDelay",
			"Presentation RespawnStateResetRetryDelay must be finite and non-negative."));
	}

	if (!FMath::IsFinite(Presentation.HealthBarVisibilityUpdateInterval)
		|| Presentation.HealthBarVisibilityUpdateInterval < 0.01f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidHealthBarUpdateInterval",
			"Presentation HealthBarVisibilityUpdateInterval must be finite and at least 0.01 seconds."));
	}

	if (!FMath::IsFinite(Presentation.HealthBarVisibilityDistance)
		|| Presentation.HealthBarVisibilityDistance < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidHealthBarDistance",
			"Presentation HealthBarVisibilityDistance must be finite and non-negative."));
	}

	return Result;
}
#endif
