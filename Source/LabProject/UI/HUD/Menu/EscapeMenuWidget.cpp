#include "UI/HUD/Menu/EscapeMenuWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/Core/UiScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EscapeMenuWidget)

UEscapeMenuWidget::UEscapeMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	bIsBackHandler = true;
	bAutoRestoreFocus = true;
}

void UEscapeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ApplyMenuText();
	Btn_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResumeClicked);
	Btn_Exit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleExitClicked);

	if (bPauseGameWhenOpened)
	{
		SetRequestedPause(true);
	}
}

void UEscapeMenuWidget::NativeDestruct()
{
	ClearDestroySessionDelegate();
	Btn_Resume->OnClicked.RemoveDynamic(this, &ThisClass::HandleResumeClicked);
	Btn_Exit->OnClicked.RemoveDynamic(this, &ThisClass::HandleExitClicked);

	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}

	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UEscapeMenuWidget::GetDesiredInputConfig() const
{
	return UUiScreen::MakeBlockingInputConfig();
}

UWidget* UEscapeMenuWidget::NativeGetDesiredFocusTarget() const
{
	return Btn_Resume;
}

bool UEscapeMenuWidget::NativeOnHandleBackAction()
{
	CloseMenu();
	return true;
}

void UEscapeMenuWidget::CloseMenu()
{
	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}
	DeactivateWidget();
	OnMenuClosed.Broadcast(this);
}

void UEscapeMenuWidget::ExitToTitleMap()
{
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem =
		UGameInstance::GetSubsystem<UOnlineSessionsSubsystem>(GetGameInstance());
	if (OnlineSessionsSubsystem)
	{
		OnlineSessionsSubsystem->MarkVoluntaryMatchExit();
	}

	if (ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance()))
	{
		LobbySubsystem->ClearPendingTitleGameResult();
	}

	if (bAppliedPause)
	{
		SetRequestedPause(false);
	}

	if (APdPlayerController* PdPlayerController = Cast<APdPlayerController>(GetOwningPlayer());
		PdPlayerController && PdPlayerController->RequestExitMatchToTitle())
	{
		return;
	}

	if (bDestroySessionOnExit && OnlineSessionsSubsystem && OnlineSessionsSubsystem->HasNamedSession())
	{
		ClearDestroySessionDelegate();
		DestroySessionCompleteHandle = OnlineSessionsSubsystem->OnDestroySessionComplete.AddUObject(
			this, &ThisClass::HandleDestroySessionForExit);
		OnlineSessionsSubsystem->DestroySession();
		return;
	}

	TravelToTitleMap();
}

void UEscapeMenuWidget::HandleResumeClicked()
{
	CloseMenu();
}

void UEscapeMenuWidget::HandleExitClicked()
{
	ExitToTitleMap();
}

void UEscapeMenuWidget::HandleDestroySessionForExit(const bool bWasSuccessful)
{
	ClearDestroySessionDelegate();
	TravelToTitleMap();
}

// CommonUI 화면이라 ULocalizedMenuWidget을 쓸 수 없어, 같은 번역표에서 문구와 언어별 글꼴을 직접 가져온다.
// 메뉴가 열린 동안에는 언어를 바꿀 수 없으므로 열 때 한 번만 적용한다.
void UEscapeMenuWidget::ApplyMenuText() const
{
	const UMenuLocalizationSubsystem* Localization = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMenuLocalizationSubsystem>() : nullptr;
	if (!Localization)
	{
		return;
	}
	const TPair<UTextBlock*, FName> Entries[] = {
		{Txt_Title, TEXT("Escape.Title")}, {Txt_Resume, TEXT("Escape.Resume")}, {Txt_Exit, TEXT("Escape.Exit")}};
	for (const TPair<UTextBlock*, FName>& Entry : Entries)
	{
		if (Entry.Key)
		{
			Entry.Key->SetText(Localization->GetTextOrFallback(Entry.Value, Entry.Key->GetText()));
			Localization->ApplyLanguageFont(*Entry.Key);
		}
	}
}

void UEscapeMenuWidget::SetRequestedPause(const bool bPaused)
{
	UWorld* World = GetWorld();
	if (!World || (!bAllowNetworkPause && World->GetNetMode() != NM_Standalone))
	{
		return;
	}

	UGameplayStatics::SetGamePaused(this, bPaused);
	bAppliedPause = bPaused;
}

void UEscapeMenuWidget::TravelToTitleMap()
{
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	const FString TitleMapName = Definition ? Definition->GetTitleTravelMapName() : FString();
	if (!TitleMapName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*TitleMapName));
	}
}

void UEscapeMenuWidget::ClearDestroySessionDelegate()
{
	if (!DestroySessionCompleteHandle.IsValid())
	{
		return;
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem =
		UGameInstance::GetSubsystem<UOnlineSessionsSubsystem>(GetGameInstance()))
	{
		OnlineSessionsSubsystem->OnDestroySessionComplete.Remove(DestroySessionCompleteHandle);
	}
	DestroySessionCompleteHandle.Reset();
}
