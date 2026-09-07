#include "UI/Widget/LeftProfileWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdHUD.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Online/AchievementDefinition.h"
#include "Online/AchievementSubsystem.h"
#include "TimerManager.h"
#include "Definition/UI/RecordDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftProfileWidget)

ULeftProfileWidget::ULeftProfileWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULeftProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindAchievementButtons();
	BindSteamAchievementStateChanged();
	BeginContentPreload();
	RefreshTierImage();
	RefreshAchievementButtons();

	PlayerNameRefreshRetryCount = 0;
	BindMatchDisplayNameChanged();
	if (!RefreshPlayerName())
	{
		SchedulePlayerNameRefreshRetry();
	}
}

void ULeftProfileWidget::NativeDestruct()
{
	ReleaseContentPreloads();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerNameRefreshRetryTimerHandle);
	}

	UnbindMatchDisplayNameChanged();
	UnbindSteamAchievementStateChanged();
	UnbindAchievementButtons();
	Super::NativeDestruct();
}

void ULeftProfileWidget::BeginContentPreload()
{
	ReleaseContentPreloads();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	DefinitionPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{
				UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
					.Record.ToSoftObjectPath(),
				UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
					.Achievement.ToSoftObjectPath()
			},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					BeginPresentationPreload(PreloadGeneration);
				}));
}

void ULeftProfileWidget::BeginPresentationPreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> PresentationPaths;
	if (const URecordDefinition* LoadedRecordData = ResolveRecordDefinition())
	{
		for (const FRecordTierEntry& TierEntry : LoadedRecordData->TierEntries)
		{
			PresentationPaths.Add(TierEntry.TierImage.ToSoftObjectPath());
		}
	}

	if (const UAchievementDefinition* LoadedAchievementData = ResolveAchievementDefinition())
	{
		for (const FAchievementEntry& Achievement : LoadedAchievementData->Achievements)
		{
			PresentationPaths.Add(Achievement.LockedIcon.ToSoftObjectPath());
			PresentationPaths.Add(Achievement.UnlockedIcon.ToSoftObjectPath());
		}
	}

	PresentationPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			PresentationPaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == ContentPreloadGeneration)
					{
						RefreshTierImage();
						RefreshAchievementButtons();
					}
				}));
}

void ULeftProfileWidget::ReleaseContentPreloads()
{
	++ContentPreloadGeneration;

	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(PresentationPreloadHandle);
	ReleaseHandle(DefinitionPreloadHandle);
}

void ULeftProfileWidget::RefreshTierImage()
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		if (Img_Tier)
		{
			Img_Tier->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const FString PlayerId = ResolveProfileSavePlayerId();
	if (PlayerId.IsEmpty())
	{
		if (Img_Tier)
		{
			Img_Tier->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const int32 WinCount = ProfileSubsystem->GetWinCount(PlayerId);
	if (Txt_WinCount)
	{
		Txt_WinCount->SetText(FText::AsNumber(WinCount));
	}
	RefreshAchievementButtons();

	if (!Img_Tier)
	{
		return;
	}

	const URecordDefinition* LoadedRecordData = ResolveRecordDefinition();
	if (!LoadedRecordData)
	{
		Img_Tier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FRecordTierEntry TierEntry = LoadedRecordData->ResolveTierForWinCount(WinCount);
	UTexture2D* TierTexture = TierEntry.TierImage.Get();
	if (!TierTexture)
	{
		Img_Tier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Img_Tier->SetBrushFromTexture(TierTexture, true);
	Img_Tier->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ULeftProfileWidget::RefreshAchievementButtons()
{
	const UAchievementDefinition* LoadedAchievementData = ResolveAchievementDefinition();
	const UGameInstance* GameInstance = GetGameInstance();
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (AchievementSubsystem
		&& !AchievementSubsystem->IsSteamAchievementQueryComplete())
	{
		AchievementSubsystem->RequestSteamAchievementQuery();
	}

	for (int32 AchievementIndex = 0; AchievementIndex < 7; ++AchievementIndex)
	{
		FString AchievementId;
		const bool bHasLocalPresentation = LoadedAchievementData
			&& LoadedAchievementData->Achievements.IsValidIndex(AchievementIndex)
			&& LoadedAchievementData->Achievements[AchievementIndex].bEnabled;
		if (bHasLocalPresentation)
		{
			AchievementId = LoadedAchievementData->Achievements[AchievementIndex].AchievementId;
			AchievementId.TrimStartAndEndInline();
		}
		const bool bHasSteamAchievement = AchievementSubsystem
			&& AchievementSubsystem->HasSteamAchievementData()
			&& !AchievementId.IsEmpty()
			&& AchievementSubsystem->IsSteamAchievementKnown(AchievementId);
		const bool bHasAchievementEntry = bHasLocalPresentation && bHasSteamAchievement;
		const bool bUnlocked = bHasAchievementEntry
			&& AchievementSubsystem->IsSteamAchievementUnlocked(AchievementId);

		if (UButton* Button = GetAchievementButton(AchievementIndex))
		{
			Button->SetVisibility(
				bHasAchievementEntry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Button->SetIsEnabled(bUnlocked);
		}

		if (!bHasAchievementEntry)
		{
			if (UImage* Image = GetAchievementImage(AchievementIndex))
			{
				Image->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}

		UImage* Image = GetAchievementImage(AchievementIndex);
		if (!Image)
		{
			continue;
		}

		const FAchievementEntry& Achievement = LoadedAchievementData->Achievements[AchievementIndex];
		const TSoftObjectPtr<UTexture2D>& Icon = bUnlocked ? Achievement.UnlockedIcon : Achievement.LockedIcon;
		if (UTexture2D* IconTexture = Icon.Get())
		{
			Image->SetBrushFromTexture(IconTexture, true);
			Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	RefreshSelectedAchievementIcon();
}

void ULeftProfileWidget::BindSteamAchievementStateChanged()
{
	UnbindSteamAchievementStateChanged();
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
			&ThisClass::HandleSteamAchievementStateChanged);
	AchievementSubsystem->RequestSteamAchievementQuery();
}

void ULeftProfileWidget::UnbindSteamAchievementStateChanged()
{
	UGameInstance* GameInstance = GetGameInstance();
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (AchievementSubsystem && SteamAchievementStateChangedHandle.IsValid())
	{
		AchievementSubsystem->OnSteamAchievementStateChanged().Remove(
			SteamAchievementStateChangedHandle);
	}
	SteamAchievementStateChangedHandle.Reset();
}

void ULeftProfileWidget::HandleSteamAchievementStateChanged()
{
	RefreshAchievementButtons();
}

void ULeftProfileWidget::BindAchievementButtons()
{
	if (AchievementButton)
	{
		AchievementButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked);
	}
	if (AchievementButton_1)
	{
		AchievementButton_1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_1);
	}
	if (AchievementButton_2)
	{
		AchievementButton_2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_2);
	}
	if (AchievementButton_3)
	{
		AchievementButton_3->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_3);
	}
	if (AchievementButton_4)
	{
		AchievementButton_4->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_4);
	}
	if (AchievementButton_5)
	{
		AchievementButton_5->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_5);
	}
	if (AchievementButton_6)
	{
		AchievementButton_6->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_6);
	}
}

void ULeftProfileWidget::UnbindAchievementButtons()
{
	if (AchievementButton)
	{
		AchievementButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked);
	}
	if (AchievementButton_1)
	{
		AchievementButton_1->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_1);
	}
	if (AchievementButton_2)
	{
		AchievementButton_2->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_2);
	}
	if (AchievementButton_3)
	{
		AchievementButton_3->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_3);
	}
	if (AchievementButton_4)
	{
		AchievementButton_4->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_4);
	}
	if (AchievementButton_5)
	{
		AchievementButton_5->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_5);
	}
	if (AchievementButton_6)
	{
		AchievementButton_6->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_6);
	}
}

void ULeftProfileWidget::BindMatchDisplayNameChanged()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	APdPlayerState* PlayerState = PlayerController ? Cast<APdPlayerState>(PlayerController->PlayerState) : nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (BoundPlayerState.Get() == PlayerState && MatchDisplayNameChangedHandle.IsValid())
	{
		return;
	}

	UnbindMatchDisplayNameChanged();
	BoundPlayerState = PlayerState;
	MatchDisplayNameChangedHandle = PlayerState->GetPlayerMatchComponent()->OnMatchDisplayNameChanged.AddUObject(
		this,
		&ThisClass::HandleMatchDisplayNameChanged);
}

void ULeftProfileWidget::UnbindMatchDisplayNameChanged()
{
	if (APdPlayerState* PlayerState = BoundPlayerState.Get())
	{
		if (MatchDisplayNameChangedHandle.IsValid())
		{
			PlayerState->GetPlayerMatchComponent()->OnMatchDisplayNameChanged.Remove(
				MatchDisplayNameChangedHandle);
		}
	}

	MatchDisplayNameChangedHandle.Reset();
	BoundPlayerState.Reset();
}

bool ULeftProfileWidget::RefreshPlayerName()
{
	if (!Txt_PlayerName)
	{
		return false;
	}

	bool bHasMatchDisplayName = false;
	FText PlayerName = NSLOCTEXT("LeftProfile", "DefaultPlayerName", "Player");

	const APlayerController* PlayerController = GetOwningPlayer();
	if (const APdPlayerState* PlayerState = PlayerController ? Cast<APdPlayerState>(PlayerController->PlayerState) : nullptr)
	{
		const FText MatchDisplayName = PlayerState->GetPlayerMatchComponent()->GetMatchDisplayName();
		if (!MatchDisplayName.IsEmpty())
		{
			PlayerName = MatchDisplayName;
			bHasMatchDisplayName = true;
		}
		else if (!PlayerState->GetPlayerName().IsEmpty())
		{
			PlayerName = FText::FromString(PlayerState->GetPlayerName());
		}
	}

	Txt_PlayerName->SetText(PlayerName);
	return bHasMatchDisplayName;
}

void ULeftProfileWidget::SchedulePlayerNameRefreshRetry()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(PlayerNameRefreshRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PlayerNameRefreshRetryTimerHandle,
		this,
		&ThisClass::HandlePlayerNameRefreshRetry,
		0.1f,
		true);
}

void ULeftProfileWidget::HandlePlayerNameRefreshRetry()
{
	BindMatchDisplayNameChanged();

	const bool bHasMatchDisplayName = RefreshPlayerName();
	++PlayerNameRefreshRetryCount;

	if (bHasMatchDisplayName || PlayerNameRefreshRetryCount >= 30)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PlayerNameRefreshRetryTimerHandle);
		}
	}
}

void ULeftProfileWidget::HandleMatchDisplayNameChanged(const FText& NewDisplayName)
{
	if (Txt_PlayerName)
	{
		Txt_PlayerName->SetText(NewDisplayName.IsEmpty()
			? NSLOCTEXT("LeftProfile", "DefaultPlayerName", "Player")
			: NewDisplayName);
	}
}

void ULeftProfileWidget::HandleAchievementButtonClicked()
{
	ApplyAchievementIcon(0);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_1()
{
	ApplyAchievementIcon(1);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_2()
{
	ApplyAchievementIcon(2);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_3()
{
	ApplyAchievementIcon(3);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_4()
{
	ApplyAchievementIcon(4);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_5()
{
	ApplyAchievementIcon(5);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_6()
{
	ApplyAchievementIcon(6);
}

void ULeftProfileWidget::ApplyAchievementIcon(const int32 AchievementIndex)
{
	if (!IsAchievementUnlocked(AchievementIndex))
	{
		return;
	}

	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (!AchievementDefinition
		|| !AchievementDefinition->Achievements.IsValidIndex(AchievementIndex))
	{
		return;
	}

	FString AchievementId =
		AchievementDefinition->Achievements[AchievementIndex].AchievementId;
	AchievementId.TrimStartAndEndInline();
	if (AchievementId.IsEmpty())
	{
		return;
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	const FString PlayerId = ResolveProfileSavePlayerId();
	if (!ProfileSubsystem
		|| !ProfileSubsystem->SetSelectedAchievementId(
			PlayerId,
			FName(*AchievementId),
			true))
	{
		return;
	}

	ApplyAchievementBrush(AchievementIndex);
	if (APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RequestLocalCosmeticProfileSync();
	}
}

void ULeftProfileWidget::RefreshSelectedAchievementIcon()
{
	if (PlayerAchieveIcon)
	{
		PlayerAchieveIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	const FName SelectedAchievementId =
		ProfileSubsystem->GetSelectedAchievementId(ResolveProfileSavePlayerId());
	if (SelectedAchievementId.IsNone())
	{
		return;
	}

	UAchievementSubsystem* AchievementSubsystem =
		UGameInstance::GetSubsystem<UAchievementSubsystem>(GetGameInstance());
	if (!AchievementSubsystem || !AchievementSubsystem->HasSteamAchievementData())
	{
		return;
	}

	const int32 AchievementIndex = FindAchievementIndexById(SelectedAchievementId);
	if (AchievementIndex != INDEX_NONE && IsAchievementUnlocked(AchievementIndex))
	{
		ApplyAchievementBrush(AchievementIndex);
		return;
	}

	const FString PlayerId = ResolveProfileSavePlayerId();
	if (ProfileSubsystem->SetSelectedAchievementId(PlayerId, NAME_None, true))
	{
		if (APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer()))
		{
			PlayerController->RequestLocalCosmeticProfileSync();
		}
	}
}

void ULeftProfileWidget::ApplyAchievementBrush(const int32 AchievementIndex)
{
	UImage* SourceImage = GetAchievementImage(AchievementIndex);
	if (!SourceImage)
	{
		return;
	}

	const FSlateBrush AchievementBrush = SourceImage->GetBrush();
	if (PlayerAchieveIcon)
	{
		PlayerAchieveIcon->SetBrush(AchievementBrush);
		PlayerAchieveIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (UImage* PlayerAvatarImage = FindHudPlayerAvatarImage())
	{
		PlayerAvatarImage->SetBrush(AchievementBrush);
		PlayerAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

int32 ULeftProfileWidget::FindAchievementIndexById(const FName AchievementId) const
{
	if (AchievementId.IsNone())
	{
		return INDEX_NONE;
	}

	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (!AchievementDefinition)
	{
		return INDEX_NONE;
	}

	for (int32 AchievementIndex = 0;
		AchievementIndex < AchievementDefinition->Achievements.Num();
		++AchievementIndex)
	{
		const FAchievementEntry& Achievement =
			AchievementDefinition->Achievements[AchievementIndex];
		FString CanonicalId = Achievement.AchievementId;
		CanonicalId.TrimStartAndEndInline();
		if (Achievement.bEnabled
			&& !CanonicalId.IsEmpty()
			&& FName(*CanonicalId) == AchievementId)
		{
			return AchievementIndex;
		}
	}

	return INDEX_NONE;
}

bool ULeftProfileWidget::IsAchievementUnlocked(const int32 AchievementIndex) const
{
	const UAchievementDefinition* LoadedAchievementData = ResolveAchievementDefinition();
	if (!LoadedAchievementData || !LoadedAchievementData->Achievements.IsValidIndex(AchievementIndex))
	{
		return false;
	}

	const FAchievementEntry& Achievement = LoadedAchievementData->Achievements[AchievementIndex];
	if (!Achievement.bEnabled)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	return AchievementSubsystem
		&& AchievementSubsystem->HasSteamAchievementData()
		&& AchievementSubsystem->IsSteamAchievementKnown(Achievement.AchievementId)
		&& AchievementSubsystem->IsSteamAchievementUnlocked(Achievement.AchievementId);
}

UButton* ULeftProfileWidget::GetAchievementButton(const int32 AchievementIndex) const
{
	switch (AchievementIndex)
	{
	case 0:
		return AchievementButton;
	case 1:
		return AchievementButton_1;
	case 2:
		return AchievementButton_2;
	case 3:
		return AchievementButton_3;
	case 4:
		return AchievementButton_4;
	case 5:
		return AchievementButton_5;
	case 6:
		return AchievementButton_6;
	default:
		return nullptr;
	}
}

UImage* ULeftProfileWidget::GetAchievementImage(const int32 AchievementIndex) const
{
	switch (AchievementIndex)
	{
	case 0:
		return AchievementImage;
	case 1:
		return AchievementImage_1;
	case 2:
		return AchievementImage_2;
	case 3:
		return AchievementImage_3;
	case 4:
		return AchievementImage_4;
	case 5:
		return AchievementImage_5;
	case 6:
		return AchievementImage_6;
	default:
		return nullptr;
	}
}

UImage* ULeftProfileWidget::FindHudPlayerAvatarImage() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdHUD* HUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	UUserWidget* PlayerHudWidget = HUD ? HUD->GetPlayerHudWidget() : nullptr;
	return FindImageInUserWidget(PlayerHudWidget, TEXT("PlayerAvatar"));
}

UImage* ULeftProfileWidget::FindImageInUserWidget(UUserWidget* RootWidget, const FName ImageName) const
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

UImage* ULeftProfileWidget::FindImageInWidget(UWidget* RootWidget, const FName ImageName) const
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
		const int32 ChildrenCount = PanelWidget->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildrenCount; ++ChildIndex)
		{
			if (UImage* FoundImage = FindImageInWidget(PanelWidget->GetChildAt(ChildIndex), ImageName))
			{
				return FoundImage;
			}
		}
	}

	if (const UContentWidget* ContentWidget = Cast<UContentWidget>(RootWidget))
	{
		if (UImage* FoundImage = FindImageInWidget(ContentWidget->GetContent(), ImageName))
		{
			return FoundImage;
		}
	}

	return nullptr;
}

FString ULeftProfileWidget::ResolveProfileSavePlayerId() const
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return FString();
	}

	FString PlayerId = ProfileSubsystem->GetPreferredSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		return PlayerId;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	PlayerId = ProfileSubsystem->ResolveSavePlayerId(
		PlayerController,
		PlayerState);
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		return PlayerId;
	}

	PlayerId = ProfileSubsystem->GetLocalClientSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	return PlayerId;
}

const URecordDefinition* ULeftProfileWidget::ResolveRecordDefinition()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Record.Get();
}

const UAchievementDefinition* ULeftProfileWidget::ResolveAchievementDefinition() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (UAchievementSubsystem* AchievementSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UAchievementSubsystem>() : nullptr)
	{
		return AchievementSubsystem->GetAchievementDefinition();
	}

	return nullptr;
}
