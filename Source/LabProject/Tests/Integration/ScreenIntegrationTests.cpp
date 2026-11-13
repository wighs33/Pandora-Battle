#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/DisplaySettingsSubsystem.h"
#include "UI/Settings/DisplaySettingsWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Definition/Level/LevelDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/HUD.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Lobby/Contents/LobbyGameState.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Luna/LunaChatSubsystem.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/Core/LoadingScreenSubsystem.h"
#include "UI/Guide/GuideWidget.h"
#include "UI/Lobby/LobbyUserWidget.h"
#include "UI/Lobby/LobbyWidget.h"
#include "UI/Record/RecordWidget.h"
#include "UI/Room/RoomListWidget.h"
#include "UI/Settings/GameSettingsWidget.h"
#include "UI/Shop/ShopWidget.h"
#include "UI/Title/TitleWidget.h"
#include "UObject/UObjectIterator.h"

// 화면(UScreenWidget)이 CommonUI 화면 층에 올라가고, 뒤로 가기(ESC)로 닫히며, 로딩 화면이 화면이 뜬 것을 보고 내려가는지 본다.

namespace
{
	const FString RoomMap = TEXT("/Game/Map/LV_Room");
	const FString LobbyMap = TEXT("/Game/Map/LV_Lobby");

	/** 게임 월드에 있는 TScreen 가운데 떠 있는 화면. 닫힌 화면은 다시 열 때 새로 만들어질 수 있어 모두 살펴본다. */
	template <typename TScreen>
	TScreen* FindActiveScreen()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		for (TObjectIterator<TScreen> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && It->GetWorld() == World && It->IsActivated())
			{
				return *It;
			}
		}
		return nullptr;
	}

	template <typename TScreen>
	bool IsScreenActive()
	{
		return FindActiveScreen<TScreen>() != nullptr;
	}

	void PressBack(const float Delay)
	{
		IntegrationTest::Step(Delay, []() { IntegrationTest::PressKey(EKeys::Escape, true); });
		IntegrationTest::Step(0.1f, []() { IntegrationTest::PressKey(EKeys::Escape, false); });
	}

	/** Open으로 화면을 열고, 그 화면이 뜬 뒤 뒤로 가기로 닫혀 타이틀이 다시 맨 위 화면이 되는지 본다. */
	template <typename TScreen>
	void ExpectScreenClosesOnBack(FAutomationTestBase* Test, const FString& What, TFunction<void(UTitleWidget&)> Open)
	{
		IntegrationTest::Step(0.2f, [Open = MoveTemp(Open)]()
		{
			if (UTitleWidget* Title = FindActiveScreen<UTitleWidget>())
			{
				Open(*Title);
			}
		});
		IntegrationTest::WaitUntil(Test, What + TEXT(" 열기"), 10.f, []() { return IsScreenActive<TScreen>(); });
		PressBack(0.5f);
		IntegrationTest::WaitUntil(Test, What + TEXT(" 뒤로 가기로 닫기"), 5.f, []() { return !IsScreenActive<TScreen>(); });
		IntegrationTest::Step(0.2f, [Test, What]()
		{
			Test->TestTrue(What + TEXT("을 닫으면 타이틀이 남는다"), IsScreenActive<UTitleWidget>());
		});
	}

	void ClickTitleButton(UTitleWidget& Title, const TCHAR* Handler)
	{
		if (UFunction* Function = Title.FindFunction(Handler))
		{
			Title.ProcessEvent(Function, nullptr);
		}
	}

	bool IsLoadingScreenUp()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
		const ULoadingScreenSubsystem* Loading = LocalPlayer ? LocalPlayer->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
		return !Loading || Loading->HasBlockingWait();
	}

	/** 맵을 열고 그 맵의 화면 TScreen이 뜨고 로딩 화면이 내려갈 때까지 기다린다. Pawn이 없는 메뉴 맵에 쓴다. */
	template <typename TScreen>
	void OpenMenuMap(FAutomationTestBase* Test, const FString& MapPath, TFunction<void()> Open)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Open = MoveTemp(Open)]()
		{
			Open();
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
		IntegrationTest::WaitUntil(Test, MapPath + TEXT(" 화면이 뜨고 로딩 화면 내리기"), 30.f, [MapPath]()
		{
			const UWorld* World = IntegrationTest::GetGameWorld();
			return World && World->GetOutermost()->GetName() == MapPath && IsScreenActive<TScreen>() && !IsLoadingScreenUp();
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
	}

	void OpenTitle(FAutomationTestBase* Test)
	{
		OpenMenuMap<UTitleWidget>(Test, IntegrationTest::TitleMap, []() { UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*IntegrationTest::TitleMap)); });
	}

	/** 방장으로 로비를 연다. */
	void OpenHostedLobby(FAutomationTestBase* Test)
	{
		OpenMenuMap<ULobbyWidget>(Test, LobbyMap, []()
		{
			const UWorld* World = IntegrationTest::GetGameWorld();
			if (const UOnlineSessionsSubsystem* Sessions = World ? World->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr)
			{
				Sessions->TravelToListenMap(LobbyMap);
			}
		});
	}

	/** 타이틀 HUD에서 Luna의 몸 자세를 따르는 메시, 곧 옷 메시 */
	USkeletalMeshComponent* FindTitleLunaOutfitMesh()
	{
		const APlayerController* Controller = IntegrationTest::GetPlayerController();
		const AHUD* Hud = Controller ? Controller->GetHUD() : nullptr;
		const TInlineComponentArray<USkeletalMeshComponent*> Meshes(Hud);
		USkeletalMeshComponent* const* Outfit = Meshes.FindByPredicate([](const USkeletalMeshComponent* Mesh)
		{
			return Mesh->LeaderPoseComponent.IsValid();
		});
		return Outfit ? *Outfit : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdTitleMenuScreensTest, "LabProject.Integration.Screens.TitleMenus",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 타이틀에서 여는 게임 설정·상점·가이드·전적 화면이 뒤로 가기로 닫혀 타이틀로 돌아온다.
bool FPdTitleMenuScreensTest::RunTest(const FString& Parameters)
{
	OpenTitle(this);

	ExpectScreenClosesOnBack<UGameSettingsWidget>(this, TEXT("게임 설정"), [](UTitleWidget& Title) { Title.OpenGameSettings(); });

	// 설정을 연 화면이 직접 닫는 길(화면이 닫힐 때 함께 닫기)도 본다.
	IntegrationTest::Step(0.2f, []()
	{
		if (UTitleWidget* Title = FindActiveScreen<UTitleWidget>())
		{
			Title->OpenGameSettings();
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("게임 설정 다시 열기"), 10.f, []() { return IsScreenActive<UGameSettingsWidget>(); });
	IntegrationTest::Step(0.5f, [this]()
	{
		UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		TestTrue(TEXT("연 화면이 게임 설정을 닫는다"), Title && Title->CloseGameSettings());
	});
	IntegrationTest::WaitUntil(this, TEXT("게임 설정 닫기"), 5.f, []() { return !IsScreenActive<UGameSettingsWidget>(); });

	ExpectScreenClosesOnBack<UShopWidget>(this, TEXT("상점"), [](UTitleWidget& Title) { ClickTitleButton(Title, TEXT("HandlePandoraShopClicked")); });
	ExpectScreenClosesOnBack<UGuideWidget>(this, TEXT("가이드"), [](UTitleWidget& Title) { ClickTitleButton(Title, TEXT("HandleGuideClicked")); });
	ExpectScreenClosesOnBack<URecordWidget>(this, TEXT("전적"), [](UTitleWidget& Title) { ClickTitleButton(Title, TEXT("HandleRecordClicked")); });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdTitleMenuHoverTest, "LabProject.Integration.Screens.TitleMenuHover",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 판 없는 타이틀 메뉴는 마우스를 올린 줄만 강조하고, 마우스가 떠나면 강조를 거둔다.
bool FPdTitleMenuHoverTest::RunTest(const FString& Parameters)
{
	OpenTitle(this);

	const auto GetHoverProgress = [](const TCHAR* ButtonName)
	{
		const UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		return Title ? Title->GetMenuHoverProgress(Title->GetWidgetFromName(ButtonName)) : -1.f;
	};
	IntegrationTest::Step(0.2f, []()
	{
		const UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		if (const UWidget* RoomList = Title ? Title->GetWidgetFromName(TEXT("Btn_RoomList")) : nullptr)
		{
			const FGeometry& Geometry = RoomList->GetCachedGeometry();
			FSlateApplication::Get().SetCursorPos(Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f));
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("방 목록 강조"), 3.f, [GetHoverProgress]()
	{
		return GetHoverProgress(TEXT("Btn_RoomList")) >= 1.f;
	});
	IntegrationTest::Step(0.1f, [this, GetHoverProgress]()
	{
		TestTrue(TEXT("마우스가 없는 빠른 매칭은 강조하지 않는다"), FMath::IsNearlyZero(GetHoverProgress(TEXT("Btn_QuickMatch"))));
		FSlateApplication::Get().SetCursorPos(FVector2D::ZeroVector);
	});
	IntegrationTest::WaitUntil(this, TEXT("마우스가 떠난 방 목록의 강조 거두기"), 3.f, [GetHoverProgress]()
	{
		return FMath::IsNearlyZero(GetHoverProgress(TEXT("Btn_RoomList")));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaChatAvailabilityTest, "LabProject.Integration.Screens.LunaChatAvailability",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 질문 칸은 모델 서버가 준비됐을 때만 열리고, DLC 받기 버튼은 DLC가 없을 때만 보인다. 서버가 멈추면 칸이 닫히고 질문도 받지 않는다.
bool FPdLunaChatAvailabilityTest::RunTest(const FString& Parameters)
{
	OpenTitle(this);

	const auto GetLuna = []() { return IntegrationTest::GetGameWorld()->GetGameInstance()->GetSubsystem<ULunaChatSubsystem>(); };
	IntegrationTest::WaitUntil(this, TEXT("모델 서버 시작 끝내기"), 60.f, [GetLuna]()
	{
		return GetLuna()->GetServerState() != ELunaServerState::Starting;
	});
	IntegrationTest::Step(0.2f, [this, GetLuna]()
	{
		UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		const UEditableTextBox* Input = Title ? Cast<UEditableTextBox>(Title->GetWidgetFromName(TEXT("Input_LunaChat"))) : nullptr;
		const UWidget* DlcButton = Title ? Title->GetWidgetFromName(TEXT("Btn_LunaDlc")) : nullptr;
		if (!TestNotNull(TEXT("질문 칸"), Input) || !TestNotNull(TEXT("DLC 받기 버튼"), DlcButton))
		{
			return;
		}

		const auto ExpectWidgets = [this, Input, DlcButton](const FString& What, const bool bCanAsk, const bool bOfferDlc)
		{
			TestEqual(What + TEXT(": 질문 칸"), !Input->GetIsReadOnly(), bCanAsk);
			TestEqual(What + TEXT(": DLC 받기 버튼"), DlcButton->GetVisibility() == ESlateVisibility::Visible, bOfferDlc);
		};

		ULunaChatSubsystem* Luna = GetLuna();
		const ELunaServerState RealState = Luna->GetServerState();
		ExpectWidgets(TEXT("지금 서버 상태"), RealState == ELunaServerState::Ready, RealState == ELunaServerState::NotInstalled);

		Title->SetLunaServerState(ELunaServerState::NotInstalled);
		ExpectWidgets(TEXT("DLC 미설치"), false, true);
		Title->SetLunaServerState(ELunaServerState::Starting);
		ExpectWidgets(TEXT("준비 중"), false, false);
		Title->SetLunaServerState(ELunaServerState::Failed);
		ExpectWidgets(TEXT("실패"), false, false);
		Title->SetLunaServerState(ELunaServerState::Ready);
		ExpectWidgets(TEXT("준비 완료"), true, false);
		Title->SetLunaServerState(RealState);

		// 서버가 멈추면 타이틀 HUD가 칸을 닫고, 다시 띄우면 준비될 때까지 막아 둔다.
		Luna->ReleaseModel();
		TestFalse(TEXT("서버가 준비되지 않으면 질문을 받지 않는다"), Luna->Ask(TEXT("hello")));
		TestTrue(TEXT("서버가 멈추면 칸을 닫는다"), Input->GetIsReadOnly());
		Luna->PrepareModel();
		TestTrue(TEXT("다시 띄워도 준비될 때까지 칸을 막는다"), Input->GetIsReadOnly());
	});

	// DLC가 있으면 다시 띄운 서버로 질문이 실제로 가고, 답이 말풍선에 나온다.
	IntegrationTest::WaitUntil(this, TEXT("모델 서버 다시 띄우기"), 60.f, [GetLuna]()
	{
		return GetLuna()->GetServerState() != ELunaServerState::Starting;
	});
	IntegrationTest::Step(0.2f, [this, GetLuna]()
	{
		ULunaChatSubsystem* Luna = GetLuna();
		TestNotEqual(TEXT("DLC가 있으면 서버가 뜬다"), Luna->GetServerState(), ELunaServerState::Failed);
		if (Luna->GetServerState() == ELunaServerState::Ready)
		{
			TestTrue(TEXT("준비된 서버는 질문을 받는다"), Luna->Ask(TEXT("판도라는 어떻게 사용해?")));
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("루나의 답 기다리기"), 190.f, [GetLuna]() { return !GetLuna()->IsReplying(); });
	IntegrationTest::Step(0.2f, [this]()
	{
		const UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		const UTextBlock* Speech = Title ? Cast<UTextBlock>(Title->GetWidgetFromName(TEXT("Txt_LunaSpeech"))) : nullptr;
		if (TestNotNull(TEXT("말풍선"), Speech))
		{
			TestNotEqual(TEXT("모델 서버가 답한다"), Speech->GetText().ToString(),
				UMenuLocalizationSubsystem::GetText(TEXT("Title.LunaChatUnavailable")).ToString());
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdRoomAndLobbyScreensTest, "LabProject.Integration.Screens.RoomAndLobby",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 방 목록과, 방장으로 연 로비가 화면 층에 뜬다. 로딩 화면은 로비 화면이 뜬 것을 보고 내려간다.
// 대기실은 고른 맵의 정원만큼 자리를 두고, 사람이 없는 자리는 빈 자리로 남긴다(혼자 연 방이라 한 자리만 찬다).
bool FPdRoomAndLobbyScreensTest::RunTest(const FString& Parameters)
{
	OpenMenuMap<URoomListWidget>(this, RoomMap, []() { UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*RoomMap)); });
	OpenHostedLobby(this);
	IntegrationTest::Step(0.5f, [this]()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		const ALobbyGameState* LobbyState = World ? World->GetGameState<ALobbyGameState>() : nullptr;
		const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
		FLobbyMatchMapOption MapOption;
		if (!TestNotNull(TEXT("대기실 상태"), LobbyState)
			|| !TestTrue(TEXT("고른 맵이 맵 목록에 있다"), Levels && Levels->FindIngameLevel(LobbyState->GetSelectedMapKey(), MapOption)))
		{
			return;
		}
		int32 Seats = 0;
		int32 EmptySeats = 0;
		for (TObjectIterator<ULobbyUserWidget> It; It; ++It)
		{
			if (It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || It->GetWorld() != World || !It->IsVisible())
			{
				continue;
			}
			++Seats;
			const UWidget* EmptySeat = It->GetWidgetFromName(TEXT("EmptySeat"));
			EmptySeats += EmptySeat && EmptySeat->IsVisible() ? 1 : 0;
		}
		TestEqual(TEXT("고른 맵의 정원만큼 자리가 보인다"), Seats, MapOption.MaxPlayerCount);
		TestEqual(TEXT("나 말고는 빈 자리다"), EmptySeats, MapOption.MaxPlayerCount - 1);
	});
	OpenTitle(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLobbyStartTravelTest, "LabProject.Integration.Screens.LobbyStartTravel",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 방을 만들어 혼자 연 로비에서 시작하면 고른 경기 맵으로 seamless travel하고, 경기 화면이 뜬 것을 보고 로딩 화면이 내려간다.
// 이동 중에 로비 월드가 정리되면 화면 루트도 뷰포트에서 빠진다. 로딩 팝업은 새 루트에 다시 올라가야 하고,
// 빠진 루트를 다시 쓰면 팝업이 활성화되지 못한 채 같은 스택에 두 번 들어가 CommonUI ensure가 오류로 남는다.
bool FPdLobbyStartTravelTest::RunTest(const FString& Parameters)
{
	const auto GetSessions = []()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return World ? World->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
	};
	const auto IsSessionSettled = [GetSessions](const bool bExpectSession)
	{
		const UOnlineSessionsSubsystem* Sessions = GetSessions();
		return Sessions && Sessions->HasNamedSession() == bExpectSession && !Sessions->IsSessionLifecyclePending();
	};

	// 경기 시작은 방 세션을 시작한 뒤에 이동하므로, 방 만들기 팝업처럼 세션부터 만든다.
	IntegrationTest::Step(0.f, [this, GetSessions]()
	{
		UOnlineSessionsSubsystem* Sessions = GetSessions();
		const UWorld* World = IntegrationTest::GetGameWorld();
		TestTrue(TEXT("방 세션 만들기 시작"), Sessions && Sessions->BeginCreateRoomSession(
			World->GetFirstLocalPlayerFromController(), TEXT("Integration"), TEXT("Lobby"), PdGameSession::MaxPlayerCount, true) != 0);
	});
	IntegrationTest::WaitUntil(this, TEXT("방 세션 만들기"), 30.f, [IsSessionSettled]() { return IsSessionSettled(true); });
	OpenHostedLobby(this);
	IntegrationTest::WaitUntil(this, TEXT("방장 시작 조건"), 30.f, []()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		const ALobbyGameMode* Lobby = World ? World->GetAuthGameMode<ALobbyGameMode>() : nullptr;
		return Lobby && Lobby->CanHostStartGame();
	});

	TSharedRef<FString> MatchMap = MakeShared<FString>();
	IntegrationTest::Step(0.f, [this, MatchMap]()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		ALobbyGameMode* Lobby = World ? World->GetAuthGameMode<ALobbyGameMode>() : nullptr;
		const ALobbyGameState* LobbyState = World ? World->GetGameState<ALobbyGameState>() : nullptr;
		const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
		FLobbyMatchMapOption MapOption;
		if (!TestNotNull(TEXT("로비 게임 모드"), Lobby) || !TestNotNull(TEXT("대기실 상태"), LobbyState)
			|| !TestTrue(TEXT("고른 맵이 맵 목록에 있다"), Levels && Levels->FindIngameLevel(LobbyState->GetSelectedMapKey(), MapOption)))
		{
			return;
		}
		*MatchMap = MapOption.Map.ToSoftObjectPath().GetLongPackageName();
		Lobby->TryStartGame();
	});
	IntegrationTest::WaitUntil(this, TEXT("경기 맵에 도착해 로딩 화면 내리기"), 120.f, [MatchMap]()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		return MatchMap->IsEmpty()
			|| (World && World->GetOutermost()->GetName() == *MatchMap && !World->IsInSeamlessTravel() && !IsLoadingScreenUp());
	});

	// 다음 테스트가 새 방을 만들 수 있게 세션을 닫고 타이틀로 돌아간다.
	IntegrationTest::Step(0.5f, [GetSessions]()
	{
		if (UOnlineSessionsSubsystem* Sessions = GetSessions())
		{
			Sessions->DestroySession();
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("방 세션 닫기"), 30.f, [IsSessionSettled]() { return IsSessionSettled(false); });
	OpenTitle(this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdTitleLunaOutfitTest, "LabProject.Integration.Screens.TitleLunaOutfit",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 타이틀의 옷 버튼을 누르면 Luna가 몸의 자세를 따르는 옷 메시를 갈아입고, 타이틀을 다시 열어도 고른 옷으로 시작한다.
// 끝나면 처음 입고 있던 옷으로 되돌린다.
bool FPdTitleLunaOutfitTest::RunTest(const FString& Parameters)
{
	// 맵을 다시 열면 옷 메시 에셋도 다시 불러올 수 있어, 고른 옷은 객체가 아니라 경로로 기억한다.
	struct FOutfits
	{
		int32 InitialIndex = 0;
		FSoftObjectPath Picked;
	};
	TSharedRef<FOutfits> Outfits = MakeShared<FOutfits>();

	OpenTitle(this);
	IntegrationTest::Step(0.f, [this, Outfits]()
	{
		UTitleWidget* Title = FindActiveScreen<UTitleWidget>();
		USkeletalMeshComponent* Outfit = FindTitleLunaOutfitMesh();
		if (!TestNotNull(TEXT("타이틀"), Title) || !TestNotNull(TEXT("Luna의 몸 자세를 따르는 옷 메시"), Outfit))
		{
			return;
		}
		const USkeletalMesh* Initial = Outfit->GetSkeletalMeshAsset();
		Title->SelectLunaOutfit(0);
		const USkeletalMesh* First = Outfit->GetSkeletalMeshAsset();
		Title->SelectLunaOutfit(1);
		const USkeletalMesh* Second = Outfit->GetSkeletalMeshAsset();
		Outfits->Picked = FSoftObjectPath(Second);
		Outfits->InitialIndex = Initial == First ? 0 : 1;
		TestTrue(TEXT("처음부터 옷을 입고 있다"), Initial != nullptr);
		TestTrue(TEXT("다른 옷 버튼을 누르면 옷 메시가 바뀐다"), First && Second && First != Second);
	});
	OpenTitle(this);
	IntegrationTest::Step(0.f, [this, Outfits]()
	{
		const USkeletalMeshComponent* Outfit = FindTitleLunaOutfitMesh();
		TestEqual(TEXT("타이틀을 다시 열면 고른 옷으로 시작한다"),
			FSoftObjectPath(Outfit ? Outfit->GetSkeletalMeshAsset() : nullptr).ToString(), Outfits->Picked.ToString());
		if (UTitleWidget* Title = FindActiveScreen<UTitleWidget>())
		{
			Title->SelectLunaOutfit(Outfits->InitialIndex);
		}
	});
	return true;
}

namespace
{
	/** 콤보에서 줄 하나를 사람이 고른 것처럼 한다. 코드로 고른 줄(Direct)은 설정을 바꾸지 않으므로, 고른 뒤 처리기를 마우스 선택으로 부른다. */
	void PickOption(UUserWidget& Page, const TCHAR* ComboName, const TCHAR* Handler, const int32 Index)
	{
		UComboBoxString* Combo = Cast<UComboBoxString>(Page.GetWidgetFromName(ComboName));
		UFunction* Function = Page.FindFunction(Handler);
		if (!Combo || !Function)
		{
			return;
		}
		Combo->SetSelectedIndex(Index);
		struct
		{
			FString Option;
			TEnumAsByte<ESelectInfo::Type> SelectionType;
		} Params{Combo->GetSelectedOption(), ESelectInfo::OnMouseClick};
		Page.ProcessEvent(Function, &Params);
	}

	int32 OptionCount(const UUserWidget& Page, const TCHAR* ComboName)
	{
		const UComboBoxString* Combo = Cast<UComboBoxString>(Page.GetWidgetFromName(ComboName));
		return Combo ? Combo->GetOptionCount() : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdDisplaySettingsTest, "LabProject.Integration.Screens.DisplaySettings",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 게임 설정의 화면 탭: 처음 실행에 기본 프레임 제한이 걸리고, 탭을 누르면 화면 설정 페이지가 보이며, 품질·프레임 제한을 고르면 엔진 설정이 바뀐다.
// 화면 모드는 바꾸지 않는다(테스트 창 크기가 바뀐다). 끝나면 이 PC의 품질·프레임 제한을 원래대로 돌린다.
bool FPdDisplaySettingsTest::RunTest(const FString& Parameters)
{
	OpenTitle(this);
	IntegrationTest::Step(0.2f, []()
	{
		if (UTitleWidget* Title = FindActiveScreen<UTitleWidget>())
		{
			Title->OpenGameSettings();
		}
	});
	IntegrationTest::WaitUntil(this, TEXT("게임 설정 열기"), 10.f, []() { return IsScreenActive<UGameSettingsWidget>(); });
	IntegrationTest::Step(0.3f, [this]()
	{
		UGameSettingsWidget* Settings = FindActiveScreen<UGameSettingsWidget>();
		UDisplaySettingsSubsystem* Display = IntegrationTest::GetGameWorld()->GetGameInstance()->GetSubsystem<UDisplaySettingsSubsystem>();
		if (!TestNotNull(TEXT("게임 설정"), Settings) || !TestNotNull(TEXT("화면 설정 서브시스템"), Display))
		{
			return;
		}
		TestTrue(TEXT("처음 실행에 기본 프레임 제한을 한 번 맞췄다"), UDisplaySettingsSubsystem::HasAppliedDefaultFrameRateLimit());
		const UWidget* TrainingTab = Settings->GetWidgetFromName(TEXT("Btn_TabTrainingRoom"));
		TestTrue(TEXT("훈련장이 아니면 훈련장 탭은 숨는다"), TrainingTab && !TrainingTab->IsVisible());
		UButton* DisplayTab = Cast<UButton>(Settings->GetWidgetFromName(TEXT("Btn_TabDisplay")));
		if (!TestNotNull(TEXT("화면 탭"), DisplayTab))
		{
			return;
		}
		DisplayTab->OnClicked.Broadcast();
		const UWidgetSwitcher* Pages = Cast<UWidgetSwitcher>(Settings->GetWidgetFromName(TEXT("Switcher_SettingsPages")));
		UDisplaySettingsWidget* Page = Pages ? Cast<UDisplaySettingsWidget>(Pages->GetActiveWidget()) : nullptr;
		if (!TestNotNull(TEXT("화면 탭을 누르면 화면 설정 페이지가 보인다"), Page))
		{
			return;
		}
		TestEqual(TEXT("화면 모드"), OptionCount(*Page, TEXT("CB_WindowMode")), UDisplaySettingsSubsystem::GetWindowModes().Num());
		TestEqual(TEXT("그래픽 품질"), OptionCount(*Page, TEXT("CB_GraphicsQuality")), UDisplaySettingsSubsystem::QualityLevelCount);
		TestEqual(TEXT("프레임 제한"), OptionCount(*Page, TEXT("CB_FrameRateLimit")), UDisplaySettingsSubsystem::GetFrameRateLimits().Num());
		TestTrue(TEXT("해상도가 하나 이상 있다"), OptionCount(*Page, TEXT("CB_Resolution")) > 0);
		const UWidget* Resolution = Page->GetWidgetFromName(TEXT("CB_Resolution"));
		TestEqual(TEXT("창 전체 화면일 때만 해상도를 못 고른다"), Resolution && Resolution->GetIsEnabled(), Display->CanChooseResolution());

		const UComboBoxString* Quality = Cast<UComboBoxString>(Page->GetWidgetFromName(TEXT("CB_GraphicsQuality")));
		TestTrue(TEXT("그래픽 품질 칸은 늘 한 단계를 보여 준다(항목마다 단계가 달라도)"), Quality && Quality->GetSelectedIndex() != INDEX_NONE);

		UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
		const Scalability::FQualityLevels SavedQuality = UserSettings->ScalabilityQuality;
		const int32 SavedFrameRateLimit = Display->GetFrameRateLimit();
		PickOption(*Page, TEXT("CB_GraphicsQuality"), TEXT("HandleQualitySelected"), 0);
		PickOption(*Page, TEXT("CB_FrameRateLimit"), TEXT("HandleFrameRateLimitSelected"), 1);
		TestEqual(TEXT("낮음을 고르면 엔진 품질이 0단계다"), UserSettings->GetOverallScalabilityLevel(), 0);
		TestEqual(TEXT("두 번째 줄을 고르면 그 프레임 제한이 걸린다"), FMath::RoundToInt(UserSettings->GetFrameRateLimit()),
			UDisplaySettingsSubsystem::GetFrameRateLimits()[1]);
		UserSettings->ScalabilityQuality = SavedQuality;
		Display->SetFrameRateLimit(SavedFrameRateLimit);
	});
	return true;
}

#endif
