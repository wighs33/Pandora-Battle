#include "UI/Widget/PandoraSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSlotWidget)

void UPandoraSlotWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	if (UPandoraInstance* PandoraInstance = Cast<UPandoraInstance>(ListItemObject))
	{
		SetData(PandoraInstance);
	}
}

void UPandoraSlotWidget::SetData(UPandoraInstance* Target)
{
	CachedData = Target;

	const UPandoraDefinition* PandoraDefinition = CachedData ? CachedData->PandoraDefinition.Get() : nullptr;

	if (TextBlock)
	{
		TextBlock->SetText(PandoraDefinition ? PandoraDefinition->DisplayName : FText::GetEmpty());
	}

	if (IconImage)
	{
		IconImage->SetBrushFromTexture(PandoraDefinition ? PandoraDefinition->IconTexture : nullptr, false);
	}

	SetIsEnabled(CachedData ? CachedData->IsOwned : false);
}
