#include "UI/Info/Map/MapMarkImages.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "UI/Common/TeamColorUtils.h"
#include "UI/Core/WidgetClassDefinition.h"

namespace
{
	bool HasPositiveSize(const FVector2D& Size)
	{
		return Size.X > 0.0f && Size.Y > 0.0f;
	}

	int32 TeamColorToIndex(const ETeamColor TeamColor)
	{
		return static_cast<int32>(TeamColor);
	}
}

void FMapMarkImages::Configure(const FMapWidgetSettings& Settings)
{
	TeamMarkImagesByTeamColorIndex.Reset();
	TeamMarkImageSizesByTeamColorIndex.Reset();
	CharacterMarkImage = Settings.CharacterMarkImage;
	CharacterMarkImageSize = Settings.CharacterMarkImageSize;

	for (const FMapWidgetTeamMarkImage& TeamMarkImage : Settings.TeamMarkImages)
	{
		const int32 TeamColorIndex = TeamColorToIndex(TeamMarkImage.TeamColor);
		if (!TeamMarkImage.TeamMarkImage.IsNull())
		{
			TeamMarkImagesByTeamColorIndex.Add(
				TeamColorIndex,
				TeamMarkImage.TeamMarkImage);
		}

		if (HasPositiveSize(TeamMarkImage.TeamMarkImageSize))
		{
			TeamMarkImageSizesByTeamColorIndex.Add(TeamColorIndex, TeamMarkImage.TeamMarkImageSize);
		}
	}
}

void FMapMarkImages::SetDesignerTeamMark(const UImage* InDesignerTeamMark)
{
	DesignerTeamMark = InDesignerTeamMark;
}

bool FMapMarkImages::ApplyCharacterMark(UImage* MarkWidget) const
{
	return ApplyImage(MarkWidget, CharacterMarkImage.Get(), CharacterMarkImageSize);
}

bool FMapMarkImages::ApplyTeamMark(
	UImage* MarkWidget,
	const int32 TeamColorIndex) const
{
	if (!MarkWidget)
	{
		return false;
	}

	const int32 NormalizedTeamColorIndex = FMath::Clamp(
		TeamColorIndex,
		0,
		TeamColorToIndex(ETeamColor::Orange));
	const TSoftObjectPtr<UObject>* FoundResource =
		TeamMarkImagesByTeamColorIndex.Find(NormalizedTeamColorIndex);
	UObject* ResourceObject = FoundResource ? FoundResource->Get() : nullptr;
	FVector2D DesiredImageSize = FVector2D::ZeroVector;
	if (const FVector2D* FoundImageSize =
		TeamMarkImageSizesByTeamColorIndex.Find(
			NormalizedTeamColorIndex))
	{
		DesiredImageSize = *FoundImageSize;
	}

	bool bUseTeamColorTint = false;
	if (!ResourceObject)
	{
		ResourceObject = CharacterMarkImage.Get();
		DesiredImageSize = CharacterMarkImageSize;
		bUseTeamColorTint = ResourceObject != nullptr;
	}
	const UImage* DesignerMark = DesignerTeamMark.Get();
	if (!ResourceObject && DesignerMark)
	{
		const FSlateBrush& DesignerBrush = DesignerMark->GetBrush();
		ResourceObject = DesignerBrush.GetResourceObject();
		DesiredImageSize = DesignerBrush.ImageSize;
		bUseTeamColorTint = ResourceObject != nullptr;
	}
	if (!ResourceObject)
	{
		return false;
	}

	MarkWidget->SetColorAndOpacity(
		bUseTeamColorTint
			? LabTeamColorUtils::GetTeamColor(
				NormalizedTeamColorIndex)
			: FLinearColor::White);
	return ApplyImage(
		MarkWidget,
		ResourceObject,
		DesiredImageSize);
}

FVector2D FMapMarkImages::GetDefaultMarkSize()
{
	return FVector2D(32.0f, 32.0f);
}

bool FMapMarkImages::ApplyImage(UImage* MarkWidget, UObject* ResourceObject, const FVector2D& DesiredImageSize)
{
	if (!MarkWidget || !ResourceObject)
	{
		return false;
	}

	FSlateBrush MarkBrush = MarkWidget->GetBrush();
	MarkBrush.SetResourceObject(ResourceObject);

	if (HasPositiveSize(DesiredImageSize))
	{
		MarkBrush.ImageSize = DesiredImageSize;
	}
	else if (!HasPositiveSize(MarkBrush.ImageSize))
	{
		MarkBrush.ImageSize = GetDefaultMarkSize();
	}

	MarkWidget->SetBrush(MarkBrush);

	const FVector2D SlotSize = HasPositiveSize(DesiredImageSize) ? DesiredImageSize : MarkBrush.ImageSize;
	if (HasPositiveSize(SlotSize))
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkWidget->Slot))
		{
			CanvasSlot->SetSize(SlotSize);
		}
	}

	return true;
}
