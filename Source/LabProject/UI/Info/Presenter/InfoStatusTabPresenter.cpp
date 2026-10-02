#include "UI/Info/Presenter/InfoStatusTabPresenter.h"

#include "Component/Player/StatUpgradeComponent.h"
#include "Mode/PdPlayerState.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Status/RightStatusWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoStatusTabPresenter)

void UInfoStatusTabPresenter::BindInfoUi(UInfoWidget* InInfoWidget)
{
	if (GetInfoWidget() != InInfoWidget)
	{
		UnbindEvents();
	}

	Super::BindInfoUi(InInfoWidget);
	BindEvents();
}

void UInfoStatusTabPresenter::Deinitialize()
{
	UnbindEvents();
	Super::Deinitialize();
}

void UInfoStatusTabPresenter::Activate()
{
	BindEvents();
}

void UInfoStatusTabPresenter::BindEvents()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightStatusWidget* StatusWidget = InfoWidget ? InfoWidget->GetRightStatusWidget() : nullptr;
	if (!StatusWidget)
	{
		return;
	}

	StatusWidget->OnClicked_StatUpButton.RemoveDynamic(this, &ThisClass::HandleStatUpClicked);
	StatusWidget->OnClicked_StatUpButton.AddUniqueDynamic(this, &ThisClass::HandleStatUpClicked);
	StatusWidget->OnClicked_StatDownButton.RemoveDynamic(this, &ThisClass::HandleStatDownClicked);
	StatusWidget->OnClicked_StatDownButton.AddUniqueDynamic(this, &ThisClass::HandleStatDownClicked);
}

void UInfoStatusTabPresenter::UnbindEvents()
{
	UInfoWidget* InfoWidget = GetInfoWidget();
	URightStatusWidget* StatusWidget = InfoWidget ? InfoWidget->GetRightStatusWidget() : nullptr;
	if (!StatusWidget)
	{
		return;
	}

	StatusWidget->OnClicked_StatUpButton.RemoveDynamic(this, &ThisClass::HandleStatUpClicked);
	StatusWidget->OnClicked_StatDownButton.RemoveDynamic(this, &ThisClass::HandleStatDownClicked);
}

void UInfoStatusTabPresenter::HandleStatUpClicked(const FGameplayTag StatTag)
{
	APdPlayerState* PlayerState = GetPlayerState();
	if (UStatUpgradeComponent* StatUpgrade = PlayerState ? PlayerState->GetStatUpgradeComponent() : nullptr)
	{
		StatUpgrade->RequestStatUp(StatTag);
	}
}

void UInfoStatusTabPresenter::HandleStatDownClicked(const FGameplayTag StatTag)
{
	APdPlayerState* PlayerState = GetPlayerState();
	if (UStatUpgradeComponent* StatUpgrade = PlayerState ? PlayerState->GetStatUpgradeComponent() : nullptr)
	{
		StatUpgrade->RequestStatDown(StatTag);
	}
}
