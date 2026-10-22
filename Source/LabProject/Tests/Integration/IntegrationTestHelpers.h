#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

/**
 * 연결 테스트가 함께 쓰는 지연 단계와 월드 조회.
 *
 * 게임 테스트는 -game 프로세스의 게임 월드 하나를 대상으로 하고, 테스트마다 맵을 새로 열어 앞 테스트의 상태를 넘겨받지 않는다.
 * 기다림은 고정 시간 대신 조건으로 걸고, 제한 시간이 지나면 무엇을 기다렸는지 오류로 남긴다.
 */
namespace PdIntegrationTest
{
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
}

#endif
