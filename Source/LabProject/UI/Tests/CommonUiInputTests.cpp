#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/WorldSettings.h"
#include "UI/PdUIActionRouter.h"
#include "UI/UiSubsystem.h"
#include "UI/UiScreen.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "UI/Widget/GuideWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Widget/GameSettingsWidget.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Components/PanelWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Button.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPdCommonUiMenuTest, "Pandora.UI.CommonUI",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPdCommonUiMenuTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    Names.Add(TEXT("SettingsOverLegacyInfo"));
    Commands.Add(TEXT("Legacy"));
    Names.Add(TEXT("SettingsOverCommonInfo"));
    Commands.Add(TEXT("Common"));
}

bool FPdCommonUiMenuTest::RunTest(const FString& Parameters)
{
    const bool bCommonInfo = Parameters == TEXT("Common");
    UWorld* Map = FAutomationEditorCommonUtils::CreateNewMap();
    Map->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

    struct FState
    {
        TStrongObjectPtr<UInfoWidget> Info;
        TStrongObjectPtr<UUiScreen> InfoScreen;
        TStrongObjectPtr<UMenuPopupWidget> Menu;
        FGuid LegacyToken;
        UPdUIActionRouter* Router = nullptr;
        UUiSubsystem* Ui = nullptr;
        APlayerController* Controller = nullptr;
        double PolicyWaitStarted = 0.0;
    };
    const TSharedRef<FState> State = MakeShared<FState>();
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, bCommonInfo]()
    {
        UWorld* World = GEditor->PlayWorld;
        if (!TestNotNull(TEXT("PIE world"), World)) return true;
        State->Controller = World->GetFirstPlayerController();
        ULocalPlayer* Player = State->Controller ? State->Controller->GetLocalPlayer() : nullptr;
        if (!TestNotNull(TEXT("Local player"), Player)) return true;
        State->Router = Player->GetSubsystem<UPdUIActionRouter>();
        State->Ui = Player->GetSubsystem<UUiSubsystem>();
        if (!TestNotNull(TEXT("CommonUI action router"), State->Router) || !TestNotNull(TEXT("UI subsystem"), State->Ui)) return true;
        State->Ui->HideTravelLoadingScreen();
        // An automated PIE restart must focus this client's viewport, just like a user click.
        FSlateApplication::Get().SetUserFocus(State->Router->GetLocalPlayerIndex(), Player->ViewportClient->GetGameViewportWidget());
        State->Router->SetFallbackInput(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently), nullptr, false);
        TestFalse(TEXT("Normal gameplay allows input"), State->Router->IsGameplayInputBlocked());
        State->Router->ProcessInput(EKeys::W, IE_Pressed);

        UClass* InfoClass = LoadClass<UInfoWidget>(nullptr, TEXT("/Game/UI/Widget/WBP_Info.WBP_Info_C"));
        UClass* MenuClass = LoadClass<UMenuPopupWidget>(nullptr, TEXT("/Game/UI/Widget/Game/WBP_MenuPopup.WBP_MenuPopup_C"));
        if (!TestNotNull(TEXT("Info Blueprint"), InfoClass) || !TestNotNull(TEXT("Menu Blueprint"), MenuClass)) return true;
        State->Info.Reset(CreateWidget<UInfoWidget>(State->Controller, InfoClass));
        if (bCommonInfo)
        {
            State->InfoScreen.Reset(CreateWidget<UUiScreen>(State->Controller));
            FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
            Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
            State->InfoScreen->SetContent(State->Info.Get(), Config, State->Info.Get(), FSimpleDelegate());
            State->InfoScreen->AddToPlayerScreen();
            State->InfoScreen->ActivateWidget();
        }
        else
        {
            State->Info->AddToPlayerScreen();
            FUiModalInputConfig LegacyConfig;
            LegacyConfig.RestorePolicy = EUiInputRestorePolicy::Gameplay;
            State->LegacyToken = State->Ui->AcquireModalInput(State->Info.Get(), State->Info.Get(), LegacyConfig);
            TestTrue(TEXT("Legacy Info blocks gameplay"), State->Router->IsGameplayInputBlocked());
        }
        State->Menu.Reset(CreateWidget<UMenuPopupWidget>(State->Controller, MenuClass));
        State->Ui->PushScreen(State->Menu.Get());
        State->PolicyWaitStarted = FPlatformTime::Seconds();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        // Slate paints the new root before CommonUI selects its input policy.
        if ((State->Router->GetLeafmostActivatableWidget() != State->Menu.Get()
            || State->Router->GetActiveInputMode() != ECommonInputMode::Menu)
            && FPlatformTime::Seconds() - State->PolicyWaitStarted < 5.0) return false;
        TestTrue(TEXT("Settings activated"), State->Menu->IsActivated());
        TestTrue(TEXT("Info remains visible"), State->Info->IsInViewport() || State->InfoScreen.IsValid());
        TestEqual(TEXT("Settings owns input"), State->Router->GetLeafmostActivatableWidget(), static_cast<UCommonActivatableWidget*>(State->Menu.Get()));
        TestEqual(TEXT("Settings uses Menu input"), State->Router->GetActiveInputMode(), ECommonInputMode::Menu);
        TestTrue(TEXT("Settings blocks gameplay"), State->Router->IsGameplayInputBlocked());
        TestTrue(TEXT("Settings shows cursor"), State->Controller->bShowMouseCursor);
        UWidget* FocusTarget = State->Menu->GetDesiredFocusTarget();
        TestTrue(TEXT("Resume button binding preserved"), FocusTarget && FocusTarget->IsA<UButton>());
        TestTrue(TEXT("Settings has focus"), FocusTarget && FocusTarget->HasUserFocus(State->Controller));
        TestEqual(TEXT("CommonUI handles Escape"), State->Router->ProcessInput(EKeys::Escape, IE_Pressed), ERouteUIInputResult::Handled);
        State->Router->ProcessInput(EKeys::Escape, IE_Released);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        TestFalse(TEXT("Settings deactivated"), State->Menu->IsActivated());
        TestTrue(TEXT("Info still blocks gameplay after settings"), State->Router->IsGameplayInputBlocked());
        TestEqual(TEXT("Legacy Info policy restored"), State->Router->GetActiveInputMode(), ECommonInputMode::All);
        if (State->InfoScreen.IsValid())
        {
            TestEqual(TEXT("Info regains the active leaf"), State->Router->GetLeafmostActivatableWidget(), static_cast<UCommonActivatableWidget*>(State->InfoScreen.Get()));
            TestTrue(TEXT("Info regains focus"), State->Info->HasUserFocus(State->Controller));
        }
        State->Router->ProcessInput(EKeys::W, IE_Pressed);
        State->Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W, IE_Pressed, 1.0f));
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.25f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        TestTrue(TEXT("Held key reached PlayerInput during All mode"), State->Controller->IsInputKeyDown(EKeys::W));
        if (State->InfoScreen.IsValid())
        {
            State->InfoScreen->DeactivateWidget();
            State->InfoScreen->RemoveFromParent();
        }
        else State->Ui->ReleaseModalInput(State->Info.Get(), State->LegacyToken);
        State->Info->RemoveFromParent();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        TestFalse(TEXT("Last screen restores gameplay"), State->Router->IsGameplayInputBlocked());
        TestFalse(TEXT("Last screen hides cursor"), State->Controller->bShowMouseCursor);
        TestFalse(TEXT("PlayerInput held movement cleared"), State->Controller->IsInputKeyDown(EKeys::W));
        TestFalse(TEXT("Movement lock released"), State->Controller->IsMoveInputIgnored());
        TestFalse(TEXT("Look lock released"), State->Controller->IsLookInputIgnored());
        TestEqual(TEXT("Held movement waits for release"), State->Router->ProcessInput(EKeys::W, IE_Repeat), ERouteUIInputResult::BlockGameInput);
        State->Router->ProcessInput(EKeys::W, IE_Released);
        TestEqual(TEXT("New movement press is delivered"), State->Router->ProcessInput(EKeys::W, IE_Pressed), ERouteUIInputResult::Unhandled);
        State->Router->ProcessInput(EKeys::W, IE_Released);
        State->Menu.Reset();
        State->Info.Reset();
        State->InfoScreen.Reset();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCommonUiLayersTest, "Pandora.UI.CommonUI.LayeredMenus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPdCommonUiLayersTest::RunTest(const FString& Parameters)
{
    UWorld* Map = FAutomationEditorCommonUtils::CreateNewMap();
    Map->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    struct FState
    {
        TStrongObjectPtr<UMenuPopupWidget> Menu;
        TStrongObjectPtr<UGuideWidget> Guide;
        TStrongObjectPtr<UCommonActivatableWidget> GuideScreen;
        TStrongObjectPtr<UGameSettingsWidget> Settings;
        TStrongObjectPtr<UConnectingPopupWidget> Connecting;
        UPdUIActionRouter* Router = nullptr;
        UUiSubsystem* Ui = nullptr;
        APlayerController* Controller = nullptr;
    };
    const TSharedRef<FState> State = MakeShared<FState>();
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        State->Controller = GEditor->PlayWorld->GetFirstPlayerController();
        ULocalPlayer* Player = State->Controller->GetLocalPlayer();
        State->Ui = Player->GetSubsystem<UUiSubsystem>();
        State->Router = Player->GetSubsystem<UPdUIActionRouter>();
        State->Ui->HideTravelLoadingScreen();
        FSlateApplication::Get().SetUserFocus(State->Router->GetLocalPlayerIndex(), Player->ViewportClient->GetGameViewportWidget());
        UUiSubsystem::SetBaseInputMode(State->Controller, EUiInputMode::GameOnly);
        UWidgetClassDefinition* Definition = UUiSubsystem::LoadConfiguredEditorWidgetClassDefinition();
        TestNotNull(TEXT("Configured UI definition"), Definition);
        State->Ui->SetWidgetClassDefinition(Definition);
        TestNotNull(TEXT("Configured guide class"), Definition ? Definition->GetGuideWidgetClass().Get() : nullptr);
        UClass* MenuClass = LoadClass<UMenuPopupWidget>(nullptr, TEXT("/Game/UI/Widget/Game/WBP_MenuPopup.WBP_MenuPopup_C"));
        if (!TestNotNull(TEXT("Settings Blueprint"), MenuClass)) return true;
        State->Menu.Reset(CreateWidget<UMenuPopupWidget>(State->Controller, MenuClass));
        State->Ui->PushScreen(State->Menu.Get());
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        UButton* GuideButton = Cast<UButton>(State->Menu->GetWidgetFromName(TEXT("Btn_Guide")));
        if (TestNotNull(TEXT("Guide button binding"), GuideButton)) GuideButton->OnClicked.Broadcast();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        State->Guide.Reset(Cast<UGuideWidget>(State->Menu->GetActiveGuideWidget()));
        if (!TestNotNull(TEXT("Guide opened by existing button"), State->Guide.Get())) return true;
        State->GuideScreen.Reset(State->Router->GetLeafmostActivatableWidget());
        TestFalse(TEXT("Guide replaces settings in the menu stack"), State->Menu->IsActivated());
        State->Guide->OpenGameSettings();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->GuideScreen.IsValid()) return true;
        UCommonActivatableWidget* Leaf = State->Router->GetLeafmostActivatableWidget();
        UPanelWidget* Root = Leaf ? Cast<UPanelWidget>(Leaf->GetRootWidget()) : nullptr;
        State->Settings.Reset(Root ? Cast<UGameSettingsWidget>(Root->GetChildAt(0)) : nullptr);
        if (!TestNotNull(TEXT("Personal settings opened"), State->Settings.Get())) return true;
        TestTrue(TEXT("Guide remains visible under personal settings"), State->GuideScreen->IsActivated());
        TestTrue(TEXT("Personal settings receives focus"), State->Settings->GetInitialFocusTarget()->HasUserFocus(State->Controller));
        UButton* Done = Cast<UButton>(State->Settings->GetWidgetFromName(TEXT("Btn_Done")));
        if (TestNotNull(TEXT("Done button binding"), Done)) Done->OnClicked.Broadcast();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->GuideScreen.IsValid()) return true;
        TestEqual(TEXT("Done returns to guide"), State->Router->GetLeafmostActivatableWidget(), State->GuideScreen.Get());
        State->Connecting.Reset(State->Ui->ShowConnectingPopup(false));
        TestNotNull(TEXT("Connecting Blueprint created"), State->Connecting.Get());
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Connecting.IsValid()) return true;
        UCommonActivatableWidget* LoadingScreen = State->Router->GetLeafmostActivatableWidget();
        TestTrue(TEXT("Connecting covers guide"), LoadingScreen != State->GuideScreen.Get());
        State->Router->ProcessInput(EKeys::Escape, IE_Pressed);
        State->Router->ProcessInput(EKeys::Escape, IE_Released);
        TestTrue(TEXT("Noncancelable loading consumes Back"), LoadingScreen && LoadingScreen->IsActivated());
        State->Connecting->SetCancelButtonEnabled(true);
        State->Router->ProcessInput(EKeys::Escape, IE_Pressed);
        State->Router->ProcessInput(EKeys::Escape, IE_Released);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->GuideScreen.IsValid()) return true;
        TestEqual(TEXT("Cancel returns to guide"), State->Router->GetLeafmostActivatableWidget(), State->GuideScreen.Get());
        State->Router->ProcessInput(EKeys::Escape, IE_Pressed);
        State->Router->ProcessInput(EKeys::Escape, IE_Released);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if (!State->Menu.IsValid()) return true;
        TestTrue(TEXT("Back from guide reactivates settings"), State->Menu->IsActivated());
        TestEqual(TEXT("Settings regains input"), State->Router->GetLeafmostActivatableWidget(), static_cast<UCommonActivatableWidget*>(State->Menu.Get()));
        UGameplayStatics::SetGamePaused(State->Controller, true);
        UButton* Resume = Cast<UButton>(State->Menu->GetDesiredFocusTarget());
        if (TestNotNull(TEXT("Resume binding"), Resume)) Resume->OnClicked.Broadcast();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        TestFalse(TEXT("Default input restores while world is paused"), State->Router->IsGameplayInputBlocked());
        TestFalse(TEXT("Nested screens leave no movement lock"), State->Controller->IsMoveInputIgnored());
        TestFalse(TEXT("Nested screens leave no look lock"), State->Controller->IsLookInputIgnored());
        UGameplayStatics::SetGamePaused(State->Controller, false);
        for (int32 Index = 0; Index < 3; ++Index)
        {
            UMenuPopupWidget* Menu = CreateWidget<UMenuPopupWidget>(State->Controller, State->Menu->GetClass());
            State->Ui->PushScreen(Menu);
            Menu->CloseMenu();
        }
        State->Menu.Reset();
        State->Guide.Reset();
        State->GuideScreen.Reset();
        State->Settings.Reset();
        State->Connecting.Reset();
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        TestFalse(TEXT("Rapid toggles leave no input lock"), State->Router->IsGameplayInputBlocked());
        TestNull(TEXT("Rapid toggles leave no active screen"), State->Router->GetLeafmostActivatableWidget());
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCommonUiAssetTest, "Pandora.UI.CommonUI.BlueprintContracts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPdCommonUiAssetTest::RunTest(const FString& Parameters)
{
    const TCHAR* Assets[] = {
        TEXT("/Game/UI/Widget/Game/WBP_MenuPopup.WBP_MenuPopup"),
        TEXT("/Game/UI/Widget/Game/WBP_TrainingRoomMenuPopup.WBP_TrainingRoomMenuPopup"),
        TEXT("/Game/UI/Widget/WBP_Info.WBP_Info"),
        TEXT("/Game/UI/Widget/Pandora/WBP_PandoraTree.WBP_PandoraTree"),
        TEXT("/Game/UI/Widget/Settings/WBP_GameSettings.WBP_GameSettings"),
        TEXT("/Game/UI/Widget/WBP_Guide.WBP_Guide"),
        TEXT("/Game/UI/Widget/WBP_SelectPandora.WBP_SelectPandora"),
        TEXT("/Game/UI/Widget/Game/WBP_GameResultPopup.WBP_GameResultPopup"),
        TEXT("/Game/UI/Widget/Lobby/WBP_Lobby.WBP_Lobby"),
        TEXT("/Game/UI/Widget/Lobby/WBP_GameConfigPopup.WBP_GameConfigPopup"),
        TEXT("/Game/UI/Widget/Game/WBP_ConnectingPopup.WBP_ConnectingPopup"),
        TEXT("/Game/Mode/BP_LobbyHUD.BP_LobbyHUD")
    };
    for (const TCHAR* Path : Assets)
    {
        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
        if (!TestNotNull(Path, Blueprint)) continue;
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        TestTrue(FString::Printf(TEXT("Blueprint compiles: %s"), Path), Blueprint->Status != BS_Error);
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                for (const FName PropertyName : {FName(TEXT("FunctionReference")), FName(TEXT("VariableReference"))})
                {
                    const FProperty* Property = Node->GetClass()->FindPropertyByName(PropertyName);
                    if (!Property) continue;
                    FString Reference;
                    Property->ExportText_InContainer(0, Reference, Node, nullptr, nullptr, PPF_None);
                    TestFalse(FString::Printf(TEXT("No removed input API: %s %s"), Path, *Node->GetName()),
                        Reference.Contains(TEXT("ToggleUiMode")) || Reference.Contains(TEXT("RestoreInfoUiInputMode"))
                        || Reference.Contains(TEXT("SetRestoreGameInputOnClose")) || Reference.Contains(TEXT("bRestoreGameInputOnClose")));
                    if (Reference.Contains(TEXT("IsInViewport")) || Reference.Contains(TEXT("SetInputMode"))
                        || Reference.Contains(TEXT("SetUserFocus")) || Reference.Contains(TEXT("SetKeyboardFocus")))
                        AddInfo(FString::Printf(TEXT("UI graph reference: %s %s %s"), Path, *Node->GetName(), *Reference));
                }
            }
        }
    }
    return true;
}

#endif
