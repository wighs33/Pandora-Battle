#if WITH_DEV_AUTOMATION_TESTS

#include "Mode/PdGameInstance.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Mode/PdGameInstanceRuntimeSubsystem.h"
#include "SavedGameData/PdSaveGame.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Settings/BgmSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdProfileSaveObfuscationTest,
	"LabProject.GameInstance.ProfileSaveObfuscation",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdProfileSaveObfuscationTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const FString PlayerId(TEXT("76561198000000000"));
	UPdSaveGame* SourceProfile = NewObject<UPdSaveGame>();
	SourceProfile->SaveSchemaVersion = PdSaveGameSchema::Current;
	SourceProfile->SaveId = FGuid::NewGuid();
	SourceProfile->SaveRevision = 42;
	SourceProfile->Gold = 736281;
	SourceProfile->MatchPlayedCount = 17;
	SourceProfile->PlayerSkinData.GrantedSkinsById.Add(
		FPrimaryAssetId(TEXT("SkinDefinition"), TEXT("DA_TestSecretSkin")),
		1);

	UPdProfileSaveEnvelope* Envelope =
		UPdProfileSaveEnvelope::CreateFromProfile(
			SourceProfile,
			PlayerId);
	TestNotNull(TEXT("Profile creates an obfuscated disk envelope"), Envelope);
	if (!Envelope)
	{
		return false;
	}

	TestEqual(
		TEXT("Envelope uses the current storage format"),
		Envelope->GetStorageFormatVersion(),
		PdProfileSaveStorage::Current);
	TestTrue(
		TEXT("Envelope contains an obfuscated payload"),
		Envelope->GetObfuscatedPayloadSize() > 0);

	TArray<uint8> StoredEnvelopeBytes;
	TestTrue(
		TEXT("Envelope can be serialized by the SaveGame system"),
		UGameplayStatics::SaveGameToMemory(
			Envelope,
			StoredEnvelopeBytes));

	UPdProfileSaveEnvelope* ReloadedEnvelope =
		Cast<UPdProfileSaveEnvelope>(
			UGameplayStatics::LoadGameFromMemory(
				StoredEnvelopeBytes));
	TestNotNull(
		TEXT("Serialized envelope can be loaded"),
		ReloadedEnvelope);
	if (!ReloadedEnvelope)
	{
		return false;
	}

	UPdSaveGame* DecodedProfile =
		ReloadedEnvelope->DecodeProfile(PlayerId);
	TestNotNull(
		TEXT("Correct profile identity decodes the payload"),
		DecodedProfile);
	if (DecodedProfile)
	{
		TestEqual(
			TEXT("Gold survives obfuscation round trip"),
			DecodedProfile->Gold,
			SourceProfile->Gold);
		TestEqual(
			TEXT("Save revision survives obfuscation round trip"),
			DecodedProfile->SaveRevision,
			SourceProfile->SaveRevision);
		TestTrue(
			TEXT("Cosmetic unlocks survive obfuscation round trip"),
			DecodedProfile->PlayerSkinData.GrantedSkinsById.Contains(
				FPrimaryAssetId(
					TEXT("SkinDefinition"),
					TEXT("DA_TestSecretSkin"))));
	}

	TestNull(
		TEXT("A different profile identity cannot decode the payload"),
		ReloadedEnvelope->DecodeProfile(TEXT("DifferentPlayer")));

	ReloadedEnvelope->CorruptPayloadForTest();
	TestNull(
		TEXT("Payload corruption is rejected before profile load"),
		ReloadedEnvelope->DecodeProfile(PlayerId));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdGameInstanceRuntimeCompositionTest,
	"LabProject.GameInstance.RuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdGameInstanceRuntimeCompositionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const UPdGameInstanceDefinition* Definition =
		LoadObject<UPdGameInstanceDefinition>(
			nullptr,
			TEXT("/Game/Data/DA_GameInstance.DA_GameInstance"));
	TestNotNull(TEXT("Default GameInstance definition asset"), Definition);
	if (Definition)
	{
		TestEqual(
			TEXT("Definition primary asset type"),
			Definition->GetPrimaryAssetId().PrimaryAssetType,
			FPrimaryAssetType(TEXT("GameInstanceDefinition")));
		TestTrue(
			TEXT("Definition uses PdSaveGame"),
			Definition->GetProfilePersistenceSettings().SaveGameClass
				== UPdSaveGame::StaticClass());
		TestEqual(
			TEXT("Default profile save retry count"),
			Definition->GetProfilePersistenceSettings().MaxSaveRetryAttempts,
			3);
	}

	UPdGameInstance* GameInstance = NewObject<UPdGameInstance>(GEngine);
	TestNotNull(TEXT("PdGameInstance test instance"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	GameInstance->AddToRoot();
	GameInstance->Init();

	UPdGameInstanceRuntimeSubsystem* RuntimeSubsystem =
		GameInstance->GetSubsystem<UPdGameInstanceRuntimeSubsystem>();
	UPlayerProfileSubsystem* ProfileSubsystem =
		GameInstance->GetSubsystem<UPlayerProfileSubsystem>();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance->GetSubsystem<UContentDataSubsystem>();
	ULobbyRuntimeSubsystem* LobbySubsystem =
		GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>();
	UBgmSubsystem* BgmSubsystem =
		GameInstance->GetSubsystem<UBgmSubsystem>();

	TestNotNull(TEXT("Runtime composition subsystem"), RuntimeSubsystem);
	TestNotNull(TEXT("Player-profile subsystem"), ProfileSubsystem);
	TestNotNull(TEXT("Content-data subsystem"), ContentSubsystem);
	TestNotNull(TEXT("Lobby-runtime subsystem"), LobbySubsystem);
	TestNotNull(TEXT("BGM subsystem"), BgmSubsystem);

	if (RuntimeSubsystem)
	{
		TestTrue(
			TEXT("Runtime subsystem resolves the default definition"),
			RuntimeSubsystem->GetGameInstanceDefinition() == Definition);
	}
	if (Definition && ProfileSubsystem)
	{
		const FPdPlayerProfilePersistenceSettings& AppliedSettings =
			ProfileSubsystem->GetSettings();
		TestEqual(
			TEXT("Profile save debounce was injected"),
			AppliedSettings.SaveDebounceSeconds,
			Definition->GetProfilePersistenceSettings().SaveDebounceSeconds);
		TestEqual(
			TEXT("Profile save retry policy was injected"),
			AppliedSettings.MaxSaveRetryAttempts,
			Definition->GetProfilePersistenceSettings().MaxSaveRetryAttempts);
	}

	GameInstance->SetLobbyGameConfig(
		TEXT("RuntimeTestMap"),
		TEXT("/Game/Map/RuntimeTest"),
		4,
		2);
	TestEqual(
		TEXT("Lobby facade delegates selected map"),
		GameInstance->GetLobbySelectedMapKey(),
		FName(TEXT("RuntimeTestMap")));
	TestEqual(
		TEXT("Lobby facade delegates max players"),
		GameInstance->GetLobbyMaxPlayerCount(),
		4);

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();

	return !HasAnyErrors();
}

#endif
