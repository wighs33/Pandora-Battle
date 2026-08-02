#include "UI/Widget/KillLogEntryWidget.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(KillLogEntryWidget)

void UKillLogEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshUI();
}

void UKillLogEntryWidget::SetInfo(const FKillLogEntry& InKillLogEntry)
{
	KillLogEntry = InKillLogEntry;
	bHasKillLogEntry = true;
	RefreshUI();
}

void UKillLogEntryWidget::RefreshUI()
{
	if (!bHasKillLogEntry)
	{
		return;
	}

	const bool bUseSeparatedNameTextBlocks = ShouldUseSeparatedNameTextBlocks();

	if (Txt_KillerName)
	{
		Txt_KillerName->SetText(KillLogEntry.KillerName);
		Txt_KillerName->SetColorAndOpacity(FSlateColor(KillerNameColor));
		Txt_KillerName->SetVisibility(bUseSeparatedNameTextBlocks ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Txt_VictimName)
	{
		Txt_VictimName->SetText(KillLogEntry.VictimName);
		Txt_VictimName->SetColorAndOpacity(FSlateColor(VictimNameColor));
		Txt_VictimName->SetVisibility(bUseSeparatedNameTextBlocks ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (Txt_KillMessage)
	{
		Txt_KillMessage->SetText(bUseSeparatedNameTextBlocks ? KillConnectorText : BuildKillMessage());
		Txt_KillMessage->SetColorAndOpacity(FSlateColor(MessageColor));
	}
}

FText UKillLogEntryWidget::BuildKillMessage() const
{
	if (KillLogEntry.bEnvironmentKill)
	{
		return FText::Format(EnvironmentKillMessageFormat, KillLogEntry.VictimName);
	}

	if (KillLogEntry.bSelfKill)
	{
		return FText::Format(SelfKillMessageFormat, KillLogEntry.VictimName);
	}

	return FText::Format(KillMessageFormat, KillLogEntry.KillerName, KillLogEntry.VictimName);
}

bool UKillLogEntryWidget::ShouldUseSeparatedNameTextBlocks() const
{
	return Txt_KillerName
		&& Txt_VictimName
		&& !KillLogEntry.bSelfKill
		&& !KillLogEntry.bEnvironmentKill;
}
