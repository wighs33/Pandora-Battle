#include "Definition/Player/PlayerControllerDefinition.h"

#include "Definition/Player/ControllerInputDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerControllerDefinition)

namespace
{
	constexpr const TCHAR* DefaultPlayerControllerDefinitionPath =
		TEXT("/Game/Data/DA_PlayerController.DA_PlayerController");
}

UPlayerControllerDefinition::UPlayerControllerDefinition()
{
	Input.DefaultInputDefinition = TSoftObjectPtr<UControllerInputDefinition>(
		UControllerInputDefinition::GetDefaultInputDefinitionPath());
}

FPrimaryAssetId UPlayerControllerDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PlayerControllerDefinition"), GetFName());
}

FSoftObjectPath UPlayerControllerDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(DefaultPlayerControllerDefinitionPath);
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

	if (Input.DefaultInputDefinition.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"MissingInputDefinition",
			"Input DefaultInputDefinition is required."));
	}

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

	if (!FMath::IsFinite(ProfileSync.LocalShopSaveSyncInterval)
		|| ProfileSync.LocalShopSaveSyncInterval < 0.01f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidProfileSyncInterval",
			"ProfileSync LocalShopSaveSyncInterval must be finite and at least 0.01 seconds."));
	}

	if (ProfileSync.LocalShopSaveSyncMaxAttempts < 1
		|| ProfileSync.MaxClientSyncedSkinNameCount < 1)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidProfileSyncLimits",
			"ProfileSync attempt and client skin-name limits must be at least one."));
	}

	if (!FMath::IsFinite(ProfileSync.RemoteSkinSyncMinInterval)
		|| ProfileSync.RemoteSkinSyncMinInterval < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidRemoteSkinSyncInterval",
			"ProfileSync RemoteSkinSyncMinInterval must be finite and non-negative."));
	}

	if (DebugGrant.SoulDustGrantAmount < 0
		|| !FMath::IsFinite(DebugGrant.StatusPointGrantAmount)
		|| DebugGrant.StatusPointGrantAmount < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerControllerDefinition",
			"InvalidDebugGrantAmounts",
			"DebugGrant amounts must be finite and non-negative."));
	}

	return Result;
}
#endif
