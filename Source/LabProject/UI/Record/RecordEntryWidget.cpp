#include "UI/Record/RecordEntryWidget.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"

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
		Txt_Result->SetText(MenuTextOrFallback(Record.bWin ? TEXT("Record.Win") : TEXT("Record.Lose"), Record.bWin ? WinText : LoseText));
		Txt_Result->SetColorAndOpacity(FSlateColor(Record.bWin ? FLinearColor(0.58f, 0.9f, 0.7f) : FLinearColor(1.0f, 0.58f, 0.64f)));
	}
	if (UBorder* Badge = Cast<UBorder>(GetWidgetFromName(TEXT("ResultBadge"))))
		Badge->SetBrushColor(Record.bWin ? FLinearColor(0.06f, 0.22f, 0.16f) : FLinearColor(0.26f, 0.07f, 0.12f));

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