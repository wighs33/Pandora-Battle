#include "UI/Info/Status/LeftProfileWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UI/HUD/PdHUD.h"
#include "UI/HUD/Player/PlayerHudWidget.h"
#include "Profile/PlayerProfileSubsystem.h"
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
	if (UAchievementSubsystem* AchievementSubsystem = AchievementSubscription.Subscribe(GetGameInstance(),
		FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshAchievementButtons)))
	{
		AchievementSubsystem->RequestSteamAchievementQuery();
	}
	BeginContentPreload();
	RefreshTierImage();
	RefreshAchievementButtons();

	// 이름 변경은 OnMatchDisplayNameChanged가, PlayerState 도착은 조종 캐릭터 준비 알림이 다시 반영한다.
	BindMatchDisplayNameChanged();
	RefreshPlayerName();
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayer(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
}

void ULeftProfileWidget::NativeDestruct()
{
	PossessedCharacterReadySubscription.Reset();
	PresentationLease.Reset();
	DefinitionLease.Reset();
	UnbindMatchDisplayNameChanged();
	AchievementSubscription.Reset();
	UnbindAchievementButtons();
	Super::NativeDestruct();
}

void ULeftProfileWidget::BeginContentPreload()
{
	PresentationLease.Reset();
	DefinitionLease.Reset();

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	DefinitionLease = ContentSubsystem->AcquireContent({
			UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Record.ToSoftObjectPath(),
			UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Achievement.ToSoftObjectPath()
		},
		FSimpleDelegate::CreateUObject(this, &ThisClass::BeginPresentationPreload));
}

void ULeftProfileWidget::BeginPresentationPreload()
{
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

	PresentationLease = ContentSubsystem->AcquireContent(PresentationPaths,
		FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				RefreshTierImage();
				RefreshAchievementButtons();
			}));
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

	const int32 WinCount = ProfileSubsystem->GetWinCount();
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
	bAchievementQueryPending = false;
	if (AchievementSubsystem && !AchievementSubsystem->IsSteamAchievementQueryComplete())
	{
		bAchievementQueryPending = AchievementSubsystem->RequestSteamAchievementQuery()
			&& !AchievementSubsystem->IsSteamAchievementQueryComplete();
	}

	for (int32 AchievementIndex = 0; AchievementIndex < 7; ++AchievementIndex)
	{
		FString AchievementId;
		const bool bHasLocalPresentation = LoadedAchievementData
			&& LoadedAchievementData->Achievements.IsValidIndex(AchievementIndex)
			&& LoadedAchievementData->Achievements[AchievementIndex].bEnabled;
		if (bHasLocalPresentation)
		{
			AchievementId = UAchievementDefinition::NormalizeAchievementId(
				LoadedAchievementData->Achievements[AchievementIndex].AchievementId);
		}
		const bool bHasSteamAchievement = AchievementSubsystem && AchievementSubsystem->HasSteamAchievementData()
			&& !AchievementId.IsEmpty() && AchievementSubsystem->IsSteamAchievementKnown(AchievementId);
		const bool bHasAchievementEntry = bHasLocalPresentation && bHasSteamAchievement;
		const bool bUnlocked = bHasAchievementEntry && AchievementSubsystem->IsSteamAchievementUnlocked(AchievementId);

		if (UButton* Button = GetAchievementButton(AchievementIndex))
		{
			Button->SetVisibility(bHasAchievementEntry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
	RefreshAchievementMessage();
}

void ULeftProfileWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	RefreshAchievementMessage();
}

void ULeftProfileWidget::RefreshAchievementMessage()
{
	UTextBlock* Message = Cast<UTextBlock>(GetWidgetFromName(TEXT("Profile_AchievementState")));
	if (!Message)
	{
		return;
	}

	for (int32 Index = 0; Index < 7; ++Index)
	{
		const UButton* Button = GetAchievementButton(Index);
		if (Button && Button->GetVisibility() == ESlateVisibility::Visible)
		{
			Message->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
	}

	Message->SetText(MenuText(bAchievementQueryPending
		? TEXT("Profile.AchievementsLoading") : TEXT("Profile.AchievementsUnavailable")));
	Message->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void ULeftProfileWidget::UpdateAchievementSelection(const int32 AchievementIndex)
{
	for (int32 Index = 0; Index < 7; ++Index)
	{
		const FName MarkerName(*FString::Printf(TEXT("Profile_AchievementSelection_%d"), Index));
		if (UWidget* Marker = GetWidgetFromName(MarkerName))
		{
			Marker->SetVisibility(Index == AchievementIndex
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
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
	MatchDisplayNameChangedHandle = PlayerState->GetPlayerMatchComponent()->OnMatchDisplayNameChanged.AddUObject(this,
		&ThisClass::HandleMatchDisplayNameChanged);
}

void ULeftProfileWidget::UnbindMatchDisplayNameChanged()
{
	if (APdPlayerState* PlayerState = BoundPlayerState.Get())
	{
		if (MatchDisplayNameChangedHandle.IsValid())
		{
			PlayerState->GetPlayerMatchComponent()->OnMatchDisplayNameChanged.Remove(MatchDisplayNameChangedHandle);
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
	FText PlayerName = FText::FromString(TEXT("Player"));

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

void ULeftProfileWidget::HandlePossessedCharacterReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
	BindMatchDisplayNameChanged();
	RefreshPlayerName();
}

void ULeftProfileWidget::HandleMatchDisplayNameChanged(const FText& NewDisplayName)
{
	if (Txt_PlayerName)
	{
		Txt_PlayerName->SetText(NewDisplayName.IsEmpty() ? FText::FromString(TEXT("Player")) : NewDisplayName);
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
	if (!AchievementDefinition || !AchievementDefinition->Achievements.IsValidIndex(AchievementIndex))
	{
		return;
	}

	const FString AchievementId = UAchievementDefinition::NormalizeAchievementId(
		AchievementDefinition->Achievements[AchievementIndex].AchievementId);
	if (AchievementId.IsEmpty())
	{
		return;
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());

	if (!ProfileSubsystem || !ProfileSubsystem->SetSelectedAchievementId(FName(*AchievementId), true))
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
	UpdateAchievementSelection(INDEX_NONE);
	if (PlayerAchieveIcon)
	{
		PlayerAchieveIcon->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	const FName SelectedAchievementId = ProfileSubsystem->GetSelectedAchievementId();
	if (SelectedAchievementId.IsNone())
	{
		return;
	}

	UAchievementSubsystem* AchievementSubsystem = UGameInstance::GetSubsystem<UAchievementSubsystem>(GetGameInstance());
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

	if (ProfileSubsystem->SetSelectedAchievementId(NAME_None, true))
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
	UpdateAchievementSelection(AchievementIndex);
	if (PlayerAchieveIcon)
	{
		PlayerAchieveIcon->SetBrush(AchievementBrush);
		PlayerAchieveIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// HUD 아바타는 HUD가 선택된 업적으로 직접 다시 그린다.
	const APdHUD* HUD = GetOwningPlayer() ? Cast<APdHUD>(GetOwningPlayer()->GetHUD()) : nullptr;
	if (UPlayerHudWidget* PlayerHudWidget = HUD ? Cast<UPlayerHudWidget>(HUD->GetPlayerHudWidget()) : nullptr)
	{
		PlayerHudWidget->RefreshAchievementAvatar();
	}
}

int32 ULeftProfileWidget::FindAchievementIndexById(const FName AchievementId) const
{
	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	return AchievementDefinition ? AchievementDefinition->FindEnabledAchievementIndex(AchievementId) : INDEX_NONE;
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
	return AchievementSubsystem && AchievementSubsystem->HasSteamAchievementData()
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
