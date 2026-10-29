#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Online/OnlineSessionsSubsystem.h"
#include "UI/Core/LoadingScreenSubsystem.h"
#include "UI/Guide/GuideWidget.h"
#include "UI/Lobby/LobbyWidget.h"
#include "UI/Record/RecordWidget.h"
#include "UI/Room/RoomListWidget.h"
#include "UI/Settings/GameSettingsWidget.h"
#include "UI/Shop/ShopWidget.h"
#include "UI/Title/TitleWidget.h"
#include "UObject/UObjectIterator.h"

// 화면(UPdScreenWidget)이 CommonUI 화면 층에 올라가고, 뒤로 가기(ESC)로 닫히며, 로딩 화면이 화면이 뜬 것을 보고 내려가는지 본다.

namespace
{
	const FString TitleMap = TEXT("/Game/Map/LV_Title");
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
		OpenMenuMap<UTitleWidget>(Test, TitleMap, []() { UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*TitleMap)); });
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdRoomAndLobbyScreensTest, "LabProject.Integration.Screens.RoomAndLobby",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 방 목록과, 방장으로 연 로비가 화면 층에 뜬다. 로딩 화면은 로비 화면이 뜬 것을 보고 내려간다.
bool FPdRoomAndLobbyScreensTest::RunTest(const FString& Parameters)
{
	OpenMenuMap<URoomListWidget>(this, RoomMap, []() { UGameplayStatics::OpenLevel(IntegrationTest::GetGameWorld(), FName(*RoomMap)); });
	OpenMenuMap<ULobbyWidget>(this, LobbyMap, []()
	{
		const UWorld* World = IntegrationTest::GetGameWorld();
		if (const UOnlineSessionsSubsystem* Sessions = World ? World->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr)
		{
			Sessions->TravelToListenMap(LobbyMap);
		}
	});
	OpenTitle(this);
	return true;
}

#endif
