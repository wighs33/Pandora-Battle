#include "UI/Record/RecordWidget.h"
#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Profile/PlayerProfileSubsystem.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/UI/RecordDefinition.h"
#include "UI/Record/RecordEntryWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecordWidget)

URecordWidget::URecordWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URecordWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	ApplyWidgetDefinitionSettings();

	BindWidgets();
	BeginContentPreload();
	RefreshRecords();
}

void URecordWidget::NativeDestruct()
{
	TierImageLease.Reset();
	RecordDefinitionLease.Reset();
	UnbindWidgets();
	Super::NativeDestruct();
}

void URecordWidget::BeginContentPreload()
{
	TierImageLease.Reset();
	RecordDefinitionLease.Reset();

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const TSoftObjectPtr<URecordDefinition>& RecordDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Record;
	if (!ContentSubsystem || RecordDefinition.IsNull())
	{
		return;
	}

	RecordDefinitionLease = ContentSubsystem->AcquireContent(
		{RecordDefinition.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::BeginTierImagePreload));
}

void URecordWidget::BeginTierImagePreload()
{
	const URecordDefinition* LoadedRecordData = ResolveRecordDefinition();
	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!LoadedRecordData || !ContentSubsystem)
	{
		RefreshRecords();
		return;
	}

	TArray<FSoftObjectPath> TierImagePaths;
	for (const FRecordTierEntry& TierEntry : LoadedRecordData->TierEntries)
	{
		TierImagePaths.Add(TierEntry.TierImage.ToSoftObjectPath());
	}

	TierImageLease = ContentSubsystem->AcquireContent(
		TierImagePaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshRecords));
}

void URecordWidget::RefreshRecords()
{
	ApplyWidgetDefinitionSettings();

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	if (!ProfileSubsystem)
	{
		return;
	}

	ApplyWinCountUI();
	ApplyTierImage();

	if (!RecordScrollBox)
	{
		return;
	}

	RecordScrollBox->ClearChildren();

	const TSubclassOf<URecordEntryWidget> ResolvedEntryClass = ResolveRecordEntryWidgetClass();
	if (!ResolvedEntryClass)
	{
		return;
	}

	const TArray<FMatchRecord> MatchRecords = ProfileSubsystem->GetMatchRecords();
	const int32 VisibleCount = FMath::Min(MatchRecords.Num(), MaxVisibleRecordEntries);
	if (Txt_EmptyRecords)
		Txt_EmptyRecords->SetVisibility(VisibleCount == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	for (int32 DisplayIndex = 0; DisplayIndex < VisibleCount; ++DisplayIndex)
	{
		const int32 SourceIndex = MatchRecords.Num() - 1 - DisplayIndex;
		if (!MatchRecords.IsValidIndex(SourceIndex))
		{
			continue;
		}

		URecordEntryWidget* EntryWidget = CreateWidget<URecordEntryWidget>(
			GetOwningPlayer(),
			ResolvedEntryClass);
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetRecord(DisplayIndex + 1, MatchRecords[SourceIndex]);
		RecordScrollBox->AddChild(EntryWidget);
	}
}

void URecordWidget::HandleCloseClicked()
{
	if (UCommonActivatableWidget* Screen = UCommonUIActionRouterBase::FindOwningActivatable(GetCachedWidget(), GetOwningLocalPlayer()))
		Screen->DeactivateWidget();
	RemoveFromParent();
}

void URecordWidget::OnMenuLanguageChanged()
{
	// Entries subscribe independently. Keep the existing rows and scroll position alive.

	ApplyWinCountUI();
	ApplyTierImage();
}

void URecordWidget::BindWidgets()
{
	if (bWidgetsBound)
	{
		return;
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	bWidgetsBound = true;
}

void URecordWidget::UnbindWidgets()
{
	if (!bWidgetsBound)
	{
		return;
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	bWidgetsBound = false;
}

void URecordWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FRecordWidgetSettings& Settings = WidgetDefinition->GetRecordWidgetSettings();
		if (Settings.RecordEntryWidgetClass)
		{
			RecordEntryWidgetClass = Settings.RecordEntryWidgetClass;
		}
		MaxVisibleRecordEntries = FMath::Max(Settings.MaxVisibleRecordEntries, 1);
		MaxTierProgressWinCount = FMath::Max(Settings.MaxTierProgressWinCount, 1.0f);
	}
}

TSubclassOf<URecordEntryWidget> URecordWidget::ResolveRecordEntryWidgetClass() const
{
	if (RecordEntryWidgetClass)
	{
		return RecordEntryWidgetClass;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		return WidgetDefinition->GetRecordEntryWidgetClass();
	}

	return nullptr;
}

const URecordDefinition* URecordWidget::ResolveRecordDefinition()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Record.Get();
}

void URecordWidget::ApplyTierImage()
{
	if (!Img_MyTier)
	{
		return;
	}

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	const URecordDefinition* LoadedRecordData = ResolveRecordDefinition();
	if (!ProfileSubsystem || !LoadedRecordData)
	{
		Img_MyTier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 WinCount = ProfileSubsystem->GetWinCount();
	const FRecordTierEntry TierEntry = LoadedRecordData->ResolveTierForWinCount(WinCount);
	if (Txt_TierName)
		Txt_TierName->SetText(FText::Format(MenuText(TEXT("Record.Tier")), FText::AsNumber(TierEntry.RankOrder + 1)));
	UTexture2D* TierTexture = TierEntry.TierImage.Get();
	if (!TierTexture)
	{
		Img_MyTier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Img_MyTier->SetBrushFromTexture(TierTexture, true);
	Img_MyTier->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URecordWidget::ApplyWinCountUI()
{
	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GetGameInstance());
	const int32 WinCount = ProfileSubsystem ? ProfileSubsystem->GetWinCount() : 0;

	if (Txt_WinCount)
	{
		Txt_WinCount->SetText(FText::AsNumber(WinCount));
	}

	if (TierProgressBar)
	{
		TierProgressBar->SetPercent(FMath::Clamp(static_cast<float>(WinCount) / MaxTierProgressWinCount, 0.0f, 1.0f));
	}
	if (Txt_ProgressFraction)
		Txt_ProgressFraction->SetText(FText::Format(FText::FromString(TEXT("{0} / {1}")), FText::AsNumber(WinCount), FText::AsNumber(FMath::RoundToInt(MaxTierProgressWinCount))));
	if (Txt_RecentCount)
		Txt_RecentCount->SetText(FText::Format(MenuText(TEXT("Record.RecentCount")), FText::AsNumber(MaxVisibleRecordEntries)));
}
