#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "AbilitySystemComponent.h"
#include "AI/Bot/PdBotController.h"
#include "AIController.h"
#include "Character/PdPlayer.h"
#include "Common/Enum_Direction.h"
#include "Common/GameResultTypes.h"
#include "Component/Lobby/LobbyConfigurationComponent.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Match/MatchOutcomeRules.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Components/Button.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraSkillSource.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "UI/Lobby/LobbyWidget.h"
#include "UObject/UObjectIterator.h"

// 방장이 대기실의 빈 자리를 봇으로 채우고, 봇이 경기로 함께 넘어가 사람처럼 싸운 뒤 대기실로 돌아오는지 본다.
// 봇과 얽힌 처치·사망과 팀 인원은 승리 골드에 들어가지 않고, 봇이 낀 경기는 전적에 남지 않는다.

namespace
{
	/** 봇 경기의 봇 수. 팀 인원이 같아야 시작할 수 있어, 방장과 봇 하나 대 봇 둘로 나눈다. */
	constexpr int32 BotMatchBotCount = 3;
	/** 경기가 끝나는 길까지 보려고 이 실행에서만 줄이는 경기 시간. 봇이 스폰되어 스킬을 쓰고, 잡힌 봇이 다시 태어날 만큼 둔다. */
	constexpr float ShortMatchTimerSeconds = 60.f;
	/** 방어력과 보호막을 넘어 한 번에 쓰러뜨리는 피해 */
	constexpr float LethalDamage = 1.0e6f;

	bool IsSettledInMap(const FString& MapPath)
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World && World->HasBegunPlay() && !World->IsInSeamlessTravel() && World->GetOutermost()->GetName() == MapPath;
	}

	ALobbyGameMode* GetLobbyGameMode()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World ? World->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	}

	AExperienceGameMode* GetMatchGameMode()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World ? World->GetAuthGameMode<AExperienceGameMode>() : nullptr;
	}

	UPlayerProfileSubsystem* GetProfile()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World ? World->GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>() : nullptr;
	}

	/** 대기실이나 경기의 참가자. bBotsOnly면 봇만 고른다. */
	TArray<APdPlayerState*> GetParticipantStates(const bool bBotsOnly)
	{
		TArray<APdPlayerState*> Participants;
		const UWorld* World = IntegrationTest::GetGameWorld();
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (GameState)
		{
			for (APlayerState* PlayerState : GameState->PlayerArray)
			{
				APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
				if (PdPlayerState && (!bBotsOnly || PdPlayerState->IsABot()))
				{
					Participants.Add(PdPlayerState);
				}
			}
		}
		return Participants;
	}

	/** 맵을 옮기면 PlayerState가 새로 생기므로, 참가자는 옮겨 가도 그대로인 플레이어 번호로 찾는다. */
	APdPlayerState* FindParticipantById(const int32 PlayerId)
	{
		for (APdPlayerState* Participant : GetParticipantStates(false))
		{
			if (Participant->GetPlayerId() == PlayerId)
			{
				return Participant;
			}
		}
		return nullptr;
	}

	APdPlayerState* GetHostState()
	{
		const APlayerController* Controller = IntegrationTest::GetPlayerController();
		return Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
	}

	int32 GetTeamOf(const APdPlayerState& Participant)
	{
		return Participant.GetPlayerMatchComponent()->GetMatchTeamColorIndex();
	}

	APdPlayer* GetCharacterOf(const APdPlayerState* Participant)
	{
		return Participant ? Cast<APdPlayer>(Participant->GetPawn()) : nullptr;
	}

	bool IsCharacterAlive(const APdPlayerState* Participant)
	{
		const APdPlayer* Character = GetCharacterOf(Participant);
		return Character && !Character->IsDead();
	}

	const UButton* GetAddBotButton()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		for (TObjectIterator<ULobbyWidget> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && It->GetWorld() == World && It->IsActivated())
			{
				return Cast<UButton>(It->GetWidgetFromName(TEXT("Btn_AddBot")));
			}
		}
		return nullptr;
	}

	/** 봇 조종기가 스폰된 캐릭터를 조종하고, 경기 지급을 받아 지금 판도라에 맞는 무기를 들었다. */
	bool IsBotReadyToFight(const APdPlayerState& Bot)
	{
		APdPlayer* Character = GetCharacterOf(&Bot);
		AController* Controller = Character ? Character->GetController() : nullptr;
		const AExperienceGameMode* GameMode = GetMatchGameMode();
		const UPandoraDefinition* Pandora = Bot.GetPandoraComponent()->GetCurrentPandoraDefinition();
		const UItemDefinition* Weapon = Character ? Character->GetEquipmentComponent()->GetCurrentWeaponDefinition() : nullptr;
		return Cast<APdBotController>(Controller) && GameMode && GameMode->GetPlayerSetupComponent()->IsPlayerReadyForGameplay(Controller)
			&& Pandora && Weapon && Pandora->IsCompatibleWithWeaponDefinition(Weapon);
	}

	/** 상대 봇이 스스로 쫓거나 치지 않게 세운다. 조종기는 다시 태어난 캐릭터도 그대로 조종하므로 경기 내내 서 있다. */
	void HoldBotStill(const APdPlayerState& Bot)
	{
		AAIController* Controller = Cast<AAIController>(Bot.GetOwner());
		if (!Controller)
		{
			return;
		}
		Controller->SetActorTickEnabled(false);
		Controller->StopMovement();
		if (APdPlayer* Character = GetCharacterOf(&Bot))
		{
			Character->GetCombatComponent()->StopPrimaryAttack();
		}
	}

	/** 방 세션을 만들고 방장으로 대기실을 열어, 봇을 넣고 경기를 시작할 수 있을 때까지 기다린다. */
	void OpenBotLobby(FAutomationTestBase* Test)
	{
		IntegrationTest::CreateRoomSession(Test);
		IntegrationTest::Step(0.f, []()
		{
			if (const UOnlineSessionsSubsystem* Sessions = IntegrationTest::GetSessions())
			{
				Sessions->TravelToListenMap(IntegrationTest::LobbyMap);
			}
		});
		IntegrationTest::WaitUntil(Test, TEXT("대기실 열기"), 60.f, []()
		{
			const ALobbyGameMode* Lobby = GetLobbyGameMode();
			return IsSettledInMap(IntegrationTest::LobbyMap) && Lobby && Lobby->CanAddBot() && Lobby->CanHostStartGame();
		});
	}

	/** 다음 테스트가 새 방을 만들 수 있게 방 세션을 닫고 타이틀로 돌아간다. 방장이 나가면 봇도 남지 않는다. */
	void LeaveBotLobby(FAutomationTestBase* Test)
	{
		IntegrationTest::DestroyRoomSession(Test);
		IntegrationTest::Step(0.f, []() { UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*IntegrationTest::TitleMap)); });
		IntegrationTest::WaitUntil(Test, TEXT("타이틀로 돌아가기"), 30.f, []() { return IsSettledInMap(IntegrationTest::TitleMap); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLobbyBotSeatsTest, "LabProject.Integration.LobbyBots.Seats",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 방장이 고른 맵의 정원까지 봇으로 자리를 채우면 더 넣지 못하고, 봇을 내보내면 다시 넣을 수 있다. 봇 추가 단추도 빈 자리가 있을 때만 눌린다.
// 봇마다 플레이어 번호와 이름이 다르고, 봇 팀을 바꿔 팀 인원이 어긋나면 시작할 수 없다.
bool FPdLobbyBotSeatsTest::RunTest(const FString& Parameters)
{
	OpenBotLobby(this);
	IntegrationTest::Step(0.f, [this]()
	{
		ALobbyGameMode* Lobby = GetLobbyGameMode();
		if (!TestNotNull(TEXT("대기실 게임 모드"), Lobby))
		{
			return;
		}
		const int32 MaxPlayerCount = Lobby->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount();
		for (int32 Seat = GetParticipantStates(false).Num(); Seat < MaxPlayerCount; ++Seat)
		{
			TestTrue(TEXT("빈 자리에 봇을 넣는다"), Lobby->AddBot());
		}
		const TArray<APdPlayerState*> Participants = GetParticipantStates(false);
		TestEqual(TEXT("고른 맵의 정원까지 찬다"), Participants.Num(), MaxPlayerCount);
		TestFalse(TEXT("정원이 차면 더 넣지 못한다"), Lobby->AddBot());

		TSet<int32> PlayerIds;
		TSet<FString> Names;
		for (const APdPlayerState* Participant : Participants)
		{
			PlayerIds.Add(Participant->GetPlayerId());
			Names.Add(Participant->GetPlayerMatchComponent()->GetMatchDisplayName().ToString());
		}
		TestEqual(TEXT("플레이어 번호가 겹치지 않는다"), PlayerIds.Num(), Participants.Num());
		TestEqual(TEXT("이름이 겹치지 않는다"), Names.Num(), Participants.Num());
	});
	IntegrationTest::WaitUntil(this, TEXT("자리가 차면 봇 추가 단추가 눌리지 않는다"), 5.f, []()
	{
		const UButton* AddBot = GetAddBotButton();
		return AddBot && AddBot->IsVisible() && !AddBot->GetIsEnabled();
	});

	IntegrationTest::Step(0.f, []()
	{
		ALobbyGameMode* Lobby = GetLobbyGameMode();
		const TArray<APdPlayerState*> Bots = GetParticipantStates(true);
		if (Lobby && !Bots.IsEmpty())
		{
			Lobby->KickPlayer(Bots.Last());
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("봇을 내보내면 자리가 비고 봇 추가 단추가 눌린다"), 5.f, []()
	{
		const ALobbyGameMode* Lobby = GetLobbyGameMode();
		const UButton* AddBot = GetAddBotButton();
		return Lobby && Lobby->CanAddBot() && AddBot && AddBot->IsVisible() && AddBot->GetIsEnabled();
	});

	IntegrationTest::Step(0.f, [this]()
	{
		ALobbyGameMode* Lobby = GetLobbyGameMode();
		const TArray<APdPlayerState*> Bots = GetParticipantStates(true);
		if (!TestNotNull(TEXT("대기실 게임 모드"), Lobby) || !TestTrue(TEXT("팀을 겹쳐 볼 봇이 둘 이상 남았다"), Bots.Num() >= 2))
		{
			return;
		}
		TestTrue(TEXT("모두 한 명씩 다른 팀이면 시작할 수 있다"), Lobby->CanHostStartGame());
		const int32 OriginalTeam = GetTeamOf(*Bots[1]);
		Lobby->SetBotTeamColor(Bots[1], GetTeamOf(*Bots[0]));
		TestFalse(TEXT("봇 팀을 겹쳐 팀 인원이 어긋나면 시작할 수 없다"), Lobby->CanHostStartGame());
		Lobby->SetBotTeamColor(Bots[1], OriginalTeam);
		TestTrue(TEXT("팀을 되돌리면 시작할 수 있다"), Lobby->CanHostStartGame());
	});
	LeaveBotLobby(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLobbyBotMatchTest, "LabProject.Integration.LobbyBots.Match",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 방장과 봇 하나 대 봇 둘로 경기를 시작한다. 봇은 봇 조종기로 경기에 스폰되어 서로 다른 판도라 세 개와 그에 맞는 무기를 받고, 판도라 스킬을 쓴다.
// 사람이 봇을 잡으면 처치 수만 오르고 골드용 처치 수는 그대로이며, 잡힌 봇은 다시 태어난다.
// 경기가 끝나면 승리 골드의 팀 인원에 봇을 세지 않고, 모두 봇과 함께 대기실로 돌아오며, 봇이 낀 경기는 전적에 남지 않는다.
// 승패가 정해지게 상대 봇 둘은 경기 내내 세워 두고, 경기 시간은 이 실행에서만 줄인다(애셋은 저장하지 않는다).
bool FPdLobbyBotMatchTest::RunTest(const FString& Parameters)
{
	struct FBotMatch
	{
		int32 HostTeam = INDEX_NONE;
		int32 AllyBotId = INDEX_NONE;
		TArray<int32> EnemyBotIds;
		FString MatchMap;
		TWeakObjectPtr<UMatchRuleDefinition> MatchRule;
		float OriginalMatchTimerSeconds = 0.f;
		int32 GoldBefore = 0;
		int32 RecordCountBefore = 0;
		int32 WinCountBefore = 0;

		int32 VictimId = INDEX_NONE;
		TWeakObjectPtr<UAbilitySystemComponent> AllyAbilities;
		FDelegateHandle AllySkillHandle;
		int32 AllySkillCount = 0;

		bool bResultReceived = false;
		int32 WinnerTeam = INDEX_NONE;
		TArray<FGameResultPlayerStat> ResultStats;
	};
	TSharedRef<FBotMatch> Match = MakeShared<FBotMatch>();

	OpenBotLobby(this);
	IntegrationTest::Step(0.f, [this, Match]()
	{
		ALobbyGameMode* Lobby = GetLobbyGameMode();
		const APdPlayerState* Host = GetHostState();
		if (!TestNotNull(TEXT("대기실 게임 모드"), Lobby) || !TestTrue(TEXT("고른 맵에 봇 경기를 할 자리가 있다"),
				Lobby->GetLobbyConfigurationComponent()->GetConfiguredMaxPlayerCount() >= BotMatchBotCount + 1)
			|| !TestNotNull(TEXT("방장"), Host))
		{
			return;
		}
		for (int32 Index = 0; Index < BotMatchBotCount; ++Index)
		{
			TestTrue(TEXT("봇 추가"), Lobby->AddBot());
		}
		const TArray<APdPlayerState*> Bots = GetParticipantStates(true);
		if (!TestEqual(TEXT("봇 수"), Bots.Num(), BotMatchBotCount))
		{
			return;
		}

		// 봇은 들어올 때 저마다 빈 팀을 받는다. 첫 봇은 방장 팀으로, 셋째 봇은 둘째 봇 팀으로 옮긴다.
		Match->HostTeam = GetTeamOf(*Host);
		Lobby->SetBotTeamColor(Bots[0], Match->HostTeam);
		Lobby->SetBotTeamColor(Bots[2], GetTeamOf(*Bots[1]));
		Match->AllyBotId = Bots[0]->GetPlayerId();
		Match->EnemyBotIds = {Bots[1]->GetPlayerId(), Bots[2]->GetPlayerId()};
		TestNotEqual(TEXT("상대 봇은 방장 팀이 아니다"), GetTeamOf(*Bots[1]), Match->HostTeam);
		TestTrue(TEXT("두 명씩 두 팀이면 시작할 수 있다"), Lobby->CanHostStartGame());
	});
	IntegrationTest::Step(0.f, [this, Match]()
	{
		ALobbyGameMode* Lobby = GetLobbyGameMode();
		const ALobbyGameState* LobbyState = IntegrationTest::GetGameWorld()->GetGameState<ALobbyGameState>();
		const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
		FLobbyMatchMapOption MapOption;
		UPlayerProfileSubsystem* Profile = GetProfile();
		if (!TestNotNull(TEXT("대기실 게임 모드"), Lobby)
			|| !TestTrue(TEXT("고른 맵이 맵 목록에 있다"), Levels && LobbyState && Levels->FindIngameLevel(LobbyState->GetSelectedMapKey(), MapOption))
			|| !TestNotNull(TEXT("프로필"), Profile))
		{
			return;
		}
		Match->MatchMap = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
		Match->GoldBefore = Profile->GetGold();
		Match->RecordCountBefore = Profile->GetMatchRecords().Num();
		Match->WinCountBefore = Profile->GetWinCount();

		// 경기 규칙은 대기실과 경기가 함께 쓰는 애셋이다. 맵을 옮기는 동안 내려가지 않게 붙잡고 경기 시간만 줄인다.
		UMatchRuleDefinition* MatchRule = const_cast<UMatchRuleDefinition*>(Lobby->GetLobbyConfigurationComponent()->GetMatchRuleDefinition());
		if (TestNotNull(TEXT("경기 규칙"), MatchRule))
		{
			MatchRule->AddToRoot();
			Match->MatchRule = MatchRule;
			Match->OriginalMatchTimerSeconds = MatchRule->MatchTimerSeconds;
			MatchRule->MatchTimerSeconds = ShortMatchTimerSeconds;
		}
		Lobby->TryStartGame();
	});
	IntegrationTest::WaitUntil(this, TEXT("경기 맵 도착"), 120.f, [Match]()
	{
		return !Match->MatchMap.IsEmpty() && IsSettledInMap(Match->MatchMap) && GetMatchGameMode();
	});
	IntegrationTest::WaitUntil(this, TEXT("봇 셋이 경기에 스폰되어 판도라에 맞는 무기를 든다"), 60.f, []()
	{
		const TArray<APdPlayerState*> Bots = GetParticipantStates(true);
		return Bots.Num() == BotMatchBotCount && !Bots.ContainsByPredicate([](const APdPlayerState* Bot) { return !IsBotReadyToFight(*Bot); });
	});

	IntegrationTest::Step(0.f, [this, Match]()
	{
		for (const APdPlayerState* Bot : GetParticipantStates(true))
		{
			TSet<const UPandoraDefinition*> Pandoras;
			for (const EEnum_Direction Direction : {EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right})
			{
				Pandoras.Add(Bot->GetPandoraComponent()->GetPandoraLoadoutDefinition(Direction));
			}
			TestTrue(TEXT("봇은 서로 다른 판도라 세 개를 받는다"), Pandoras.Num() == 3 && !Pandoras.Contains(nullptr));
			const bool bAlly = Bot->GetPlayerId() == Match->AllyBotId;
			TestTrue(TEXT("봇 팀은 대기실에서 정한 그대로다"), bAlly == (GetTeamOf(*Bot) == Match->HostTeam));
		}
		for (const int32 EnemyBotId : Match->EnemyBotIds)
		{
			if (const APdPlayerState* Enemy = FindParticipantById(EnemyBotId))
			{
				HoldBotStill(*Enemy);
			}
		}

		const APdPlayerState* Ally = FindParticipantById(Match->AllyBotId);
		UAbilitySystemComponent* AllyAbilities = Ally ? Ally->GetAbilitySystemComponent() : nullptr;
		if (TestNotNull(TEXT("내 편 봇의 능력"), AllyAbilities))
		{
			Match->AllyAbilities = AllyAbilities;
			Match->AllySkillHandle = AllyAbilities->AbilityActivatedCallbacks.AddLambda([Match](UGameplayAbility* Ability)
			{
				const UPandoraSkillSource* Source = Ability ? Cast<UPandoraSkillSource>(Ability->GetCurrentSourceObject()) : nullptr;
				Match->AllySkillCount += Source && Source->GetSkillDataAsset() ? 1 : 0;
			});
		}

		AExperienceGameState* GameState = IntegrationTest::GetGameWorld()->GetGameState<AExperienceGameState>();
		if (TestNotNull(TEXT("경기 상태"), GameState))
		{
			GameState->OnGameResultReceived().AddLambda([Match](const FText&, const int32 WinnerTeamColorIndex, const FText&, int32,
				const TArray<FGameResultPlayerStat>& PlayerStats)
			{
				Match->bResultReceived = true;
				Match->WinnerTeam = WinnerTeamColorIndex;
				Match->ResultStats = PlayerStats;
			});
		}
	});

	// 내 편 봇이 상대 봇을 먼저 잡았을 수 있어, 살아 있는 상대 봇을 고른다.
	IntegrationTest::WaitUntil(this, TEXT("상대 봇이 살아 있다"), 30.f, [Match]()
	{
		return Match->EnemyBotIds.ContainsByPredicate([](const int32 Id) { return IsCharacterAlive(FindParticipantById(Id)); });
	});
	IntegrationTest::Step(0.f, [this, Match]()
	{
		const APdPlayerState* Host = GetHostState();
		APdPlayer* HostCharacter = GetCharacterOf(Host);
		const int32* VictimId = Match->EnemyBotIds.FindByPredicate([](const int32 Id) { return IsCharacterAlive(FindParticipantById(Id)); });
		APdPlayerState* Victim = VictimId ? FindParticipantById(*VictimId) : nullptr;
		if (!TestNotNull(TEXT("방장 캐릭터"), HostCharacter) || !TestNotNull(TEXT("살아 있는 상대 봇"), Victim))
		{
			return;
		}
		const UPlayerMatchComponent* HostRecord = Host->GetPlayerMatchComponent();
		const int32 KillsBefore = HostRecord->GetKillCount();
		const int32 RewardKillsBefore = HostRecord->GetRewardKillCount();
		const int32 VictimDeathsBefore = Victim->GetPlayerMatchComponent()->GetDeathCount();
		Match->VictimId = Victim->GetPlayerId();

		DamageRules::FOutgoingDamage Lethal;
		Lethal.Damage = LethalDamage;
		Lethal.bAllowHitReact = false;
		HostCharacter->GetCombatComponent()->ApplyOutgoingDamageToTarget(GetCharacterOf(Victim), Lethal, HostCharacter, HostCharacter);
		TestFalse(TEXT("방장이 봇을 쓰러뜨린다"), IsCharacterAlive(Victim));
		TestEqual(TEXT("처치 수는 오른다"), HostRecord->GetKillCount(), KillsBefore + 1);
		TestEqual(TEXT("골드용 처치 수는 그대로다"), HostRecord->GetRewardKillCount(), RewardKillsBefore);
		TestEqual(TEXT("봇의 사망이 기록된다"), Victim->GetPlayerMatchComponent()->GetDeathCount(), VictimDeathsBefore + 1);
	});
	IntegrationTest::WaitUntil(this, TEXT("잡힌 봇이 다시 태어난다"), 30.f, [Match]()
	{
		return IsCharacterAlive(FindParticipantById(Match->VictimId));
	});
	IntegrationTest::WaitUntil(this, TEXT("내 편 봇이 판도라 스킬을 쓴다"), 45.f, [Match]() { return Match->AllySkillCount > 0; });
	IntegrationTest::Step(0.f, [Match]()
	{
		if (UAbilitySystemComponent* AllyAbilities = Match->AllyAbilities.Get())
		{
			AllyAbilities->AbilityActivatedCallbacks.Remove(Match->AllySkillHandle);
		}
	});

	IntegrationTest::WaitUntil(this, TEXT("경기 시간이 끝나 결과가 나온다"), ShortMatchTimerSeconds + 30.f, [Match]() { return Match->bResultReceived; });
	IntegrationTest::Step(0.f, [this, Match]()
	{
		const AExperienceGameMode* GameMode = GetMatchGameMode();
		const APdPlayerState* Host = GetHostState();
		if (!Match->bResultReceived || !TestNotNull(TEXT("경기 게임 모드"), GameMode) || !TestNotNull(TEXT("방장"), Host))
		{
			return;
		}
		// 상대 봇은 서 있었고 방장이 한 번 잡았으므로 방장 편이 이긴다.
		TestEqual(TEXT("방장 편이 이긴다"), Match->WinnerTeam, Match->HostTeam);
		const FVictoryGoldRates Rates = GameMode->GetVictoryGoldRates();
		TestTrue(TEXT("팀 인원 골드가 있어야 봇을 세는지 가릴 수 있다"), Rates.GoldPerWinningTeamMember > 0);

		const int32 HostId = Host->GetPlayerId();
		const FGameResultPlayerStat* HostStat = Match->ResultStats.FindByPredicate([HostId](const FGameResultPlayerStat& Stat)
		{
			return Stat.PlayerStateId == HostId;
		});
		if (TestNotNull(TEXT("결과에 방장이 있다"), HostStat))
		{
			constexpr int32 HumanWinnerCount = 1;
			const int32 ExpectedGold = MatchOutcomeRules::CalculateVictoryGold(
				HostStat->RewardKillCount, HostStat->RewardDeathCount, HumanWinnerCount, Rates);
			TestEqual(TEXT("승리 골드의 팀 인원에 내 편 봇을 세지 않는다"), HostStat->GoldReward, ExpectedGold);
			TestEqual(TEXT("결과에 보인 골드가 프로필에 들어온다"), GetProfile()->GetGold() - Match->GoldBefore, ExpectedGold);
		}
		int32 BotStatCount = 0;
		for (const FGameResultPlayerStat& Stat : Match->ResultStats)
		{
			if (Stat.PlayerStateId != HostId)
			{
				++BotStatCount;
				TestTrue(TEXT("봇은 결과에 봇으로 표시된다"), Stat.bBot);
				TestEqual(TEXT("봇은 골드를 받지 않는다"), Stat.GoldReward, 0);
			}
		}
		TestEqual(TEXT("결과에 봇 셋이 있다"), BotStatCount, BotMatchBotCount);
	});

	IntegrationTest::WaitUntil(this, TEXT("봇과 함께 대기실로 돌아온다"), 90.f, []()
	{
		return IsSettledInMap(IntegrationTest::LobbyMap) && GetLobbyGameMode() && GetParticipantStates(true).Num() == BotMatchBotCount;
	});
	IntegrationTest::Step(1.f, [this, Match]()
	{
		for (const APdPlayerState* Bot : GetParticipantStates(true))
		{
			TestFalse(TEXT("돌아온 봇은 대기실에 남는다"), Bot->GetLobbyPlayerStateComponent()->IsLeavingLobby());
		}
		const ALobbyGameMode* Lobby = GetLobbyGameMode();
		TestTrue(TEXT("돌아온 대기실에서 다시 시작할 수 있다"), Lobby && Lobby->CanHostStartGame());
		UPlayerProfileSubsystem* Profile = GetProfile();
		TestEqual(TEXT("봇이 낀 경기는 전적에 남지 않는다"), Profile->GetMatchRecords().Num(), Match->RecordCountBefore);
		TestEqual(TEXT("승수도 그대로다"), Profile->GetWinCount(), Match->WinCountBefore);
	});

	// 줄인 경기 시간을 되돌린다. 앞 단계가 실패해도 이 단계는 돈다.
	IntegrationTest::Step(0.f, [Match]()
	{
		if (UMatchRuleDefinition* MatchRule = Match->MatchRule.Get())
		{
			MatchRule->MatchTimerSeconds = Match->OriginalMatchTimerSeconds;
			MatchRule->RemoveFromRoot();
		}
	});
	LeaveBotLobby(this);
	return true;
}

#endif
