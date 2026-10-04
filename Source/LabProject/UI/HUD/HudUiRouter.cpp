#include "UI/HUD/HudUiRouter.h"
#include "UI/HUD/HudMenuLayer.h"
#include "UI/HUD/HudScoreboardLayer.h"
#include "UI/HUD/HudScreenLayer.h"
#include "UI/HUD/HudSelectPandoraLayer.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/PdPlayer.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Match/GameResultWidget.h"
#include "UI/HUD/PdHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/Info/Presenter/InfoUiPresenter.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Pandora/PandoraTreeWidget.h"
#include "UI/HUD/Player/PlayerHudWidget.h"
#include "UI/HUD/Notification/RightNotificationsWidget.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudUiRouter)

void UHudUiRouter::Initialize(APdHUD* InOwnerHud)
{
	if (!IsValid(InOwnerHud) || OwnerHud.Get() == InOwnerHud)
	{
		return;
	}

	Shutdown();
	OwnerHud = InOwnerHud;

	MenuLayer = NewObject<UHudMenuLayer>(this);
	MenuLayer->Initialize(InOwnerHud, this);

	ScreenLayer = NewObject<UHudScreenLayer>(this);
	ScreenLayer->Initialize(InOwnerHud, this);

	ScoreboardLayer = NewObject<UHudScoreboardLayer>(this);
	ScoreboardLayer->Initialize(InOwnerHud, this);

	SelectPandoraLayer = NewObject<UHudSelectPandoraLayer>(this);
	SelectPandoraLayer->Initialize(InOwnerHud);
}

void UHudUiRouter::Shutdown()
{
	ResetLayers();

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.Reset();
	ActiveDefinition = nullptr;
	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->WidgetClassDefinition = nullptr;
	}
	if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
	{
		UiSubsystem->ClearWidgetClassDefinition(PreviousDefinition);
	}

	MenuLayer = nullptr;
	ScreenLayer = nullptr;
	ScoreboardLayer = nullptr;
	SelectPandoraLayer = nullptr;
	OwnerHud.Reset();
}

bool UHudUiRouter::AddDefinitionRequest(UWidgetClassDefinition* Definition)
{
	if (!IsValid(Definition) || !OwnerHud.IsValid())
	{
		return false;
	}

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.Add(Definition);
	ApplyActiveDefinition(Definition);
	return PreviousDefinition != ActiveDefinition;
}

bool UHudUiRouter::RemoveDefinitionRequest(const UWidgetClassDefinition* Definition)
{
	if (!Definition)
	{
		return false;
	}

	const int32 RequestIndex = DefinitionRequests.FindLastByPredicate(
		[Definition](const TObjectPtr<UWidgetClassDefinition>& Request)
		{
			return Request == Definition;
		});
	if (RequestIndex == INDEX_NONE)
	{
		return false;
	}

	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	DefinitionRequests.RemoveAt(RequestIndex);
	UWidgetClassDefinition* NewDefinition =
		DefinitionRequests.IsEmpty() ? nullptr : DefinitionRequests.Last().Get();
	ApplyActiveDefinition(NewDefinition);
	return PreviousDefinition != ActiveDefinition;
}

void UHudUiRouter::EnsureCoreLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	UWidgetClassDefinition* Definition = ActiveDefinition;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition)
	{
		return;
	}

	if (bEnsuringCoreLayers)
	{
		return;
	}

	TGuardValue<bool> EnsureCoreLayersGuard(bEnsuringCoreLayers, true);

	Hud->RefreshUiBindings();

	if (!Hud->CachedPlayerHUD)
	{
		if (const TSubclassOf<UUserWidget> WidgetClass = Definition->GetPlayerHudWidgetClass())
		{
			Hud->CachedPlayerHUD = CreateWidget<UUserWidget>(Controller, WidgetClass);
			if (UPlayerHudWidget* PlayerHudWidget = Cast<UPlayerHudWidget>(Hud->CachedPlayerHUD))
			{
				PlayerHudWidget->InitializePlayerHud(Definition);
			}
		}
	}
	if (Hud->CachedPlayerHUD && !Hud->CachedPlayerHUD->IsInViewport())
	{
		Hud->CachedPlayerHUD->AddToViewport();
	}
	// 파생 HUD가 전체 화면 UI(예: 로비 위젯)를 이미 연 뒤에 Core 층이 들어올 수 있다.
	// 새로 추가한 HUD가 보인다고 가정하지 말고, 만든 뒤에는 항상 현재 정책을 적용한다.
	Hud->RefreshPlayerHudVisibility();
	Hud->ApplyStatusViewModelToPlayerHud();
	Hud->RefreshHudTimerVisibility();

	if (SelectPandoraLayer)
	{
		SelectPandoraLayer->EnsureWidget(*Controller, *Definition);
	}
	if (!Hud->AimCrosshairWidget)
	{
		if (const TSubclassOf<UUserWidget> WidgetClass = Definition->GetAimCrosshairWidgetClass())
		{
			Hud->AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->CachedRightNotificationsUI)
	{
		if (const TSubclassOf<URightNotificationsWidget> WidgetClass =
			Definition->GetRightNotificationsWidgetClass())
		{
			Hud->CachedRightNotificationsUI =
				CreateWidget<URightNotificationsWidget>(Controller, WidgetClass);
		}
	}
	if (Hud->CachedRightNotificationsUI && !Hud->CachedRightNotificationsUI->IsInViewport())
	{
		Hud->CachedRightNotificationsUI->AddToViewport(20);
	}

	if (SelectPandoraLayer)
	{
		SelectPandoraLayer->BindPresenter();
	}
}

void UHudUiRouter::EnsureInfoLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	UWidgetClassDefinition* Definition = ActiveDefinition;
	if (!Hud || !Controller || !Controller->IsLocalController() || !Definition
		|| bEnsuringInfoLayers)
	{
		return;
	}

	TGuardValue<bool> EnsureInfoLayersGuard(bEnsuringInfoLayers, true);
	if (!Hud->CachedInfoUI)
	{
		if (const TSubclassOf<UInfoWidget> WidgetClass = Definition->GetInfoWidgetClass())
		{
			Hud->CachedInfoUI = CreateWidget<UInfoWidget>(Controller, WidgetClass);
		}
	}
	if (!Hud->CachedPandoraTreeUI)
	{
		if (const TSubclassOf<UPandoraTreeWidget> WidgetClass =
			Definition->GetPandoraTreeWidgetClass())
		{
			Hud->CachedPandoraTreeUI =
				CreateWidget<UPandoraTreeWidget>(Controller, WidgetClass);
		}
	}

	if (Hud->CachedInfoUI)
	{
		if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
		{
			Presenter->BindInfoUi(Hud->CachedInfoUI);
			Hud->CachedInfoUI->OnClickedInfoCenterButton.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
			Hud->CachedInfoUI->OnClickedInfoCenterButton.AddUniqueDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
		}

		Hud->ApplyInventoryWidgetSettings();
	}
}

void UHudUiRouter::ReleaseInfoLayers()
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}
	if (Hud->CachedInfoUI)
	{
		if (UInfoUiPresenter* Presenter = Hud->CachedInfoUiPresenter)
		{
			Hud->CachedInfoUI->OnClickedInfoCenterButton.RemoveDynamic(
				Presenter,
				&UInfoUiPresenter::HandleClickedInfoCenterButton);
			Presenter->UnbindInfoUi(Hud->CachedInfoUI);
		}
		Hud->CachedInfoUI->SetReturnCameraOnHide(true);
		Hud->CachedInfoUI->RemoveFromParent();
		Hud->CachedInfoUI = nullptr;
	}
	if (Hud->CachedPandoraTreeUI)
	{
		Hud->CachedPandoraTreeUI->SetReturnCameraOnHide(true);
		Hud->CachedPandoraTreeUI->RemoveFromParent();
		Hud->CachedPandoraTreeUI = nullptr;
	}
}

void UHudUiRouter::ResetLayers()
{
	if (ScreenLayer)
	{
		ScreenLayer->Shutdown();
	}
	if (MenuLayer)
	{
		MenuLayer->Shutdown();
	}
	if (ScoreboardLayer)
	{
		ScoreboardLayer->Shutdown();
	}

	APdHUD* Hud = OwnerHud.Get();
	if (!Hud)
	{
		return;
	}

	Hud->CloseSelectPandoraUiInternal(false);
	HideAimCrosshair();
	Hud->AimCrosshairWidget = nullptr;

	if (Hud->CachedPlayerHUD)
	{
		Hud->CachedPlayerHUD->RemoveFromParent();
		Hud->CachedPlayerHUD = nullptr;
	}
	ReleaseInfoLayers();
	if (SelectPandoraLayer)
	{
		SelectPandoraLayer->ReleaseWidget();
	}
	if (Hud->CachedRightNotificationsUI)
	{
		Hud->CachedRightNotificationsUI->RemoveFromParent();
		Hud->CachedRightNotificationsUI = nullptr;
	}
}

void UHudUiRouter::ShowAimCrosshair(const FGameplayTag DesiredCrosshairWidgetTag)
{
	APdHUD* Hud = OwnerHud.Get();
	APdPlayerController* Controller = ResolvePlayerController();
	if (!Hud || !Controller || !ActiveDefinition)
	{
		return;
	}

	TSubclassOf<UUserWidget> DesiredWidgetClass =
		ActiveDefinition->FindWidgetClassByTag(DesiredCrosshairWidgetTag);
	if (!DesiredWidgetClass)
	{
		DesiredWidgetClass = ActiveDefinition->GetAimCrosshairWidgetClass();
	}
	if (!DesiredWidgetClass)
	{
		return;
	}

	if (!Hud->AimCrosshairWidget || Hud->AimCrosshairWidget->GetClass() != DesiredWidgetClass)
	{
		HideAimCrosshair();
		Hud->AimCrosshairWidget = CreateWidget<UUserWidget>(Controller, DesiredWidgetClass);
	}
	if (Hud->AimCrosshairWidget && !Hud->AimCrosshairWidget->IsInViewport())
	{
		Hud->AimCrosshairWidget->AddToViewport();
	}
}

void UHudUiRouter::HideAimCrosshair()
{
	if (APdHUD* Hud = OwnerHud.Get())
	{
		if (Hud->AimCrosshairWidget)
		{
			Hud->AimCrosshairWidget->RemoveFromParent();
		}
	}
}

void UHudUiRouter::ApplyActiveDefinition(UWidgetClassDefinition* NewDefinition)
{
	UWidgetClassDefinition* PreviousDefinition = ActiveDefinition;
	ActiveDefinition = NewDefinition;

	if (APdHUD* Hud = OwnerHud.Get())
	{
		Hud->WidgetClassDefinition = ActiveDefinition;
	}

	if (UUiSubsystem* UiSubsystem = ResolveUiSubsystem())
	{
		if (ActiveDefinition)
		{
			UiSubsystem->SetWidgetClassDefinition(ActiveDefinition);
		}
		else
		{
			UiSubsystem->ClearWidgetClassDefinition(PreviousDefinition);
		}
	}
}

UUiSubsystem* UHudUiRouter::ResolveUiSubsystem() const
{
	const APdPlayerController* Controller = ResolvePlayerController();
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

APdPlayerController* UHudUiRouter::ResolvePlayerController() const
{
	const APdHUD* Hud = OwnerHud.Get();
	return Hud ? Cast<APdPlayerController>(Hud->GetOwningPlayerController()) : nullptr;
}

