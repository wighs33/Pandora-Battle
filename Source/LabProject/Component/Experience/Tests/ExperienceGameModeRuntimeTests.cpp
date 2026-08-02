#if WITH_DEV_AUTOMATION_TESTS

#include "Mode/ExperienceGameMode.h"

#include "Component/Experience/ExperienceGameplayLoadoutProvisioner.h"
#include "Component/Experience/ExperienceLobbyProfileProvisioner.h"
#include "Component/Experience/ExperienceMatchFlowComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Experience/ExperienceSpawnComponent.h"
#include "Component/Experience/ExperienceTrainingRoomProvisioner.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExperienceGameModeRuntimeCompositionTest,
	"LabProject.Experience.GameModeRuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FExperienceGameModeRuntimeCompositionTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);

	const AExperienceGameMode* GameMode =
		GetDefault<AExperienceGameMode>();
	TestNotNull(TEXT("Experience GameMode CDO"), GameMode);
	if (!GameMode)
	{
		return false;
	}

	const UExperienceMatchFlowComponent* MatchFlow =
		GameMode->GetMatchFlowComponent();
	const UExperienceSpawnComponent* Spawn =
		GameMode->GetSpawnComponent();
	const UExperiencePlayerProvisioningComponent* Provisioning =
		GameMode->GetPlayerProvisioningComponent();

	const auto TestProvisionerComposition =
		[this](
			const UExperiencePlayerProvisioningComponent* Coordinator,
			const FString& Context)
		{
			if (!Coordinator)
			{
				return;
			}

			const UExperienceLobbyProfileProvisioner* LobbyProvisioner =
				Coordinator->GetLobbyProfileProvisioner();
			const UExperienceGameplayLoadoutProvisioner*
				GameplayProvisioner =
					Coordinator->GetGameplayLoadoutProvisioner();
			const UExperienceTrainingRoomProvisioner*
				TrainingProvisioner =
					Coordinator->GetTrainingRoomProvisioner();

			TestTrue(
				*FString::Printf(
					TEXT("%s lobby-profile provisioner"),
					*Context),
				IsValid(LobbyProvisioner));
			TestTrue(
				*FString::Printf(
					TEXT("%s gameplay-loadout provisioner"),
					*Context),
				IsValid(GameplayProvisioner));
			TestTrue(
				*FString::Printf(
					TEXT("%s training-room provisioner"),
					*Context),
				IsValid(TrainingProvisioner));
			TestTrue(
				*FString::Printf(
					TEXT("%s provisioner ownership"),
					*Context),
				(!LobbyProvisioner
					|| LobbyProvisioner->GetOuter() == Coordinator)
					&& (!GameplayProvisioner
						|| GameplayProvisioner->GetOuter()
							== Coordinator)
					&& (!TrainingProvisioner
						|| TrainingProvisioner->GetOuter()
							== Coordinator));
		};

	TestNotNull(TEXT("Match-flow component"), MatchFlow);
	TestNotNull(TEXT("Spawn component"), Spawn);
	TestNotNull(TEXT("Player-provisioning component"), Provisioning);

	if (MatchFlow)
	{
		TestTrue(
			TEXT("Match-flow owner"),
			MatchFlow->GetOwner() == GameMode);
		TestEqual(
			TEXT("Default victory reward policy"),
			MatchFlow->CalculateVictoryGoldReward(2, 1, 1),
			153);
		TestEqual(
			TEXT("Victory reward never becomes negative"),
			MatchFlow->CalculateVictoryGoldReward(0, 99, 0),
			0);
	}

	if (Spawn)
	{
		TestTrue(
			TEXT("Spawn owner"),
			Spawn->GetOwner() == GameMode);
		TestEqual(
			TEXT("No pending respawns on CDO"),
			Spawn->GetPendingRespawnCount(),
			0);
	}

	if (Provisioning)
	{
		TestTrue(
			TEXT("Provisioning owner"),
			Provisioning->GetOwner() == GameMode);
		TestEqual(
			TEXT("No pending item grants on CDO"),
			Provisioning->GetPendingDefaultItemGrantCount(),
			0);
		TestProvisionerComposition(
			Provisioning,
			TEXT("Native GameMode CDO"));

		UExperiencePlayerProvisioningComponent* DuplicatedProvisioning =
			DuplicateObject<UExperiencePlayerProvisioningComponent>(
				Provisioning,
				GetTransientPackage());
		TestNotNull(
			TEXT("Duplicated player-provisioning component"),
			DuplicatedProvisioning);
		TestProvisionerComposition(
			DuplicatedProvisioning,
			TEXT("Duplicated provisioning component"));
		if (DuplicatedProvisioning)
		{
			TestTrue(
				TEXT("Duplicated lobby provisioner is instance-owned"),
				DuplicatedProvisioning->GetLobbyProfileProvisioner()
					!= Provisioning->GetLobbyProfileProvisioner());
			TestTrue(
				TEXT("Duplicated gameplay provisioner is instance-owned"),
				DuplicatedProvisioning->GetGameplayLoadoutProvisioner()
					!= Provisioning->GetGameplayLoadoutProvisioner());
			TestTrue(
				TEXT("Duplicated training provisioner is instance-owned"),
				DuplicatedProvisioning->GetTrainingRoomProvisioner()
					!= Provisioning->GetTrainingRoomProvisioner());
		}
	}

	UExperienceMatchFlowComponent* DetachedMatchFlow =
		NewObject<UExperienceMatchFlowComponent>(
			GetTransientPackage());
	TestNotNull(
		TEXT("Detached match-flow component"),
		DetachedMatchFlow);
	if (DetachedMatchFlow)
	{
		FExperienceMatchFlowSettings CustomMatchSettings;
		CustomMatchSettings.VictoryGoldPerKill = 7;
		CustomMatchSettings.VictoryGoldPenaltyPerDeath = 2;
		CustomMatchSettings.VictoryGoldPerWinningTeamMember = 5;
		DetachedMatchFlow->ApplySettings(CustomMatchSettings);
		TestEqual(
			TEXT("Match reward policy uses injected settings"),
			DetachedMatchFlow->CalculateVictoryGoldReward(2, 1, 3),
			27);
	}

	const FTrainingRoomItemStackGrant TrainingGrant;
	TestEqual(
		TEXT("Training grant keeps its serialized default quantity"),
		TrainingGrant.Quantity,
		100);

	const FGameplayItemStackGrant GameplayGrant;
	TestEqual(
		TEXT("Gameplay grant keeps its serialized default quantity"),
		GameplayGrant.Quantity,
		1);
	TestEqual(
		TEXT("Gameplay grant is unassigned by default"),
		GameplayGrant.QuickSlotIndex,
		INDEX_NONE);

	const UClass* BlueprintGameModeClass =
		LoadClass<AExperienceGameMode>(
			nullptr,
			TEXT("/Game/Mode/BP_GameMode.BP_GameMode_C"));
	TestNotNull(
		TEXT("Existing BP_GameMode class still loads"),
		BlueprintGameModeClass);
	const AExperienceGameMode* BlueprintGameMode =
		BlueprintGameModeClass
			? Cast<AExperienceGameMode>(
				BlueprintGameModeClass->GetDefaultObject())
			: nullptr;
	TestNotNull(
		TEXT("Existing BP_GameMode CDO"),
		BlueprintGameMode);
	if (BlueprintGameMode)
	{
		TestNotNull(
			TEXT("BP_GameMode inherited match-flow component"),
			BlueprintGameMode->GetMatchFlowComponent());
		TestNotNull(
			TEXT("BP_GameMode inherited spawn component"),
			BlueprintGameMode->GetSpawnComponent());
		TestNotNull(
			TEXT("BP_GameMode inherited provisioning component"),
			BlueprintGameMode->GetPlayerProvisioningComponent());
		TestProvisionerComposition(
			BlueprintGameMode->GetPlayerProvisioningComponent(),
			TEXT("BP_GameMode CDO"));
	}

	return !HasAnyErrors();
}

#endif
