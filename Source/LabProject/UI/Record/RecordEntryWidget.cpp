#include "UI/Record/RecordEntryWidget.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RecordEntryWidget)

void URecordEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RefreshUI();
}

void URecordEntryWidget::SetRecord(const int32 InDisplayNumber, const FMatchRecord& InRecord)
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
		Txt_Result->SetText(MenuText(Record.bWin ? TEXT("Record.Win") : TEXT("Record.Lose")));
		Txt_Result->SetColorAndOpacity(FSlateColor(Record.bWin ? WinResultColor : LoseResultColor));
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
		Txt_Reward->SetText(FText::AsNumber(Record.Reward));
	}
}