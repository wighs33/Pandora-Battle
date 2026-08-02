#if WITH_DEV_AUTOMATION_TESTS

#include "Lobby/Contents/LobbyGameMode.h"

#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Lobby/LobbyExperienceComponent.h"
#include "Component/Lobby/LobbyPlayerCoordinatorComponent.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Lobby/LobbyRespawnComponent.h"
#include "Definition/Lobby/LobbyModeDefinition.h"
#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "GameFramework/HUD.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLobbyRuntimeCompositionTest,
	"LabProject.Lobby.RuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FLobbyRuntimeCompositionTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);

	const ULobbyModeDefinition* Definition =
		LoadObject<ULobbyModeDefinition>(
			nullptr,
			TEXT(
				"/Game/Data/DA_LobbyMode.DA_LobbyMode"));
	TestNotNull(TEXT("Default lobby-mode definition"), Definition);
	if (Definition)
	{
		TestEqual(
			TEXT("Definition primary asset type"),
			Definition->GetPrimaryAssetId().PrimaryAssetType,
			FPrimaryAssetType(
				TEXT("LobbyModeDefinition")));
		TestEqual(
			TEXT("Lobby respawn delay"),
			Definition->GetFlowSettings()
				.LobbyRespawnDelay,
			3.0f);
		TestEqual(
			TEXT("Kick disconnect delay"),
			Definition->GetFlowSettings()
				.KickDisconnectDelay,
			2.0f);
		TestEqual(
			TEXT("Room travel fallback"),
			Definition->GetTravelSettings()
				.RoomTravelMapName,
			FString(TEXT("/Game/Map/LV_Room")));
		TestNotNull(
			TEXT("Definition resolves match rules"),
			Definition->GetContentSettings()
				.MatchRuleDefinition.LoadSynchronous());
		if (const UMatchRuleDefinition* MatchRules =
			Definition->GetContentSettings().MatchRuleDefinition.Get())
		{
			TestTrue(
				TEXT("Lobby exposes at least one map option"),
				!MatchRules->LobbyMapOptions.IsEmpty());
			for (const FLobbyMatchMapOption& MapOption : MatchRules->LobbyMapOptions)
			{
				TestNotNull(
					*FString::Printf(
						TEXT("Lobby map '%s' thumbnail"),
						*MapOption.MapKey.ToString()),
					MapOption.Thumbnail.Get());
			}
		}
		TestNotNull(
			TEXT("Definition resolves preview policy"),
			Definition->GetContentSettings()
				.LobbyPreviewDefinition.LoadSynchronous());
	}

	const ALobbyGameMode* GameMode =
		GetDefault<ALobbyGameMode>();
	TestNotNull(TEXT("Lobby GameMode CDO"), GameMode);
	if (!GameMode)
	{
		return false;
	}

	ULobbyConfigurationComponent* Configuration =
		GameMode->GetLobbyConfigurationComponent();
	const ULobbyExperienceComponent* Experience =
		GameMode->GetLobbyExperienceComponent();
	const ULobbyPlayerCoordinatorComponent* PlayerCoordinator =
		GameMode->GetLobbyPlayerCoordinatorComponent();
	const ULobbyRespawnComponent* Respawn =
		GameMode->GetLobbyRespawnComponent();

	TestNotNull(TEXT("Configuration component"), Configuration);
	TestNotNull(TEXT("Experience component"), Experience);
	TestNotNull(TEXT("Player coordinator component"), PlayerCoordinator);
	TestNotNull(TEXT("Respawn component"), Respawn);
	TestNotNull(TEXT("Existing match coordinator"), GameMode->GetMatchCoordinator());
	TestNotNull(TEXT("Existing preview grant service"), GameMode->GetPreviewGrantService());
	TestNotNull(TEXT("Existing travel coordinator"), GameMode->GetTravelCoordinator());
	if (Configuration)
	{
		TestTrue(
			TEXT("Configuration owner"),
			Configuration->GetOwner() == GameMode);
		TestTrue(
			TEXT("Configuration resolves the default definition"),
			Configuration->GetLobbyModeDefinition()
				== Definition);
	}
	if (Experience)
	{
		TestTrue(
			TEXT("Experience owner"),
			Experience->GetOwner() == GameMode);
	}
	if (PlayerCoordinator)
	{
		TestTrue(
			TEXT("Player coordinator owner"),
			PlayerCoordinator->GetOwner() == GameMode);
		TestEqual(
			TEXT("No pending kick timers on CDO"),
			PlayerCoordinator->GetPendingKickCount(),
			0);
	}
	if (Respawn)
	{
		TestTrue(
			TEXT("Respawn owner"),
			Respawn->GetOwner() == GameMode);
		TestEqual(
			TEXT("No pending respawn timers on CDO"),
			Respawn->GetPendingRespawnCount(),
			0);
	}

	const ALobbyPlayerState* PlayerState =
		GetDefault<ALobbyPlayerState>();
	TestNotNull(TEXT("Lobby PlayerState CDO"), PlayerState);
	if (PlayerState)
	{
		const ULobbyPlayerStateComponent* LobbyState =
			PlayerState->GetLobbyPlayerStateComponent();
		TestNotNull(TEXT("Replicated lobby-state component"), LobbyState);
		if (LobbyState)
		{
			TestTrue(
				TEXT("Lobby-state owner"),
				LobbyState->GetOwner() == PlayerState);
			TestFalse(TEXT("Ready defaults false"), LobbyState->IsReady());
			TestFalse(TEXT("Leaving defaults false"), LobbyState->IsLeavingLobby());
			TestTrue(TEXT("Nickname defaults empty"), LobbyState->GetNickname().IsEmpty());
		}
	}

	const ULobbyPreviewDefinition* PreviewDefinition =
		LoadObject<ULobbyPreviewDefinition>(
			nullptr,
			TEXT(
				"/Game/Data/DA_LobbyPreview.DA_LobbyPreview"));
	TestNotNull(TEXT("Lobby preview definition"), PreviewDefinition);
	if (PreviewDefinition)
	{
		const FLobbyPreviewAttributeSettings& Attributes =
			PreviewDefinition->GetPreviewAttributeSettings();
		TestEqual(TEXT("Preview max health"), Attributes.MaxHealth, 100.0f);
		TestEqual(TEXT("Preview health"), Attributes.Health, 100.0f);
		TestEqual(TEXT("Preview max mana"), Attributes.MaxMana, 100.0f);
		TestEqual(TEXT("Preview max stamina"), Attributes.MaxStamina, 100.0f);
	}

	const UClass* BlueprintGameModeClass =
		LoadClass<ALobbyGameMode>(
			nullptr,
			TEXT(
				"/Game/Mode/BP_LobbyGameMode.BP_LobbyGameMode_C"));
	TestNotNull(
		TEXT("Existing BP_LobbyGameMode class still loads"),
		BlueprintGameModeClass);
	const ALobbyGameMode* BlueprintGameMode =
		BlueprintGameModeClass
			? Cast<ALobbyGameMode>(
				BlueprintGameModeClass->GetDefaultObject())
			: nullptr;
	TestNotNull(
		TEXT("Existing BP_LobbyGameMode CDO"),
		BlueprintGameMode);
	if (BlueprintGameMode)
	{
		TestNotNull(
			TEXT("BP inherited configuration component"),
			BlueprintGameMode
				->GetLobbyConfigurationComponent());
		TestNotNull(
			TEXT("BP inherited experience component"),
			BlueprintGameMode
				->GetLobbyExperienceComponent());
		TestNotNull(
			TEXT("BP inherited player coordinator"),
			BlueprintGameMode
				->GetLobbyPlayerCoordinatorComponent());
		TestNotNull(
			TEXT("BP inherited respawn component"),
			BlueprintGameMode
				->GetLobbyRespawnComponent());
		TestEqual(
			TEXT("BP keeps its configured HUD class"),
			BlueprintGameMode->HUDClass
				? BlueprintGameMode->HUDClass
					->GetPathName()
				: FString(),
			FString(
				TEXT(
					"/Game/Mode/BP_LobbyHUD.BP_LobbyHUD_C")));
		TestEqual(
			TEXT("BP keeps its configured pawn class"),
			BlueprintGameMode->DefaultPawnClass
				? BlueprintGameMode->DefaultPawnClass
					->GetPathName()
				: FString(),
			FString(
				TEXT(
					"/Game/Actor/Stickman/BP_Player.BP_Player_C")));
	}

	return !HasAnyErrors();
}

#endif
