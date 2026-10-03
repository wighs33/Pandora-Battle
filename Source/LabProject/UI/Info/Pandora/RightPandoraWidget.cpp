#include "UI/Info/Pandora/RightPandoraWidget.h"

#include "Components/Button.h"
#include "Components/TileView.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightPandoraWidget)

void URightPandoraWidget::ApplyWidgetDefinitionSettings()
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	const FRightPandoraWidgetSettings* Settings = WidgetDefinition ? &WidgetDefinition->GetRightPandoraWidgetSettings() : nullptr;

	AddTypeFilter(OffensiveButton, Settings ? Settings->OffensiveTypeTag : FGameplayTag(), &UProjectTagDefinition::GetPandoraOffensiveTypeTag);
	AddTypeFilter(DefensiveButton, Settings ? Settings->DefensiveTypeTag : FGameplayTag(), &UProjectTagDefinition::GetPandoraDefensiveTypeTag);
	AddTypeFilter(SupportButton, Settings ? Settings->SupportTypeTag : FGameplayTag(), &UProjectTagDefinition::GetPandoraSupportTypeTag);
	AddTypeFilter(SpecialButton, Settings ? Settings->SpecialTypeTag : FGameplayTag(), &UProjectTagDefinition::GetPandoraSpecialTypeTag);
}

void URightPandoraWidget::RebuildTileView()
{
	if (!TileView)
	{
		return;
	}

	TileView->ClearListItems();
	for (const TObjectPtr<UObject>& ListItem : CachedSourceListItems)
	{
		UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(ListItem.Get());
		if (PandoraDefinition && MatchesSearch(PandoraDefinition, PandoraDefinition->GetDisplayName(), ActiveSearchText))
		{
			TileView->AddItem(PandoraDefinition);
		}
	}

	// 목록 객체가 같아도 보유 상태는 달라질 수 있으므로 항목의 표시 데이터를 다시 만든다.
	TileView->RegenerateAllEntries();
}
