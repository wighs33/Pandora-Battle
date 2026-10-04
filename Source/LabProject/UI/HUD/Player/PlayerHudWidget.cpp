#include "UI/HUD/Player/PlayerHudWidget.h"

#include "UI/Common/EditorTransactionReset.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Online/AchievementDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Lobby/LobbyHUD.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerState.h"
#include "Online/AchievementSubsystem.h"
#include "UI/Common/TeamColorUtils.h"
#include "UI/HUD/Match/KillBoxWidget.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerHudWidget)

void UPlayerHudWidget::InitializePlayerHud(UWidgetClassDefinition* InWidgetClassDefinition)
{
	WidgetClassDefinition = InWidgetClassDefinition;
	RefreshLobbyTipVisibility();
	RefreshKillBoxVisibility();
	RefreshAchievementAvatar();

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
	BindAchievementNotifications();
	RefreshLobbyTipVisibility();
	RefreshKillBoxVisibility();
	RefreshAchievementAvatar();

	if (CanRebuildKillBox())
	{
		CenterKillBoxContainer();
		RebuildKillBox();
	}
}

void UPlayerHudWidget::NativeDestruct()
{
	UnbindAchievementNotifications();
	ClearKillBoxWidgets();
	PdEditorTransaction::ResetIfContainsPieObjects();
	Super::NativeDestruct();
}

void UPlayerHudWidget::OnMenuLanguageChanged()
{
	for (const auto& Pair : KillBoxWidgets)
	{
		if (Pair.Value)
		{
			Pair.Value->SetTeamInfo(Pair.Key, ResolveTeamName(Pair.Key), LabTeamColorUtils::GetTeamColor(Pair.Key));
		}
	}
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
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	const APlayerController* PlayerController = GetOwningPlayer();
	if (!ProfileSubsystem || !PlayerController)
	{
		return false;
	}

	UImage* PlayerAvatarImage = FindImageInUserWidget(this, TEXT("PlayerAvatar"));
	if (PlayerAvatarImage)
	{
		PlayerAvatarImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	UAchievementSubsystem* AchievementSubsystem =
		UGameInstance::GetSubsystem<UAchievementSubsystem>(GetGameInstance());
	if (!AchievementSubsystem)
	{
		return false;
	}
	if (!AchievementSubsystem->IsSteamAchievementQueryComplete())
	{
		AchievementSubsystem->RequestSteamAchievementQuery();
	}
	if (!AchievementSubsystem->HasSteamAchievementData())
	{
#if WITH_EDITOR
		// Temporary local HUD preview; does not unlock or select a profile achievement.
		if (PlayerAvatarImage && GetWorld() && GetWorld()->IsPlayInEditor())
		{
			const UAchievementDefinition* PreviewDefinition = AchievementSubsystem->GetAchievementDefinition();
			if (PreviewDefinition && !PreviewDefinition->Achievements.IsEmpty())
			{
				if (UTexture2D* PreviewTexture = PreviewDefinition->Achievements[0].UnlockedIcon.Get())
				{
					PlayerAvatarImage->SetBrushFromTexture(PreviewTexture, false);
					PlayerAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					return true;
				}
			}
		}
#endif
		return false;
	}

	const FName SelectedAchievementId =
		ProfileSubsystem->GetSelectedAchievementId();
	if (SelectedAchievementId.IsNone())
	{
		return true;
	}

	const FString SteamAchievementId = SelectedAchievementId.ToString();
	if (!AchievementSubsystem->IsSteamAchievementKnown(SteamAchievementId)
		|| !AchievementSubsystem->IsSteamAchievementUnlocked(SteamAchievementId))
	{
		return true;
	}

	const UAchievementDefinition* AchievementDefinition = AchievementSubsystem
		? AchievementSubsystem->GetAchievementDefinition()
		: nullptr;
	if (!AchievementDefinition)
	{
		return false;
	}

	const FAchievementEntry* Achievement = AchievementDefinition->FindEnabledAchievement(SelectedAchievementId);
	if (!Achievement)
	{
		return true;
	}

	UTexture2D* AchievementTexture = Achievement->UnlockedIcon.Get();
	if (!AchievementTexture || !PlayerAvatarImage)
	{
		return false;
	}

	PlayerAvatarImage->SetBrushFromTexture(AchievementTexture, true);
	PlayerAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

// Steam 업적 조회 결과와 업적 아이콘 로딩 완료 때 아바타 아이콘을 다시 그린다.
void UPlayerHudWidget::BindAchievementNotifications()
{
	UnbindAchievementNotifications();
	UGameInstance* GameInstance = GetGameInstance();
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (!AchievementSubsystem)
	{
		return;
	}

	SteamAchievementStateChangedHandle =
		AchievementSubsystem->OnSteamAchievementStateChanged().AddUObject(
			this,
			&ThisClass::HandleAchievementDisplayChanged);
	AchievementPresentationReadyHandle =
		AchievementSubsystem->OnAchievementPresentationReady().AddUObject(
			this,
			&ThisClass::HandleAchievementDisplayChanged);
	AchievementSubsystem->RequestSteamAchievementQuery();
}

void UPlayerHudWidget::UnbindAchievementNotifications()
{
	UGameInstance* GameInstance = GetGameInstance();
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (AchievementSubsystem)
	{
		AchievementSubsystem->OnSteamAchievementStateChanged().Remove(
			SteamAchievementStateChangedHandle);
		AchievementSubsystem->OnAchievementPresentationReady().Remove(
			AchievementPresentationReadyHandle);
	}
	SteamAchievementStateChangedHandle.Reset();
	AchievementPresentationReadyHandle.Reset();
}

void UPlayerHudWidget::HandleAchievementDisplayChanged()
{
	RefreshAchievementAvatar();
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
	const ULevelDefinition* Levels =
		ULevelDefinition::ResolveDefaultDefinition();
	if (!World || !Levels)
	{
		return false;
	}

	return Levels->IsTrainingRoomMapName(
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
		return MenuText(TEXT("Team.Red"));
	case 1:
		return MenuText(TEXT("Team.Blue"));
	case 2:
		return MenuText(TEXT("Team.Yellow"));
	case 3:
		return MenuText(TEXT("Team.Purple"));
	case 4:
		return MenuText(TEXT("Team.Green"));
	case 5:
		return MenuText(TEXT("Team.Orange"));
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
