#include "Definition/Lobby/LobbyModeDefinition.h"

#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyModeDefinition)

namespace
{
	constexpr const TCHAR* DefaultLobbyModeDefinitionPath =
		TEXT("/Game/Data/DA_LobbyMode.DA_LobbyMode");
	constexpr const TCHAR* DefaultLobbyPreviewDefinitionPath =
		TEXT("/Game/Data/DA_LobbyPreview.DA_LobbyPreview");
}

ULobbyModeDefinition::ULobbyModeDefinition()
{
	Content.MatchRuleDefinition =
		TSoftObjectPtr<UMatchRuleDefinition>(
			UMatchRuleDefinition::GetDefaultDefinitionPath());
	Content.LobbyPreviewDefinition =
		TSoftObjectPtr<ULobbyPreviewDefinition>(
			FSoftObjectPath(DefaultLobbyPreviewDefinitionPath));
}

FPrimaryAssetId ULobbyModeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("LobbyModeDefinition"), GetFName());
}

FSoftObjectPath ULobbyModeDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(DefaultLobbyModeDefinitionPath);
}

#if WITH_EDITOR
EDataValidationResult ULobbyModeDefinition::IsDataValid(
	FDataValidationContext& Context) const
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
	auto ValidateNonNegativeFinite = [&MarkInvalid](
		const float Value,
		const FText& FieldName)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			MarkInvalid(FText::Format(
				NSLOCTEXT(
					"LobbyModeDefinition",
					"InvalidNonNegativeTime",
					"{0} must be a non-negative finite value."),
				FieldName));
		}
	};

	ValidateNonNegativeFinite(
		Flow.FullLobbyAutoStartDelay,
		NSLOCTEXT(
			"LobbyModeDefinition",
			"FullLobbyAutoStartDelay",
			"Flow.FullLobbyAutoStartDelay"));
	ValidateNonNegativeFinite(
		Flow.KickDisconnectDelay,
		NSLOCTEXT(
			"LobbyModeDefinition",
			"KickDisconnectDelay",
			"Flow.KickDisconnectDelay"));
	ValidateNonNegativeFinite(
		Flow.LobbyRespawnDelay,
		NSLOCTEXT(
			"LobbyModeDefinition",
			"LobbyRespawnDelay",
			"Flow.LobbyRespawnDelay"));

	if (DedicatedSession.bAutoCreateDedicatedServerSession
		&& DedicatedSession.DedicatedServerRoomName.TrimStartAndEnd().IsEmpty())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingDedicatedServerRoomName",
			"DedicatedSession.DedicatedServerRoomName is required when automatic dedicated-session creation is enabled."));
	}

	if (Travel.RoomMap.IsNull()
		&& Travel.RoomTravelMapName.TrimStartAndEnd().IsEmpty())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingRoomTravelDestination",
			"Travel must provide RoomMap or RoomTravelMapName."));
	}

	if (Content.MatchRuleDefinition.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingMatchRuleDefinition",
			"Content.MatchRuleDefinition is required."));
	}
	else if (!Content.MatchRuleDefinition.LoadSynchronous())
	{
		MarkInvalid(FText::Format(
			NSLOCTEXT(
				"LobbyModeDefinition",
				"InvalidMatchRuleDefinition",
				"Content.MatchRuleDefinition could not be loaded: {0}"),
			FText::FromString(Content.MatchRuleDefinition.ToString())));
	}

	if (Content.LobbyPreviewDefinition.IsNull())
	{
		MarkInvalid(NSLOCTEXT(
			"LobbyModeDefinition",
			"MissingLobbyPreviewDefinition",
			"Content.LobbyPreviewDefinition is required."));
	}
	else if (!Content.LobbyPreviewDefinition.LoadSynchronous())
	{
		MarkInvalid(FText::Format(
			NSLOCTEXT(
				"LobbyModeDefinition",
				"InvalidLobbyPreviewDefinition",
				"Content.LobbyPreviewDefinition could not be loaded: {0}"),
			FText::FromString(Content.LobbyPreviewDefinition.ToString())));
	}

	return Result;
}
#endif
