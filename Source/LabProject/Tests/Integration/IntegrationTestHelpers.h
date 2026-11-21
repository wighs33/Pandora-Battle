#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Common/GameSessionConstants.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Tests/AutomationCommon.h"

/**
 * 연결 테스트가 함께 쓰는 지연 단계와 월드 조회.
 *
 * 게임 테스트는 -game 프로세스의 게임 월드 하나를 대상으로 하고, 테스트마다 맵을 새로 열어 앞 테스트의 상태를 넘겨받지 않는다.
 * 기다림은 고정 시간 대신 조건으로 걸고, 제한 시간이 지나면 무엇을 기다렸는지 오류로 남긴다.
 */
namespace IntegrationTest
{
	/** 여러 테스트 파일이 여는 맵. 익명 이름공간에 따로 두면 유니티 빌드로 묶일 때 이름이 겹친다. */
	inline const FString TitleMap = TEXT("/Game/Map/LV_Title");
	inline const FString LobbyMap = TEXT("/Game/Map/LV_Lobby");

	inline UWorld* GetGameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	inline APlayerController* GetPlayerController()
	{
		const UWorld* World = GetGameWorld();
		return World ? World->GetFirstPlayerController() : nullptr;
	}

	/** Delay초 기다린 뒤 Body를 한 번 실행한다. 단계는 넣은 순서대로 돈다. */
	inline void Step(const float Delay, TFunction<void()> Body)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(Delay));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Body = MoveTemp(Body)]()
		{
			Body();
			return true;
		}));
	}

	/** Condition이 참이 될 때까지 기다린다. Timeout초가 지나면 What을 오류로 남기고 다음 단계로 넘어간다. */
	inline void WaitUntil(FAutomationTestBase* Test, const FString& What, const float Timeout, TFunction<bool()> Condition)
	{
		TSharedRef<double> StartTime = MakeShared<double>(-1.0);
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, What, Timeout, StartTime, Condition = MoveTemp(Condition)]()
		{
			const double Now = FPlatformTime::Seconds();
			if (*StartTime < 0.0)
			{
				*StartTime = Now;
			}
			if (Condition())
			{
				return true;
			}
			if (Now - *StartTime > Timeout)
			{
				Test->AddError(FString::Printf(TEXT("%s: %.0f초 안에 되지 않았습니다."), *What, Timeout));
				return true;
			}
			return false;
		}));
	}

	/**
	 * 맵을 새로 열고 플레이어 Pawn과 HUD가 생긴 뒤, 콘텐츠 로딩이 끝나도록 SettleSeconds만큼 더 기다린다.
	 * Options는 travel 옵션이다(예: 보스 레이드 월드로 여는 BossRaid=1).
	 */
	inline void OpenMap(FAutomationTestBase* Test, const FString& MapPath, const float SettleSeconds = 3.f,
		const FString& Options = FString())
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([MapPath, Options]()
		{
			UGameplayStatics::OpenLevel(GetGameWorld(), FName(*MapPath), true, Options);
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
		WaitUntil(Test, MapPath + TEXT(" 열기"), 30.f, [MapPath]()
		{
			const UWorld* World = GetGameWorld();
			const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
			return World && World->HasBegunPlay() && World->GetOutermost()->GetName() == MapPath
				&& Controller && Controller->GetPawn() && Controller->GetHUD();
		});
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(SettleSeconds));
	}

	/**
	 * 실제 키 입력과 같은 경로로 키 이벤트를 보낸다. 게임 뷰포트(UCommonGameViewportClient)처럼 CommonUI 라우터가 먼저 받고,
	 * 라우터가 처리하지도 막지도 않은 키만 PlayerController(Enhanced Input)로 간다.
	 */
	inline void SendKeyEvent(const FKey& Key, const EInputEvent Event)
	{
		APlayerController* Controller = GetPlayerController();
		const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
		if (!LocalPlayer)
		{
			return;
		}

		const UCommonUIActionRouterBase* Router = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>();
		if (Router && Router->ProcessInput(Key, Event) != ERouteUIInputResult::Unhandled)
		{
			return;
		}
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Released ? 0.f : 1.f));
	}

	/** 키를 누르거나 뗀다. 떼기는 다음 단계에서 한다. */
	inline void PressKey(const FKey& Key, const bool bPressed)
	{
		SendKeyEvent(Key, bPressed ? IE_Pressed : IE_Released);
	}

	inline UOnlineSessionsSubsystem* GetSessions()
	{
		const UWorld* World = GetGameWorld();
		return World ? World->GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
	}

	/** 방 세션이 있는지(bExpectSession) 또는 없는지가 정해졌고, 만들거나 닫는 중이 아니다. */
	inline bool IsRoomSessionSettled(const bool bExpectSession)
	{
		const UOnlineSessionsSubsystem* Sessions = GetSessions();
		return Sessions && Sessions->HasNamedSession() == bExpectSession && !Sessions->IsSessionLifecyclePending();
	}

	/** 방 만들기 팝업처럼 방 세션부터 만든다. 대기실의 경기 시작은 방 세션을 시작한 뒤에 이동한다. */
	inline void CreateRoomSession(FAutomationTestBase* Test)
	{
		Step(0.f, [Test]()
		{
			UOnlineSessionsSubsystem* Sessions = GetSessions();
			const UWorld* World = GetGameWorld();
			Test->TestTrue(TEXT("방 세션 만들기 시작"), Sessions && Sessions->BeginCreateRoomSession(
				World->GetFirstLocalPlayerFromController(), TEXT("Integration"), TEXT("Lobby"), PdGameSession::MaxPlayerCount, true) != 0);
		});
		WaitUntil(Test, TEXT("방 세션 만들기"), 30.f, []() { return IsRoomSessionSettled(true); });
	}

	/** 다음 테스트가 새 방을 만들 수 있게 방 세션을 닫는다. */
	inline void DestroyRoomSession(FAutomationTestBase* Test)
	{
		Step(0.5f, []()
		{
			if (UOnlineSessionsSubsystem* Sessions = GetSessions())
			{
				Sessions->DestroySession();
			}
		});
		WaitUntil(Test, TEXT("방 세션 닫기"), 30.f, []() { return IsRoomSessionSettled(false); });
	}
}

#endif
