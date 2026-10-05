#include "UI/Info/Map/MapPawnMarkers.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "UI/Info/Map/MapMarkImages.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapPawnMarkers)

namespace
{
	// 다른 플레이어 표식 위에 내 팀 표식, 그 위에 내 캐릭터 표식이 오도록 쌓는다.
	constexpr int32 RemoteTeamMarkZOrder = 10000;
	constexpr int32 SelfTeamMarkZOrder = 10010;
	constexpr int32 SelfCharacterMarkZOrder = 10011;
}

FVector2D FMapMarkProjection::ToMapPosition(const FVector& WorldLocation) const
{
	const FVector PlaneLocalLocation = PlaneTransform.InverseTransformPosition(WorldLocation);
	const float SafePlaneLocalSize = FMath::Max(PlaneLocalSize, 1.0f);
	const float NormalizedX = FMath::Clamp((PlaneLocalLocation.X / SafePlaneLocalSize) + 0.5f, 0.0f, 1.0f);
	const float NormalizedY = FMath::Clamp((PlaneLocalLocation.Y / SafePlaneLocalSize) + 0.5f, 0.0f, 1.0f);
	return FVector2D(NormalizedX * MapSize.X, NormalizedY * MapSize.Y);
}

float FMapMarkProjection::ToMarkAngle(const FVector& WorldForward) const
{
	float RotationAngle = RotationOffsetDegrees;
	if (bRotateToForward)
	{
		const FVector PlaneLocalForward = PlaneTransform.InverseTransformVectorNoScale(WorldForward);
		FVector2D ScreenForward(PlaneLocalForward.X, PlaneLocalForward.Y);
		if (!ScreenForward.Normalize())
		{
			ScreenForward = FVector2D(0.0f, 1.0f);
		}

		RotationAngle += -FMath::RadiansToDegrees(FMath::Atan2(ScreenForward.X, ScreenForward.Y));
	}
	return RotationAngle;
}

void FMapPawnMarkers::UpdateSelfMarks(const FMapMarkerCanvas& Canvas, const APawn& OwningPawn,
	const bool bOnShownRegion)
{
	bool bTeamMarkVisible = false;
	if (!SelfTeamMark)
	{
		SelfTeamMark = CreateMark(Canvas, TEXT("SelfTeamMark"), SelfTeamMarkZOrder);
	}

	if (SelfTeamMark)
	{
		bTeamMarkVisible = ApplyTeamMark(SelfTeamMark, OwningPawn, *Canvas.MarkImages) && bOnShownRegion
			&& PlaceMark(SelfTeamMark, OwningPawn, Canvas.Projection, true);

		if (!bTeamMarkVisible)
		{
			SelfTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!SelfCharacterMark)
	{
		SelfCharacterMark = CreateMark(Canvas, TEXT("SelfCharacterMark"), SelfCharacterMarkZOrder);
	}

	if (SelfCharacterMark)
	{
		const bool bCharacterMarkVisible = bTeamMarkVisible && Canvas.MarkImages->ApplyCharacterMark(SelfCharacterMark)
			&& PlaceMark(SelfCharacterMark, OwningPawn, Canvas.Projection, false);

		if (!bCharacterMarkVisible)
		{
			SelfCharacterMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void FMapPawnMarkers::UpdateRemoteMarks(const FMapMarkerCanvas& Canvas,
	const TConstArrayView<const APawn*> PawnsOnShownRegion)
{
	EnsureRemoteMarkCount(Canvas, PawnsOnShownRegion.Num());

	int32 VisibleMarkCount = 0;
	for (const APawn* RemotePawn : PawnsOnShownRegion)
	{
		if (!RemoteTeamMarks.IsValidIndex(VisibleMarkCount))
		{
			break;
		}

		UImage* CurrentTeamMark = RemoteTeamMarks[VisibleMarkCount];
		if (ApplyTeamMark(CurrentTeamMark, *RemotePawn, *Canvas.MarkImages)
			&& PlaceMark(CurrentTeamMark, *RemotePawn, Canvas.Projection, true))
		{
			++VisibleMarkCount;
		}
		else if (CurrentTeamMark)
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	for (int32 Index = VisibleMarkCount; Index < RemoteTeamMarks.Num(); ++Index)
	{
		if (UImage* CurrentTeamMark = RemoteTeamMarks[Index])
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void FMapPawnMarkers::HideSelfMarks() const
{
	if (SelfTeamMark)
	{
		SelfTeamMark->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SelfCharacterMark)
	{
		SelfCharacterMark->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void FMapPawnMarkers::HideRemoteMarks() const
{
	for (UImage* CurrentTeamMark : RemoteTeamMarks)
	{
		if (CurrentTeamMark)
		{
			CurrentTeamMark->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void FMapPawnMarkers::RemoveAll()
{
	if (SelfTeamMark)
	{
		SelfTeamMark->RemoveFromParent();
		SelfTeamMark = nullptr;
	}

	if (SelfCharacterMark)
	{
		SelfCharacterMark->RemoveFromParent();
		SelfCharacterMark = nullptr;
	}

	for (UImage* RemoteTeamMark : RemoteTeamMarks)
	{
		if (RemoteTeamMark)
		{
			RemoteTeamMark->RemoveFromParent();
		}
	}
	RemoteTeamMarks.Reset();
}

void FMapPawnMarkers::EnsureRemoteMarkCount(const FMapMarkerCanvas& Canvas, const int32 RequiredCount)
{
	while (RemoteTeamMarks.Num() < RequiredCount)
	{
		UImage* NewTeamMark = CreateMark(Canvas,
			*FString::Printf(TEXT("RemoteTeamMark_%d"), RemoteTeamMarks.Num()),
			RemoteTeamMarkZOrder);
		if (!NewTeamMark)
		{
			return;
		}

		RemoteTeamMarks.Add(NewTeamMark);
	}
}

UImage* FMapPawnMarkers::CreateMark(const FMapMarkerCanvas& Canvas, const FName MarkName, const int32 ZOrder)
{
	UPanelWidget* ParentPanel = Canvas.ParentPanel;
	if (!ParentPanel)
	{
		return nullptr;
	}

	UImage* NewMark = NewObject<UImage>(Canvas.Outer, UImage::StaticClass(), MarkName);
	if (!NewMark)
	{
		return nullptr;
	}

	NewMark->SetColorAndOpacity(FLinearColor::White);
	NewMark->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	NewMark->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanel* CanvasParent = Cast<UCanvasPanel>(ParentPanel))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasParent->AddChildToCanvas(NewMark);
		if (!CanvasSlot)
		{
			return nullptr;
		}

		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetSize(FMapMarkImages::GetDefaultMarkSize());
		CanvasSlot->SetZOrder(ZOrder);
	}
	else
	{
		if (!ParentPanel->AddChild(NewMark))
		{
			return nullptr;
		}
	}

	return NewMark;
}

bool FMapPawnMarkers::ApplyTeamMark(UImage* Mark, const APawn& Pawn, const FMapMarkImages& MarkImages)
{
	if (!Mark)
	{
		return false;
	}

	const APdPlayerState* PdPlayerState = Pawn.GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return false;
	}

	return MarkImages.ApplyTeamMark(Mark, PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex());
}

bool FMapPawnMarkers::PlaceMark(UImage* Mark, const APawn& Pawn, const FMapMarkProjection& Projection,
	const bool bRotateToPawnForward)
{
	if (!Mark)
	{
		return false;
	}

	// 앵커·정렬·회전 중심은 CreateMark에서 정해 두었으므로 위치와 각도만 갱신한다.
	const FVector2D MarkPosition = Projection.ToMapPosition(Pawn.GetActorLocation());
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Mark->Slot))
	{
		CanvasSlot->SetPosition(MarkPosition);
	}
	else
	{
		Mark->SetRenderTranslation(MarkPosition);
	}

	Mark->SetRenderTransformAngle(bRotateToPawnForward ? Projection.ToMarkAngle(Pawn.GetActorForwardVector()) : 0.0f);
	Mark->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}
