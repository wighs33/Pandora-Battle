#include "UI/Widget/PlayerHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Definition/Online/AchievementDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Online/AchievementSubsystem.h"
#include "UI/TeamColorUtils.h"
#include "UI/Widget/KillBoxWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/TransBuffer.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerHudWidget)

namespace
{
	void ResetEditorTransactionBufferIfContainsPieObjects()
	{
#if WITH_EDITOR
		if (GEditor && GEditor->Trans && GEditor->Trans->ContainsPieObjects())
		{
			GEditor->ResetTransaction(NSLOCTEXT(
				"PlayerHudWidget",
				"TransactionContainedPlayerHudPieObject",
				"A player HUD PIE object was in the transaction buffer and had to be destroyed"));
		}
#endif
	}
}

void UPlayerHudWidget::InitializePlayerHud(UWidgetClassDefinition* InWidgetClassDefinition)
{
	WidgetClassDefinition = InWidgetClassDefinition;
	RefreshLobbyTipVisibility();
	RefreshKillBoxVisibility();
	if (!RefreshAchievementAvatar())
	{
		StartAchievementAvatarRefreshRetry();
	}

	if (CanRebuildKillBox())
	{
		CenterKillBoxContainer();
		RebuildKillBox();
	}
}

void UPlayerHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ClearTransactionalFlagsForRuntimeWidget(this);
	RefreshLobbyTipVisibility();
	RefreshKillBoxVisibility();
	AchievementAvatarRefreshRetryCount = 0;
	if (!RefreshAchievementAvatar())
	{
		StartAchievementAvatarRefreshRetry();
	}

	if (CanRebuildKillBox())
	{
		CenterKillBoxContainer();
		RebuildKillBox();
	}
}

void UPlayerHudWidget::NativeDestruct()
{
	ClearAchievementAvatarRefreshRetry();
	ClearKillBoxWidgets();
	ResetEditorTransactionBufferIfContainsPieObjects();
	Super::NativeDestruct();
}

void UPlayerHudWidget::RefreshLobbyTipVisibility()
{
	if (!Txt_LobbyTip)
	{
		return;
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	const bool bIsLobbyHud = OwningPlayer && OwningPlayer->GetHUD<ALobbyHUD>();
	Txt_LobbyTip->SetVisibility(bIsLobbyHud ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UPlayerHudWidget::RefreshAchievementAvatar()
{
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	const APlayerController* PlayerController = GetOwningPlayer();
	if (!PdGameInstance || !PlayerController)
	{
		return false;
	}

	FString PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->ResolveSavePlayerId(
			PlayerController,
			PlayerController->PlayerState);
		PlayerId.TrimStartAndEndInline();
	}
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->GetLocalClientSavePlayerId();
		PlayerId.TrimStartAndEndInline();
	}
	if (PlayerId.IsEmpty())
	{
		return false;
	}

	const FName SelectedAchievementId =
		PdGameInstance->GetSelectedAchievementId(PlayerId);
	if (SelectedAchievementId.IsNone())
	{
		return true;
	}

	UAchievementSubsystem* AchievementSubsystem =
		PdGameInstance->GetSubsystem<UAchievementSubsystem>();
	const UAchievementDefinition* AchievementDefinition = AchievementSubsystem
		? AchievementSubsystem->GetAchievementDefinition()
		: nullptr;
	if (!AchievementDefinition)
	{
		return false;
	}

	for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
	{
		FString CanonicalId = Achievement.AchievementId;
		CanonicalId.TrimStartAndEndInline();
		if (!Achievement.bEnabled
			|| CanonicalId.IsEmpty()
			|| FName(*CanonicalId) != SelectedAchievementId)
		{
			continue;
		}

		UTexture2D* AchievementTexture = Achievement.UnlockedIcon.Get();
		UImage* PlayerAvatarImage = FindImageInUserWidget(this, TEXT("PlayerAvatar"));
		if (!AchievementTexture || !PlayerAvatarImage)
		{
			return false;
		}

		PlayerAvatarImage->SetBrushFromTexture(AchievementTexture, true);
		PlayerAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return true;
	}

	return true;
}

void UPlayerHudWidget::StartAchievementAvatarRefreshRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AchievementAvatarRefreshTimerHandle,
			this,
			&ThisClass::HandleAchievementAvatarRefreshRetry,
			0.2f,
			true);
	}
}

void UPlayerHudWidget::HandleAchievementAvatarRefreshRetry()
{
	++AchievementAvatarRefreshRetryCount;
	if (RefreshAchievementAvatar() || AchievementAvatarRefreshRetryCount >= 25)
	{
		ClearAchievementAvatarRefreshRetry();
	}
}

void UPlayerHudWidget::ClearAchievementAvatarRefreshRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AchievementAvatarRefreshTimerHandle);
	}
	AchievementAvatarRefreshTimerHandle.Invalidate();
}

UImage* UPlayerHudWidget::FindImageInUserWidget(
	UUserWidget* RootWidget,
	const FName ImageName) const
{
	if (!RootWidget || !RootWidget->WidgetTree)
	{
		return nullptr;
	}

	if (UImage* FoundImage = Cast<UImage>(RootWidget->WidgetTree->FindWidget(ImageName)))
	{
		return FoundImage;
	}

	return FindImageInWidget(RootWidget->WidgetTree->RootWidget, ImageName);
}

UImage* UPlayerHudWidget::FindImageInWidget(
	UWidget* RootWidget,
	const FName ImageName) const
{
	if (!RootWidget)
	{
		return nullptr;
	}

	if (RootWidget->GetFName() == ImageName)
	{
		if (UImage* Image = Cast<UImage>(RootWidget))
		{
			return Image;
		}
	}

	if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(RootWidget))
	{
		if (UImage* FoundImage = FindImageInUserWidget(ChildUserWidget, ImageName))
		{
			return FoundImage;
		}
	}

	if (const UPanelWidget* PanelWidget = Cast<UPanelWidget>(RootWidget))
	{
		for (int32 ChildIndex = 0; ChildIndex < PanelWidget->GetChildrenCount(); ++ChildIndex)
		{
			if (UImage* FoundImage =
				FindImageInWidget(PanelWidget->GetChildAt(ChildIndex), ImageName))
			{
				return FoundImage;
			}
		}
	}

	if (const UContentWidget* ContentWidget = Cast<UContentWidget>(RootWidget))
	{
		return FindImageInWidget(ContentWidget->GetContent(), ImageName);
	}

	return nullptr;
}

void UPlayerHudWidget::RefreshKillBoxVisibility()
{
	if (!HorizontalBox_KillBox)
	{
		return;
	}

	const bool bTrainingRoom = IsTrainingRoomMap();
	HorizontalBox_KillBox->SetVisibility(
		bTrainingRoom
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	if (bTrainingRoom)
	{
		ClearKillBoxWidgets();
	}
}

bool UPlayerHudWidget::IsTrainingRoomMap() const
{
	const UWorld* World = GetWorld();
	const UMatchRuleDefinition* MatchRules =
		UMatchRuleDefinition::ResolveDefaultDefinition();
	if (!World || !MatchRules)
	{
		return false;
	}

	return MatchRules->IsTrainingRoomMapName(
		UGameplayStatics::GetCurrentLevelName(this, true));
}

void UPlayerHudWidget::RebuildKillBox()
{
	ClearKillBoxWidgets();
	if (!CanRebuildKillBox())
	{
		return;
	}

	CenterKillBoxContainer();

	TArray<int32> ActiveTeamColorIndices;
	TMap<int32, int32> KillCountByTeam;
	ResolveActiveTeamStats(ActiveTeamColorIndices, KillCountByTeam);
	BuildKillBoxWidgetsForTeams(ActiveTeamColorIndices);
	RefreshKillBox();
	StartKillBoxRefreshTimer();
}

void UPlayerHudWidget::BuildKillBoxWidgetsForTeams(const TArray<int32>& TeamColorIndices)
{
	if (!HorizontalBox_KillBox)
	{
		return;
	}

	CenterKillBoxContainer();

	const TSubclassOf<UKillBoxWidget> KillBoxWidgetClass = ResolveKillBoxWidgetClass();
	if (!KillBoxWidgetClass)
	{
		return;
	}

	for (const int32 TeamColorIndex : TeamColorIndices)
	{
		UKillBoxWidget* KillBoxWidget = CreateWidget<UKillBoxWidget>(GetOwningPlayer(), KillBoxWidgetClass);
		if (!KillBoxWidget)
		{
			continue;
		}
		ClearTransactionalFlagsForRuntimeWidget(KillBoxWidget);

		KillBoxWidget->SetTeamInfo(
			TeamColorIndex,
			ResolveTeamName(TeamColorIndex),
			LabTeamColorUtils::GetTeamColor(TeamColorIndex));
		KillBoxWidget->SetKillCount(0);

		if (UHorizontalBoxSlot* KillBoxSlot = HorizontalBox_KillBox->AddChildToHorizontalBox(KillBoxWidget))
		{
			KillBoxSlot->SetHorizontalAlignment(HAlign_Center);
			KillBoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		KillBoxWidgets.Add(TeamColorIndex, KillBoxWidget);
	}
}

void UPlayerHudWidget::StartKillBoxRefreshTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			KillBoxRefreshTimerHandle,
			this,
			&ThisClass::RefreshKillBox,
			ResolveKillBoxRefreshInterval(),
			true);
	}
}

void UPlayerHudWidget::RefreshKillBox()
{
	if (!CanRebuildKillBox())
	{
		return;
	}

	TArray<int32> ActiveTeamColorIndices;
	TMap<int32, int32> KillCountByTeam;
	ResolveActiveTeamStats(ActiveTeamColorIndices, KillCountByTeam);

	if (!AreKillBoxWidgetsBuiltForTeams(ActiveTeamColorIndices))
	{
		if (HorizontalBox_KillBox)
		{
			HorizontalBox_KillBox->ClearChildren();
		}

		KillBoxWidgets.Reset();
		CenterKillBoxContainer();
		BuildKillBoxWidgetsForTeams(ActiveTeamColorIndices);
	}

	for (const TPair<int32, TObjectPtr<UKillBoxWidget>>& KillBoxPair : KillBoxWidgets)
	{
		if (UKillBoxWidget* KillBoxWidget = KillBoxPair.Value)
		{
			KillBoxWidget->SetKillCount(KillCountByTeam.FindRef(KillBoxPair.Key));
		}
	}
}

void UPlayerHudWidget::ResolveActiveTeamStats(
	TArray<int32>& OutTeamColorIndices,
	TMap<int32, int32>& OutKillCountByTeam) const
{
	OutTeamColorIndices.Reset();
	OutKillCountByTeam.Reset();

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
		if (!PdPlayerState)
		{
			continue;
		}

		const int32 TeamColorIndex =
			PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex();
		if (TeamColorIndex == INDEX_NONE)
		{
			continue;
		}

		if (!OutKillCountByTeam.Contains(TeamColorIndex))
		{
			OutTeamColorIndices.Add(TeamColorIndex);
			OutKillCountByTeam.Add(TeamColorIndex, 0);
		}

		OutKillCountByTeam.FindChecked(TeamColorIndex) +=
			PdPlayerState->GetPlayerMatchComponent()->GetKillCount();
	}

	OutTeamColorIndices.Sort();
}

bool UPlayerHudWidget::AreKillBoxWidgetsBuiltForTeams(const TArray<int32>& TeamColorIndices) const
{
	if (KillBoxWidgets.Num() != TeamColorIndices.Num())
	{
		return false;
	}

	for (const int32 TeamColorIndex : TeamColorIndices)
	{
		if (!KillBoxWidgets.Contains(TeamColorIndex))
		{
			return false;
		}
	}

	return true;
}

TSubclassOf<UKillBoxWidget> UPlayerHudWidget::ResolveKillBoxWidgetClass() const
{
	const UWidgetClassDefinition* ResolvedWidgetDefinition = WidgetClassDefinition
		? WidgetClassDefinition.Get()
		: UWidgetClassDefinition::ResolveWidgetClassDefinition(this);

	if (ResolvedWidgetDefinition)
	{
		if (UClass* LoadedClass = ResolvedWidgetDefinition->GetKillBoxEntryWidgetClass().Get())
		{
			if (LoadedClass->IsChildOf(UKillBoxWidget::StaticClass()))
			{
				return LoadedClass;
			}
		}
	}

	return nullptr;
}

float UPlayerHudWidget::ResolveKillBoxRefreshInterval() const
{
	const UWidgetClassDefinition* ResolvedWidgetDefinition = WidgetClassDefinition
		? WidgetClassDefinition.Get()
		: UWidgetClassDefinition::ResolveWidgetClassDefinition(this);

	const float RefreshInterval = ResolvedWidgetDefinition
		? ResolvedWidgetDefinition->GetKillBoxWidgetSettings().RefreshInterval
		: 0.2f;
	return FMath::Max(RefreshInterval, 0.01f);
}

FText UPlayerHudWidget::ResolveTeamName(const int32 TeamColorIndex) const
{
	switch (TeamColorIndex)
	{
	case 0:
		return NSLOCTEXT("PlayerHudWidget", "TeamNameRed", "Red");
	case 1:
		return NSLOCTEXT("PlayerHudWidget", "TeamNameBlue", "Blue");
	case 2:
		return NSLOCTEXT("PlayerHudWidget", "TeamNameYellow", "Yellow");
	case 3:
		return NSLOCTEXT("PlayerHudWidget", "TeamNamePurple", "Purple");
	case 4:
		return NSLOCTEXT("PlayerHudWidget", "TeamNameGreen", "Green");
	case 5:
		return NSLOCTEXT("PlayerHudWidget", "TeamNameOrange", "Orange");
	default:
		return FText::GetEmpty();
	}
}

bool UPlayerHudWidget::CanRebuildKillBox() const
{
	const UWorld* World = GetWorld();
	return !IsDesignTime()
		&& World
		&& World->IsGameWorld()
		&& HorizontalBox_KillBox
		&& !IsTrainingRoomMap();
}

void UPlayerHudWidget::CenterKillBoxContainer() const
{
	if (!HorizontalBox_KillBox || !HorizontalBox_KillBox->Slot)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HorizontalBox_KillBox->Slot))
	{
		const FAnchors Anchors = CanvasSlot->GetAnchors();
		CanvasSlot->SetAnchors(FAnchors(0.5f, Anchors.Minimum.Y, 0.5f, Anchors.Maximum.Y));

		const FVector2D Position = CanvasSlot->GetPosition();
		CanvasSlot->SetPosition(FVector2D(0.0f, Position.Y));

		const FVector2D Alignment = CanvasSlot->GetAlignment();
		CanvasSlot->SetAlignment(FVector2D(0.5f, Alignment.Y));
		CanvasSlot->SetAutoSize(true);
		return;
	}

	if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(HorizontalBox_KillBox->Slot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		return;
	}

	if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(HorizontalBox_KillBox->Slot))
	{
		VerticalBoxSlot->SetHorizontalAlignment(HAlign_Center);
		return;
	}

	if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(HorizontalBox_KillBox->Slot))
	{
		HorizontalBoxSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UPlayerHudWidget::ClearKillBoxWidgets()
{
	ClearKillBoxTimer();
	KillBoxWidgets.Reset();

	if (HorizontalBox_KillBox)
	{
		HorizontalBox_KillBox->ClearChildren();
	}
}

void UPlayerHudWidget::ClearKillBoxTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KillBoxRefreshTimerHandle);
	}

	KillBoxRefreshTimerHandle.Invalidate();
}

void UPlayerHudWidget::ClearTransactionalFlagsForRuntimeWidget(UUserWidget* Widget) const
{
	if (!Widget || Widget->IsDesignTime())
	{
		return;
	}

	Widget->ClearFlags(RF_Transactional);
	if (UWidgetTree* RuntimeWidgetTree = Widget->WidgetTree)
	{
		RuntimeWidgetTree->ClearFlags(RF_Transactional);
		RuntimeWidgetTree->ForEachWidget([](UWidget* ChildWidget)
		{
			if (ChildWidget)
			{
				ChildWidget->ClearFlags(RF_Transactional);
			}
		});
	}
}
