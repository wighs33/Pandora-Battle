#include "UI/Core/PdUIActionRouter.h"

#include "Common/PdTicker.h"
#include "Component/Player/ControllerInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "UI/Core/GameplayScreen.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdUIActionRouter)

void UPdUIActionRouter::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	FTSTicker::RemoveTicker(PendingInputConfigTickerHandle);
	PendingInputConfigTickerHandle.Reset();
	Super::PlayerControllerChanged(NewPlayerController);
	PressedKeys.Reset();
	KeysAwaitingRelease.Reset();
	ChatInputWidget.Reset();
	AppliedGameplayPolicy = EPdGameplayInputPolicy::Allow;
	ActiveInputConfig.Reset();
	if (NewPlayerController)
	{
		ApplyDefaultInput();
	}
}

void UPdUIActionRouter::Deinitialize()
{
	FTSTicker::RemoveTicker(PendingInputConfigTickerHandle);
	PendingInputConfigTickerHandle.Reset();
	Super::Deinitialize();
}

void UPdUIActionRouter::BeginChatInput(UWidget* InputWidget)
{
	ChatInputWidget = InputWidget;
	if (IsDefaultScreenActive())
	{
		ApplyDefaultInput();
	}
}

void UPdUIActionRouter::EndChatInput(const UWidget* InputWidget)
{
	if (ChatInputWidget.Get() != InputWidget)
	{
		return;
	}
	ChatInputWidget.Reset();
	if (IsDefaultScreenActive())
	{
		ApplyDefaultInput();
	}
}

void UPdUIActionRouter::SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot)
{
	Super::SetActiveRoot(NewActiveRoot);
	// UE 5.8은 Slate root 제거를 마친 뒤 ActiveInputConfig를 비운다.
	// 그 정리가 끝난 다음 틱에 기본 정책을 복원한다. 게임 중 기본 화면으로 돌아온 때도 채팅 포커스를 되돌리려고 같은 길을 쓴다.
	PdTicker::RunNextTick(this, [this]()
	{
		if (IsDefaultScreenActive())
		{
			ApplyDefaultInput();
		}
	});
}

FUIInputConfig UPdUIActionRouter::MakeDefaultInputConfig() const
{
	// 게임 중 기본 화면의 뒤로 가기(ESC)는 Menu 모드 바인딩이라, 게임 입력과 함께 받는 All 모드를 쓴다.
	const bool bChatInput = ChatInputWidget.IsValid();
	FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = bChatInput;
	return Config;
}

void UPdUIActionRouter::ApplyDefaultInput()
{
	ApplyUIInputConfig(MakeDefaultInputConfig(), true);
	if (UWidget* ChatInput = ChatInputWidget.Get())
	{
		GetLocalPlayerChecked()->GetSlateOperations().SetUserFocus(ChatInput->TakeWidget());
	}
}

bool UPdUIActionRouter::IsDefaultScreenActive() const
{
	const UCommonActivatableWidget* ActiveScreen = GetLeafmostActivatableWidget();
	return !ActiveScreen || ActiveScreen->IsA<UGameplayScreen>();
}

bool UPdUIActionRouter::IsGameplayInputBlocked() const
{
	return AppliedGameplayPolicy == EPdGameplayInputPolicy::Block;
}

void UPdUIActionRouter::ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh)
{
	FTSTicker::RemoveTicker(PendingInputConfigTickerHandle);
	PendingInputConfigTickerHandle.Reset();

	APlayerController* Controller = GetLocalPlayerChecked()->GetPlayerController(GetWorld());
	if (!IsValid(Controller) || Controller->IsActorBeingDestroyed())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetLocalPlayerChecked()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem && !InputSubsystem->GetPlayerInput())
	{
		// 접속/Travel 중에는 Controller가 먼저 연결된다. 입력 객체가 준비되면 최신 요청만 적용한다.
		const TWeakObjectPtr<APlayerController> PendingController = Controller;
		PendingInputConfigTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this, PendingController, NewConfig](float)
		{
			APlayerController* CurrentController = GetLocalPlayerChecked()->GetPlayerController(GetWorld());
			if (PendingController.IsValid() && PendingController.Get() == CurrentController && !CurrentController->IsActorBeingDestroyed())
			{
				const UEnhancedInputLocalPlayerSubsystem* Input = GetLocalPlayerChecked()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				if (Input && !Input->GetPlayerInput())
				{
					return true;
				}

				PendingInputConfigTickerHandle.Reset();
				ApplyUIInputConfig(NewConfig, true);
			}
			PendingInputConfigTickerHandle.Reset();
			return false;
		}));
		return;
	}

	const bool bWasBlocked = IsGameplayInputBlocked();
	// 게임 중 기본 화면 위에 다른 화면이 있거나 채팅을 입력하는 동안에는 게임 입력을 막는다.
	if (!IsDefaultScreenActive() || ChatInputWidget.IsValid())
	{
		AppliedGameplayPolicy = EPdGameplayInputPolicy::Block;
	}
	else
	{
		AppliedGameplayPolicy = EPdGameplayInputPolicy::Allow;
	}
	const bool bWillBlock = IsGameplayInputBlocked();
	if (bWasBlocked != bWillBlock)
	{
		KeysAwaitingRelease.Append(PressedKeys);
	}
	if (bWasBlocked && !bWillBlock && Controller)
	{
		// 차단 해제 때만 정리한다. Allow 정책 재적용은 누른 입력을 유지한다.
		Controller->FlushPressedKeys();
	}
	if (!bWasBlocked && bWillBlock && Controller)
	{
		if (UControllerInputComponent* Input = Controller->FindComponentByClass<UControllerInputComponent>())
		{
			Input->ReleaseGameplayInput();
		}
	}
	if (NewConfig.GetInputMode() == ECommonInputMode::Menu && GetActiveInputMode() != ECommonInputMode::Menu && Controller)
	{
		if (UControllerInputComponent* Input = Controller->FindComponentByClass<UControllerInputComponent>())
		{
			Input->ReleaseHeldUiInput();
		}
	}
	Super::ApplyUIInputConfig(NewConfig, bForceRefresh);
	if (Controller)
	{
		// CommonUI가 입력을 분배하므로 이전 UIOnly의 전역 Viewport 차단은 사용하지 않는다.
		if (UGameViewportClient* Viewport = GetLocalPlayerChecked()->ViewportClient)
		{
			Viewport->SetIgnoreInput(false);
		}
		const bool bShowCursor = NewConfig.GetMouseCaptureMode() == EMouseCaptureMode::NoCapture;
		Controller->SetShowMouseCursor(bShowCursor);
		Controller->bEnableClickEvents = bShowCursor;
		Controller->bEnableMouseOverEvents = bShowCursor;
	}
}

ERouteUIInputResult UPdUIActionRouter::ProcessInput(FKey Key, EInputEvent InputEvent) const
{
	if (InputEvent == IE_Pressed)
	{
		PressedKeys.Add(Key);
	}
	if (InputEvent == IE_Released)
	{
		PressedKeys.Remove(Key);
		KeysAwaitingRelease.Remove(Key);
	}
	const ERouteUIInputResult Result = Super::ProcessInput(Key, InputEvent);
	// UI를 여닫기 전에 누른 키는 실제 Release 전까지 게임으로 다시 전달하지 않는다.
	return Result == ERouteUIInputResult::Unhandled && KeysAwaitingRelease.Contains(Key)
		? ERouteUIInputResult::BlockGameInput : Result;
}
