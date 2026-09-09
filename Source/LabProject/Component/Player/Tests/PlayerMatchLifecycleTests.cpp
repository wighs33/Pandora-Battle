#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/Player/PlayerLoadoutComponent.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Mode/ExperienceGameMode.h"

// 플레이어 식별 정보는 인계하되, 새 경기의 점수·사망 횟수·맵 구역·선택 슬롯은 초기화해야 한다.
// 상태 인계와 입장 함수를 직접 호출하는 검사이며, 실제 맵 이동·네트워크·리스폰은 검사하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerMatchLifecycleTest,
	"LabProject.Player.Match.IdentityHandoffAndNewMatchReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerMatchLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Temporary world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	APdPlayerState* Lobby = World->SpawnActor<APdPlayerState>();
	APdPlayerState* PlayerState = World->SpawnActor<APdPlayerState>();
	AExperienceGameMode* GameMode = World->SpawnActor<AExperienceGameMode>();
	ALobbyGameMode* LobbyGameMode = World->SpawnActor<ALobbyGameMode>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Lobby PlayerState"), Lobby) || !TestNotNull(TEXT("Match PlayerState"), PlayerState)
		|| !TestNotNull(TEXT("Match GameMode"), GameMode) || !TestNotNull(TEXT("Lobby GameMode"), LobbyGameMode)
		|| !TestNotNull(TEXT("Player controller"), Controller))
	{
		return false;
	}
	Controller->SetPlayerState(PlayerState);
	UPlayerMatchComponent* Match = PlayerState->GetPlayerMatchComponent();
	UPlayerLoadoutComponent* Loadout = PlayerState->GetPlayerLoadoutComponent();

	// 피처가 없는 상태에서도 공통 PlayerState의 기본 속성이 엔진 초기화로 등록되어야 한다.
	UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent();
	if (!ASC->HasBeenInitialized())
	{
		ASC->InitializeComponent();
	}
	TestNotNull(TEXT("Common PlayerState supplies basic attributes without a GameFeature"), ASC->GetSet<UBasicAttributeSet>());
	int32 BasicAttributeSetCount = 0;
	for (const UAttributeSet* AttributeSet : ASC->GetSpawnedAttributes())
	{
		BasicAttributeSetCount += AttributeSet && AttributeSet->IsA<UBasicAttributeSet>() ? 1 : 0;
	}
	TestEqual(TEXT("Basic attributes are registered exactly once"), BasicAttributeSetCount, 1);

	// 준비: 인계할 식별 정보와 이전 경기의 기록을 서로 구분할 수 있는 값으로 채운다.
	Lobby->GetLobbyPlayerStateComponent()->SetNickname(FText::FromString(TEXT("ConfirmedPlayer")));
	Lobby->GetPlayerMatchComponent()->SetMatchTeamColorIndex(1);
	Lobby->GetPlayerMatchComponent()->SetMatchSpawnIndex(2);
	Lobby->GetPlayerMatchComponent()->SetSelectedAchievementId(TEXT("TestAchievement"));
	Lobby->GetPlayerMatchComponent()->SetPlayerMapRegion(EPlayerMapRegion::Temple);
	Lobby->GetPlayerMatchComponent()->RecordDeath(5);
	Lobby->GetPlayerLoadoutComponent()->RequestSelectLoadout(2);
	Lobby->SetScore(9.0f);
	const FPlayerMatchIdentity Identity = Lobby->GetPlayerMatchComponent()->GetPlayerMatchIdentity();

	// 실행·확인: 엔진의 인계 경로를 호출해 식별 정보만 프로젝트 상태로 전달되는지 검사한다.
	Lobby->SeamlessTravelTo(PlayerState);
	TestTrue(TEXT("Handoff preserves name, team, spawn index and achievement"), Match->GetPlayerMatchIdentity().Matches(Identity));
	TestEqual(TEXT("Handoff does not copy previous deaths"), Match->GetDeathCount(), 0);
	TestEqual(TEXT("Handoff does not copy the previous region"), Match->GetPlayerMapRegion(), EPlayerMapRegion::Dome);
	TestEqual(TEXT("Handoff does not copy the previous selection"), Loadout->GetSelectedLoadoutNumber(), 0);
	TestEqual(TEXT("Engine handoff carries score until match entry"), PlayerState->GetScore(), 9.0f);

	// 대상에 남은 값도 초기화되는지 확인한다. 기본값 0끼리 비교하는 검사에 그치지 않는다.
	Match->RecordDeath(3);
	Match->SetPlayerMapRegion(EPlayerMapRegion::Temple);
	Loadout->RequestSelectLoadout(3);
	if (!TestEqual(TEXT("Nonzero deaths are prepared"), Match->GetDeathCount(), 3)
		|| !TestEqual(TEXT("Nonzero selection is prepared"), Loadout->GetSelectedLoadoutNumber(), 3))
	{
		return false;
	}
	GameMode->GenericPlayerInitialization(Controller);
	TestTrue(TEXT("New match retains the transferred identity"), Match->GetPlayerMatchIdentity().Matches(Identity));
	TestEqual(TEXT("New match resets the copied score"), PlayerState->GetScore(), 0.0f);
	TestEqual(TEXT("New match resets deaths"), Match->GetDeathCount(), 0);
	TestEqual(TEXT("A map without explicit settings uses the default region"), Match->GetPlayerMapRegion(), EPlayerMapRegion::Dome);
	TestEqual(TEXT("New match clears the selected loadout"), Loadout->GetSelectedLoadoutNumber(), 0);

	// CopyProperties 호출 없이 재입장해도 로비 전용 상태를 새로 준비하고 공통 식별 정보는 유지한다.
	ULobbyPlayerStateComponent* LobbyState = PlayerState->GetLobbyPlayerStateComponent();
	LobbyState->SetDefaultNickname(FText::FromString(TEXT("PreviousLobbyHint")));
	Match->SetPlayerMatchIdentity(Identity);
	LobbyState->SetLeavingLobby(true);
	TestFalse(TEXT("Stale nickname hint differs from the confirmed name"), LobbyState->GetNicknameHint().EqualTo(Identity.DisplayName));
	TestTrue(TEXT("Previous lobby leaving state is prepared"), LobbyState->IsLeavingLobby());
	LobbyGameMode->GenericPlayerInitialization(Controller);
	TestTrue(TEXT("Lobby entry preserves name, team, spawn index and achievement"), Match->GetPlayerMatchIdentity().Matches(Identity));
	TestFalse(TEXT("Lobby entry clears previous leaving state"), LobbyState->IsLeavingLobby());
	TestTrue(TEXT("Lobby entry uses the confirmed name as its new hint"), LobbyState->GetNicknameHint().EqualTo(Identity.DisplayName));
	TestTrue(TEXT("Lobby entry activates its nickname hint"), LobbyState->IsUsingNicknameHint());
	TestEqual(TEXT("Lobby entry uses the lobby update frequency"), PlayerState->GetNetUpdateFrequency(), 30.0f);
	GameMode->GenericPlayerInitialization(Controller);
	TestEqual(TEXT("Match entry restores the gameplay update frequency"), PlayerState->GetNetUpdateFrequency(), 100.0f);
	return true;
}

#endif
