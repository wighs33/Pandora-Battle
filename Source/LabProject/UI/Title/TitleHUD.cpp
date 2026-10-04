#include "UI/Title/TitleHUD.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Core/UiScreen.h"
#include "Engine/LocalPlayer.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"

#include "UI/Match/GameResultWidget.h"
#include "UI/Title/TitleWidget.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Localization/MenuLocalizationSubsystem.h"
#include "Luna/LunaChatSubsystem.h"
#include "Online/Backend/BackendClientSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleHUD)

namespace
{
	// Blender's "Key 1" is imported into Luna as "Key_1".
	const FName TitleBlinkMorph(TEXT("Key_1"));
	// Blender's "Key 2" (mouth) is imported into Luna as "Key_2".
	const FName TitleMouthMorph(TEXT("Key_2"));
	// DT_MenuText rows: Title.LunaGreeting, then Title.LunaTip01 ... Title.LunaTip20.
	const FName LunaGreetingKey(TEXT("Title.LunaGreeting"));
	const FName LunaChatThinkingKey(TEXT("Title.LunaChatThinking"));
	const FName LunaChatUnavailableKey(TEXT("Title.LunaChatUnavailable"));
	const FName RpgModeJoiningKey(TEXT("Title.RpgModeJoining"));
	const FName RpgModeFailedKey(TEXT("Title.RpgModeFailed"));
}

ATitleHUD::ATitleHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TitlePresentationRoot")));
	TitleCharacterMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TitleCharacterMesh"));
	TitleCharacterMesh->SetupAttachment(RootComponent);
	TitleCharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TitleCharacterMesh->SetGenerateOverlapEvents(false);
	TitleCharacterMesh->SetVisibleInSceneCaptureOnly(true);
	TitleCharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	TitleCharacterMesh->bForceRefpose = true;

	TitleCharacterCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("TitleCharacterCapture"));
	TitleCharacterCapture->SetupAttachment(RootComponent);
	TitleCharacterCapture->SetRelativeLocation(FVector(0, -300, 100));
	TitleCharacterCapture->SetRelativeRotation(FRotator(0, 90, 0));
	TitleCharacterCapture->FOVAngle = 35;
	TitleCharacterCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	TitleCharacterCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	TitleCharacterCapture->ShowFlags.SetAtmosphere(false);
	TitleCharacterCapture->ShowFlags.SetFog(false);
	TitleCharacterCapture->ShowFlags.SetMotionBlur(false);
	TitleCharacterCapture->ShowFlags.SetBloom(false);
	TitleCharacterCapture->bCaptureEveryFrame = true;
	TitleCharacterCapture->bCaptureOnMovement = false;

	TitleCharacterKeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TitleCharacterKeyLight"));
	TitleCharacterKeyLight->SetupAttachment(RootComponent);
	TitleCharacterKeyLight->SetRelativeLocation(FVector(-120, -160, 240));
	TitleCharacterKeyLight->SetIntensity(12000);
	TitleCharacterKeyLight->SetAttenuationRadius(600);
	TitleCharacterKeyLight->SetCastShadows(false);

	TitleCharacterFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TitleCharacterFillLight"));
	TitleCharacterFillLight->SetupAttachment(RootComponent);
	TitleCharacterFillLight->SetRelativeLocation(FVector(130, -80, 120));
	TitleCharacterFillLight->SetIntensity(5000);
	TitleCharacterFillLight->SetAttenuationRadius(500);
	TitleCharacterFillLight->SetCastShadows(false);

	// The title's world view is empty behind UMG; do not meter exposure against it.
	UPostProcessComponent* TitleExposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("TitleExposure"));
	TitleExposure->SetupAttachment(RootComponent);
	TitleExposure->bUnbound = true;
	TitleExposure->Settings.bOverride_AutoExposureMethod = true;
	TitleExposure->Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	TitleExposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	TitleExposure->Settings.AutoExposureApplyPhysicalCameraExposure = false;
}

void ATitleHUD::InitializeTitleCharacter()
{
	if (!TitleWidget || !TitleCharacterMesh->GetSkeletalMeshAsset() || !TitleCharacterDisplayMaterial) return;

	// Keep the portrait's lights away from the level. Only this mesh is captured.
	SetActorLocation(FVector(0, 0, -100000));
	SetActorHiddenInGame(false);
	TitleCharacterRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	TitleCharacterRenderTarget->ClearColor = FLinearColor(0, 0, 0, 1);
	TitleCharacterRenderTarget->InitCustomFormat(768, 1024, PF_FloatRGBA, false);
	TitleCharacterCapture->TextureTarget = TitleCharacterRenderTarget;
	TitleCharacterCapture->ShowOnlyComponent(TitleCharacterMesh);
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TitleCharacterDisplayMaterial, this);
	Material->SetTextureParameterValue(TEXT("CharacterTexture"), TitleCharacterRenderTarget);
	TitleWidget->SetTitleCharacterMaterial(Material);
	if (TitleCharacterMesh->GetSkeletalMeshAsset()->FindMorphTarget(TitleBlinkMorph))
	{
		ScheduleTitleBlink();
	}
	bTitleMouthAvailable = TitleCharacterMesh->GetSkeletalMeshAsset()->FindMorphTarget(TitleMouthMorph) != nullptr;
	ShowTitleSpeech();
}

void ATitleHUD::ScheduleTitleBlink()
{
	TitleBlinkElapsed = -1.f;
	TitleCharacterMesh->SetMorphTarget(TitleBlinkMorph, 0.f);
	const float MinInterval = FMath::Max(0.1f, BlinkIntervalMin);
	GetWorldTimerManager().SetTimer(TitleBlinkTimer, this, &ATitleHUD::BeginTitleBlink,
		FMath::FRandRange(MinInterval, FMath::Max(MinInterval, BlinkIntervalMax)), false);
}

void ATitleHUD::BeginTitleBlink()
{
	TitleBlinkElapsed = 0.f;
}

void ATitleHUD::ShowTitleSpeech()
{
	if (!TitleWidget) return;
	// The greeting always comes first and is never part of the random pool.
	FName TextKey = LunaGreetingKey;
	if (LastTitleTip == INDEX_NONE)
	{
		LastTitleTip = 0;
	}
	else
	{
		// Any tip except the one just shown.
		int32 Tip = FMath::RandRange(1, LastTitleTip > 0 ? LunaChat::TipCount - 1 : LunaChat::TipCount);
		if (LastTitleTip > 0 && Tip >= LastTitleTip) ++Tip;
		LastTitleTip = Tip;
		TextKey = LunaChat::TipKey(Tip);
	}
	// The mouth only moves while the bubble is actually visible.
	if (TitleWidget->ShowLunaSpeech(TextKey, GetTitleCharacterHeadTopUV()) && bTitleMouthAvailable)
	{
		TitleMouthElapsed = 0.f;
	}
	GetWorldTimerManager().SetTimer(TitleSpeechTimer, this, &ATitleHUD::HideTitleSpeech,
		FMath::Max(0.5f, SpeechDisplayDuration), false);
}

void ATitleHUD::HideTitleSpeech()
{
	if (TitleWidget) TitleWidget->HideLunaSpeech();
	TitleMouthElapsed = -1.f;
	TitleCharacterMesh->SetMorphTarget(TitleMouthMorph, 0.f);
	const float NextTipDelay = bLunaChatActive ? ChatTipResumeDelay : SpeechHiddenInterval;
	bLunaChatActive = false;
	GetWorldTimerManager().SetTimer(TitleSpeechTimer, this, &ATitleHUD::ShowTitleSpeech,
		FMath::Max(0.1f, NextTipDelay), false);
}

void ATitleHUD::BindLunaChat()
{
	ULunaChatSubsystem* Luna = UGameInstance::GetSubsystem<ULunaChatSubsystem>(GetGameInstance());
	if (!TitleWidget || !Luna) return;
	TitleWidget->OnLunaQuestionSubmitted().AddUObject(this, &ATitleHUD::HandleLunaQuestion);
	LunaReplyUpdatedHandle = Luna->OnReplyUpdated().AddUObject(this, &ATitleHUD::HandleLunaReplyUpdated);
	LunaReplyFinishedHandle = Luna->OnReplyFinished().AddUObject(this, &ATitleHUD::HandleLunaReplyFinished);
}

void ATitleHUD::UnbindLunaChat()
{
	if (TitleWidget) TitleWidget->OnLunaQuestionSubmitted().RemoveAll(this);
	if (ULunaChatSubsystem* Luna = UGameInstance::GetSubsystem<ULunaChatSubsystem>(GetGameInstance()))
	{
		Luna->OnReplyUpdated().Remove(LunaReplyUpdatedHandle);
		Luna->OnReplyFinished().Remove(LunaReplyFinishedHandle);
		// Nobody is left to show an answer that is still streaming.
		Luna->CancelReply();
	}
	LunaReplyUpdatedHandle.Reset();
	LunaReplyFinishedHandle.Reset();
}

void ATitleHUD::BindRpgMode()
{
	UBackendClientSubsystem* Backend = UGameInstance::GetSubsystem<UBackendClientSubsystem>(GetGameInstance());
	if (!TitleWidget || !Backend) return;
	TitleWidget->OnRpgModeRequested().AddUObject(this, &ATitleHUD::HandleRpgModeRequested);
	Backend->OnMatchJoinFinished.AddUniqueDynamic(this, &ATitleHUD::HandleRpgJoinFinished);
}

void ATitleHUD::UnbindRpgMode()
{
	if (TitleWidget) TitleWidget->OnRpgModeRequested().RemoveAll(this);
	if (UBackendClientSubsystem* Backend = UGameInstance::GetSubsystem<UBackendClientSubsystem>(GetGameInstance()))
	{
		Backend->OnMatchJoinFinished.RemoveDynamic(this, &ATitleHUD::HandleRpgJoinFinished);
	}
	bRpgJoinPending = false;
}

void ATitleHUD::HandleRpgModeRequested()
{
	UBackendClientSubsystem* Backend = UGameInstance::GetSubsystem<UBackendClientSubsystem>(GetGameInstance());
	if (!TitleWidget || !Backend || Backend->IsMatchJoinInProgress()) return;
	bRpgJoinPending = true;
	TitleWidget->SetRpgModeEnabled(false);
	ShowLunaChatText(GetLunaChatLine(RpgModeJoiningKey,
		NSLOCTEXT("TitleHUD", "RpgModeJoining", "Let me find you a spot in the boss raid...")), true);
	// Logs in first when needed. A failure can be reported before this call returns.
	Backend->JoinOnlineMatch(EOnlineMatchMode::Rpg);
}

void ATitleHUD::HandleRpgJoinFinished(const bool bSucceeded, const FString& ErrorMessage)
{
	if (!bRpgJoinPending) return;
	bRpgJoinPending = false;
	// On success the client is already travelling to the raid map, and the title closes with this level.
	if (bSucceeded) return;

	if (TitleWidget) TitleWidget->SetRpgModeEnabled(true);
	const FText Line = GetLunaChatLine(RpgModeFailedKey,
		NSLOCTEXT("TitleHUD", "RpgModeFailed", "I couldn't reach the boss raid. Please try again in a moment."));
	ShowLunaChatText(Line, true);
	GetWorldTimerManager().SetTimer(TitleSpeechTimer, this, &ATitleHUD::HideTitleSpeech,
		FMath::Max(ChatReplyMinDuration, Line.ToString().Len() * ChatReplySecondsPerCharacter), false);
}

void ATitleHUD::HandleLunaQuestion(const FString& Question)
{
	ULunaChatSubsystem* Luna = UGameInstance::GetSubsystem<ULunaChatSubsystem>(GetGameInstance());
	if (!Luna || !Luna->Ask(Question)) return;
	// Luna listens with her mouth closed until the first words of the answer arrive.
	ShowLunaChatText(GetLunaChatLine(LunaChatThinkingKey,
		NSLOCTEXT("TitleHUD", "LunaChatThinking", "Hmm, let me think...")), false);
}

void ATitleHUD::HandleLunaReplyUpdated(const FString& ReplySoFar)
{
	ShowLunaChatText(FText::FromString(ReplySoFar), true);
}

void ATitleHUD::HandleLunaReplyFinished(const bool bSucceeded, const FString& Reply)
{
	const FText Line = bSucceeded
		? FText::FromString(Reply)
		: GetLunaChatLine(LunaChatUnavailableKey,
			NSLOCTEXT("TitleHUD", "LunaChatUnavailable", "I can't answer right now. Please ask me again in a moment."));
	ShowLunaChatText(Line, true);
	const float ReadingTime = Line.ToString().Len() * ChatReplySecondsPerCharacter;
	GetWorldTimerManager().SetTimer(TitleSpeechTimer, this, &ATitleHUD::HideTitleSpeech,
		FMath::Max(ChatReplyMinDuration, ReadingTime), false);
}

void ATitleHUD::ShowLunaChatText(const FText& Text, const bool bSpeaking)
{
	if (!TitleWidget) return;
	// A conversation replaces the rotating tips until its answer has been read.
	bLunaChatActive = true;
	GetWorldTimerManager().ClearTimer(TitleSpeechTimer);
	const bool bShown = TitleWidget->ShowLunaSpeechText(Text, GetTitleCharacterHeadTopUV());
	if (bShown && bSpeaking && bTitleMouthAvailable)
	{
		if (TitleMouthElapsed < 0.f) TitleMouthElapsed = 0.f;
	}
	else
	{
		TitleMouthElapsed = -1.f;
		TitleCharacterMesh->SetMorphTarget(TitleMouthMorph, 0.f);
	}
}

FText ATitleHUD::GetLunaChatLine(const FName Key, const FText& Fallback) const
{
	const UMenuLocalizationSubsystem* Localization = UGameInstance::GetSubsystem<UMenuLocalizationSubsystem>(GetGameInstance());
	return Localization ? Localization->GetTextOrFallback(Key, Fallback) : Fallback;
}

FVector2D ATitleHUD::GetTitleCharacterHeadTopUV() const
{
	// Project the top of Luna's bounds through the portrait capture; Img_TitleCharacter shows that render target as is.
	const FBoxSphereBounds& CharacterBounds = TitleCharacterMesh->Bounds;
	const FVector HeadTop = CharacterBounds.Origin + FVector(0, 0, CharacterBounds.BoxExtent.Z);
	const FVector ViewPoint = TitleCharacterCapture->GetComponentTransform().InverseTransformPositionNoScale(HeadTop);
	if (!TitleCharacterRenderTarget || ViewPoint.X <= UE_KINDA_SMALL_NUMBER) return FVector2D(0.5, 0.0);
	// Scene captures use a horizontal FOV; the vertical extent follows the render target's aspect.
	const double TanHalfFov = FMath::Tan(FMath::DegreesToRadians(TitleCharacterCapture->FOVAngle * 0.5f));
	const double Aspect = double(TitleCharacterRenderTarget->SizeX) / FMath::Max(1, TitleCharacterRenderTarget->SizeY);
	const double U = 0.5 + 0.5 * ViewPoint.Y / (ViewPoint.X * TanHalfFov);
	const double V = 0.5 - 0.5 * ViewPoint.Z * Aspect / (ViewPoint.X * TanHalfFov);
	return FVector2D(FMath::Clamp(U, 0.0, 1.0), FMath::Clamp(V, 0.0, 1.0));
}

void ATitleHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (TitleMouthElapsed >= 0.f)
	{
		// Smooth 0 -> 1 -> 0 per cycle, repeated while the bubble is visible.
		TitleMouthElapsed += DeltaSeconds;
		const float MouthPhase = TitleMouthElapsed / FMath::Max(0.05f, MouthCycleDuration);
		TitleCharacterMesh->SetMorphTarget(TitleMouthMorph, 0.5f - 0.5f * FMath::Cos(2.f * UE_PI * MouthPhase));
	}
	if (TitleBlinkElapsed < 0.f) return;
	TitleBlinkElapsed += DeltaSeconds;
	const float Phase = TitleBlinkElapsed / FMath::Max(0.05f, BlinkDuration);
	if (Phase >= 1.f)
	{
		ScheduleTitleBlink();
		return;
	}
	// Close quickly, briefly reach full closure, then open a little more slowly.
	const float Weight = Phase < 0.5f
		? FMath::Clamp(Phase / 0.4f, 0.f, 1.f)
		: FMath::Clamp((1.f - Phase) / 0.5f, 0.f, 1.f);
	TitleCharacterMesh->SetMorphTarget(TitleBlinkMorph, Weight);
}

void ATitleHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!TitleWidget && TitleWidgetClass)
	{
		TitleWidget = CreateWidget<UTitleWidget>(PlayerController, TitleWidgetClass);
	}

	if (!TitleWidget)
	{
		return;
	}

	InitializeTitleCharacter();
	BindLunaChat();
	BindRpgMode();
	Screen = CreateWidget<UUiScreen>(PlayerController);
	FUIInputConfig Config(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	Config.bIgnoreMoveInput = Config.bIgnoreLookInput = true;
	// 타이틀/방 목록의 종료는 기존 버튼이 담당한다.
	Screen->SetContent(TitleWidget, Config, EPdGameplayInputPolicy::Block, TitleWidget, FSimpleDelegate::CreateLambda([]() {}));
	PlayerController->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Screen);
	// 엔진 기본 PlayerController를 사용하므로 로컬 설정을 여기서 적용한다.
	PlayerController->GetLocalPlayer()->GetSubsystem<ULocalPlayerSettingsSubsystem>()->ApplyLocalPlayerSettings(PlayerController);

	ShowPendingGameResult();
}

void ATitleHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindLunaChat();
	UnbindRpgMode();
	GetWorldTimerManager().ClearTimer(TitleBlinkTimer);
	TitleBlinkElapsed = -1.f;
	TitleCharacterMesh->SetMorphTarget(TitleBlinkMorph, 0.f);
	GetWorldTimerManager().ClearTimer(TitleSpeechTimer);
	TitleMouthElapsed = -1.f;
	TitleCharacterMesh->SetMorphTarget(TitleMouthMorph, 0.f);
	if (TitleWidget) TitleWidget->HideLunaSpeech();
	TitleCharacterCapture->bCaptureEveryFrame = false;
	TitleCharacterCapture->TextureTarget = nullptr;
	TitleCharacterRenderTarget = nullptr;
	if (Screen) Screen->DeactivateWidget();
	Screen = nullptr;
	if (GameResultWidget)
	{
		GameResultWidget->RemoveFromParent();
		GameResultWidget = nullptr;
	}

	if (TitleWidget)
	{
		TitleWidget->RemoveFromParent();
		TitleWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void ATitleHUD::ShowPendingGameResult()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GetGameInstance());
	if (!PlayerController || !PlayerController->IsLocalController() || !LobbySubsystem)
	{
		return;
	}

	FGameResultPresentationData GameResultData;
	if (!LobbySubsystem->ConsumePendingTitleGameResult(GameResultData))
	{
		return;
	}

	if (!GameResultWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			GameResultWidgetClass = WidgetDefinition->GetGameResultWidgetClass();
		}
	}

	if (!GameResultWidgetClass)
	{
		return;
	}

	GameResultWidget = CreateWidget<UGameResultWidget>(PlayerController, GameResultWidgetClass);
	if (!GameResultWidget)
	{
		return;
	}

	GameResultWidget->SetInfo(
		GameResultData.WinnerTitle,
		GameResultData.WinnerTeamColorIndex,
		GameResultData.MaxKillerName,
		GameResultData.MaxKillCount,
		GameResultData.PlayerStats);
	GameResultWidget->SetExitToLobbyEnabled(GameResultData.bAllowLobbyTravelOnExit);
	GameResultWidget->SetShowRewards(GameResultData.bShowRewards);
	GameResultWidget->ShowResultScreen();
}
