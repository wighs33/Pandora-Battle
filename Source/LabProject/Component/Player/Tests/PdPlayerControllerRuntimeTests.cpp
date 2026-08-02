#if WITH_DEV_AUTOMATION_TESTS

#include "Mode/PdPlayerController.h"

#include "Component/Player/ControllerDebugGrantComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Component/Player/ControllerProfileSyncComponent.h"
#include "Component/Player/ControllerSessionComponent.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdPlayerControllerRuntimeCompositionTest,
	"LabProject.Player.ControllerRuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdPlayerControllerRuntimeCompositionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const APdPlayerController* Controller = GetDefault<APdPlayerController>();
	TestNotNull(TEXT("PdPlayerController CDO"), Controller);
	if (!Controller)
	{
		return false;
	}

	const UControllerPresentationComponent* Presentation =
		Controller->GetControllerPresentationComponent();
	const UControllerProfileSyncComponent* ProfileSync =
		Controller->GetControllerProfileSyncComponent();
	const UControllerSessionComponent* Session =
		Controller->GetControllerSessionComponent();
	const UControllerDebugGrantComponent* DebugGrant =
		Controller->GetControllerDebugGrantComponent();

	TestNotNull(TEXT("Presentation component"), Presentation);
	TestNotNull(TEXT("Profile-sync component"), ProfileSync);
	TestNotNull(TEXT("Session component"), Session);
	TestNotNull(TEXT("Debug-grant component"), DebugGrant);
	if (Presentation)
	{
		TestTrue(TEXT("Presentation owner"), Presentation->GetOwner() == Controller);
	}
	if (ProfileSync)
	{
		TestTrue(TEXT("Profile-sync owner"), ProfileSync->GetOwner() == Controller);
	}
	if (Session)
	{
		TestTrue(TEXT("Session owner"), Session->GetOwner() == Controller);
	}
	if (DebugGrant)
	{
		TestTrue(TEXT("Debug-grant owner"), DebugGrant->GetOwner() == Controller);
	}

	const UPlayerControllerDefinition* Definition =
		LoadObject<UPlayerControllerDefinition>(
			nullptr,
			TEXT("/Game/Data/DA_PlayerController.DA_PlayerController"));
	TestNotNull(TEXT("Default player-controller definition asset"), Definition);
	if (Definition)
	{
		TestEqual(
			TEXT("Definition primary asset type"),
			Definition->GetPrimaryAssetId().PrimaryAssetType,
			FPrimaryAssetType(TEXT("PlayerControllerDefinition")));
		TestEqual(
			TEXT("Travel readiness max attempts"),
			Definition->GetPresentationSettings().TravelLoadingReadyCheckMaxAttempts,
			50);
		TestEqual(
			TEXT("Profile sync max attempts"),
			Definition->GetProfileSyncSettings().LocalShopSaveSyncMaxAttempts,
			5);
		TestEqual(
			TEXT("Client skin-name validation limit"),
			Definition->GetProfileSyncSettings().MaxClientSyncedSkinNameCount,
			512);
		TestEqual(
			TEXT("Local cosmetic profile policy"),
			Definition->GetProfileSyncSettings().RemoteSkinClaimPolicy,
			EPdRemoteSkinClaimPolicy::TrustLocalCosmeticProfile);
		TestEqual(
			TEXT("Remote cosmetic sync rate limit"),
			Definition->GetProfileSyncSettings().RemoteSkinSyncMinInterval,
			0.20f);
		TestEqual(
			TEXT("Default input definition path"),
			Definition->GetInputSettings().DefaultInputDefinition.ToSoftObjectPath(),
			UControllerInputDefinition::GetDefaultInputDefinitionPath());
	}

	TestTrue(
		TEXT("Local cosmetic policy accepts a non-default catalog name"),
		UControllerProfileSyncComponent::IsRemoteSkinNameAllowedByPolicy(
			TEXT("DA_Jean"),
			EPdRemoteSkinClaimPolicy::TrustLocalCosmeticProfile));
	TestTrue(
		TEXT("Fail-closed policy accepts a default skin"),
		UControllerProfileSyncComponent::IsRemoteSkinNameAllowedByPolicy(
			TEXT("DA_SantaHat"),
			EPdRemoteSkinClaimPolicy::DefaultUnlocksOnly));
	TestFalse(
		TEXT("Fail-closed policy rejects a purchased skin"),
		UControllerProfileSyncComponent::IsRemoteSkinNameAllowedByPolicy(
			TEXT("DA_Jean"),
			EPdRemoteSkinClaimPolicy::DefaultUnlocksOnly));
	TestFalse(
		TEXT("No policy accepts an empty skin name"),
		UControllerProfileSyncComponent::IsRemoteSkinNameAllowedByPolicy(
			NAME_None,
			EPdRemoteSkinClaimPolicy::TrustLocalCosmeticProfile));

	TestNull(
		TEXT("Legacy arbitrary skin-save RPC is removed"),
		Controller->FindFunction(TEXT("Server_ApplyLocalSkinSave")));
	const UFunction* CosmeticProfileRpc =
		Controller->FindFunction(TEXT("Server_SubmitLocalCosmeticProfile"));
	TestNotNull(TEXT("Bounded local cosmetic profile RPC"), CosmeticProfileRpc);
	if (CosmeticProfileRpc)
	{
		TestTrue(
			TEXT("Cosmetic profile RPC executes on the server"),
			CosmeticProfileRpc->HasAnyFunctionFlags(FUNC_NetServer));
	}

	return !HasAnyErrors();
}

#endif
