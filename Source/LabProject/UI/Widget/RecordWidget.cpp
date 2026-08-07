#include "UI/Widget/RecordWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdGameInstance.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/UI/RecordDefinition.h"
#include "UI/WidgetLookup.h"
#include "UI/Widget/RecordEntryWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecordWidget)

URecordWidget::URecordWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URecordWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyWidgetDefinitionSettings();
	ResolveWidgets();
	BindWidgets();
	BeginContentPreload();
	RefreshRecords();
}

void URecordWidget::NativeDestruct()
{
	ReleaseContentPreloads();
	UnbindWidgets();
	Super::NativeDestruct();
}

void URecordWidget::BeginContentPreload()
{
	ReleaseContentPreloads();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	const TSoftObjectPtr<URecordDefinition>& RecordDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Record;
	if (!ContentSubsystem || RecordDefinition.IsNull())
	{
		return;
	}

	RecordDefinitionPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{RecordDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					BeginTierImagePreload(PreloadGeneration);
				}));
}

void URecordWidget::BeginTierImagePreload(const int32 PreloadGeneration)
{
	if (PreloadGeneration != ContentPreloadGeneration)
	{
		return;
	}

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

	TierImagePreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			TierImagePaths,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, PreloadGeneration]()
				{
					if (PreloadGeneration == ContentPreloadGeneration)
					{
						RefreshRecords();
					}
				}));
}

void URecordWidget::ReleaseContentPreloads()
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

	ReleaseHandle(TierImagePreloadHandle);
	ReleaseHandle(RecordDefinitionPreloadHandle);
}

void URecordWidget::RefreshRecords()
{
	ApplyWidgetDefinitionSettings();
	ResolveWidgets();

	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{

		return;
	}

	const FString PlayerId = ResolveRecordPlayerId();
	if (PlayerId.IsEmpty())
	{

		return;
	}

	ApplyWinCountUI(PlayerId);
	ApplyTierImage(PlayerId);

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

	const TArray<FMatchRecord> MatchRecords = PdGameInstance->GetMatchRecords(PlayerId);
	const int32 VisibleCount = FMath::Min(MatchRecords.Num(), MaxVisibleRecordEntries);
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
	RemoveFromParent();
}

void URecordWidget::ResolveWidgets()
{
	if (!RecordScrollBox)
	{
		RecordScrollBox = PdWidgetLookup::FindWidgetByNames<UPanelWidget>(WidgetTree, {
			TEXT("RecordScrollBox"),
			TEXT("ScrollBox_Record"),
			TEXT("SB_Record"),
			TEXT("RecordsContainer")
		});
	}

	if (!Btn_Close)
	{
		Btn_Close = PdWidgetLookup::FindWidgetByNames<UButton>(WidgetTree, {
			TEXT("Btn_Close"),
			TEXT("Btn_Exit"),
			TEXT("Btn_Back")
		});
	}

	if (!Img_MyTier)
	{
		Img_MyTier = PdWidgetLookup::FindWidgetByNames<UImage>(WidgetTree, {
			TEXT("Img_MyTier"),
			TEXT("Img_Tier"),
			TEXT("Img_RecordTier")
		});
	}

	if (!Txt_WinCount)
	{
		Txt_WinCount = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_WinCount"),
			TEXT("Txt_Wins"),
			TEXT("Txt_TotalWins")
		});
	}

	if (!TierProgressBar)
	{
		TierProgressBar = PdWidgetLookup::FindWidgetByNames<UProgressBar>(WidgetTree, {
			TEXT("TierProgressBar"),
			TEXT("PB_TierProgress"),
			TEXT("ProgressBar_Tier")
		});
	}
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

FString URecordWidget::ResolveRecordPlayerId() const
{
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return FString();
	}

	FString PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		return PlayerId;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	PlayerId = PdGameInstance->ResolveSavePlayerId(
		PlayerController,
		PlayerState);
	PlayerId.TrimStartAndEndInline();
	if (!PlayerId.IsEmpty())
	{
		return PlayerId;
	}

	PlayerId = PdGameInstance->GetLocalClientSavePlayerId();
	PlayerId.TrimStartAndEndInline();
	return PlayerId;
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

void URecordWidget::ApplyTierImage(const FString& PlayerId)
{
	if (!Img_MyTier)
	{
		return;
	}

	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	const URecordDefinition* LoadedRecordData = ResolveRecordDefinition();
	if (!PdGameInstance || !LoadedRecordData)
	{
		Img_MyTier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 WinCount = PdGameInstance->GetWinCount(PlayerId);
	const FRecordTierEntry TierEntry = LoadedRecordData->ResolveTierForWinCount(WinCount);
	UTexture2D* TierTexture = TierEntry.TierImage.Get();
	if (!TierTexture)
	{
		Img_MyTier->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Img_MyTier->SetBrushFromTexture(TierTexture, true);
	Img_MyTier->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URecordWidget::ApplyWinCountUI(const FString& PlayerId)
{
	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	const int32 WinCount = PdGameInstance ? PdGameInstance->GetWinCount(PlayerId) : 0;

	if (Txt_WinCount)
	{
		Txt_WinCount->SetText(FText::AsNumber(WinCount));
	}

	if (TierProgressBar)
	{
		TierProgressBar->SetPercent(FMath::Clamp(static_cast<float>(WinCount) / MaxTierProgressWinCount, 0.0f, 1.0f));
	}
}
