#include "UI/Widget/RecordEntryWidget.h"

#include "Components/TextBlock.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecordEntryWidget)

void URecordEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgets();
	RefreshUI();
}

void URecordEntryWidget::SetRecord(const int32 InDisplayNumber, const FPdMatchRecord& InRecord)
{
	DisplayNumber = FMath::Max(InDisplayNumber, 1);
	Record = InRecord;
	Record.KillCount = FMath::Max(Record.KillCount, 0);
	Record.DeathCount = FMath::Max(Record.DeathCount, 0);
	Record.Reward = FMath::Max(Record.Reward, 0);
	bHasRecord = true;
	RefreshUI();
}

void URecordEntryWidget::RefreshUI()
{
	ResolveWidgets();
	if (!bHasRecord)
	{
		return;
	}

	if (Txt_Number)
	{
		Txt_Number->SetText(FText::AsNumber(DisplayNumber));
	}

	if (Txt_Result)
	{
		Txt_Result->SetText(Record.bWin ? WinText : LoseText);
	}

	if (Txt_Kills)
	{
		Txt_Kills->SetText(FText::AsNumber(Record.KillCount));
	}

	if (Txt_Deaths)
	{
		Txt_Deaths->SetText(FText::AsNumber(Record.DeathCount));
	}

	if (Txt_Reward)
	{
		Txt_Reward->SetText(FText::Format(RewardTextFormat, FText::AsNumber(Record.Reward)));
	}
}

void URecordEntryWidget::ResolveWidgets()
{
	if (!Txt_Number)
	{
		Txt_Number = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_Number"),
			TEXT("Txt_No"),
			TEXT("Txt_Index"),
			TEXT("Txt_RecordNumber")
		});
	}

	if (!Txt_Result)
	{
		Txt_Result = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_Result"),
			TEXT("Txt_WinLose"),
			TEXT("Txt_RecordResult")
		});
	}

	if (!Txt_Kills)
	{
		Txt_Kills = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_Kills"),
			TEXT("Txt_Kill"),
			TEXT("Txt_KillCount")
		});
	}

	if (!Txt_Deaths)
	{
		Txt_Deaths = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_Deaths"),
			TEXT("Txt_Death"),
			TEXT("Txt_DeathCount")
		});
	}

	if (!Txt_Reward)
	{
		Txt_Reward = PdWidgetLookup::FindWidgetByNames<UTextBlock>(WidgetTree, {
			TEXT("Txt_Reward"),
			TEXT("Txt_GoldReward"),
			TEXT("Txt_RecordReward")
		});
	}
}
