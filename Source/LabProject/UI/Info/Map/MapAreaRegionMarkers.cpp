#include "UI/Info/Map/MapAreaRegionMarkers.h"

#include "Common/PlayerMapRegion.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "GameFramework/GameStateBase.h"
#include "Mode/PdPlayerState.h"
#include "UI/Info/Map/MapMarkImages.h"

namespace
{
	// 플레이어 ID·지역·팀 색을 한 값으로 묶어 표식 줄을 다시 만들어야 하는지 비교한다.
	uint64 MakeMarkerStateKey(const APdPlayerState& PlayerState)
	{
		const UPlayerMatchComponent* MatchComponent =
			PlayerState.GetPlayerMatchComponent();
		const uint32 PlayerId = static_cast<uint32>(
			PlayerState.GetPlayerId());
		const uint16 TeamColorIndex = static_cast<uint16>(
			MatchComponent
				? MatchComponent->GetMatchTeamColorIndex() + 1
				: 0);
		const uint8 MapRegion = static_cast<uint8>(
			MatchComponent
				? MatchComponent->GetPlayerMapRegion()
				: EPlayerMapRegion::Dome);
		return (static_cast<uint64>(PlayerId) << 32)
			| (static_cast<uint64>(MapRegion) << 16)
			| TeamColorIndex;
	}
}

UHorizontalBox* FMapAreaRegionBoxes::Resolve(
	const EPlayerMapRegion MapRegion) const
{
	switch (MapRegion)
	{
	case EPlayerMapRegion::Dome:
		return Dome;
	case EPlayerMapRegion::Temple:
		return Temple;
	case EPlayerMapRegion::Windmill:
		return Windmill;
	default:
		return Dome;
	}
}

void FMapAreaRegionBoxes::SetVisible(const bool bVisible) const
{
	const ESlateVisibility NewVisibility = bVisible
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	for (UHorizontalBox* MarkerBox : {Dome, Windmill, Temple})
	{
		if (MarkerBox)
		{
			MarkerBox->SetVisibility(NewVisibility);
		}
	}
}

void FMapAreaRegionMarkers::Refresh(
	UObject& Outer,
	const FMapAreaRegionBoxes& Boxes,
	const AGameStateBase* GameState,
	const FMapMarkImages& MarkImages)
{
	if (!GameState)
	{
		Reset();
		return;
	}

	TArray<APdPlayerState*> PlayerStates;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (APdPlayerState* PdPlayerState =
			Cast<APdPlayerState>(PlayerState))
		{
			PlayerStates.Add(PdPlayerState);
		}
	}
	PlayerStates.Sort(
		[](const APdPlayerState& Left, const APdPlayerState& Right)
		{
			if (Left.GetPlayerId() != Right.GetPlayerId())
			{
				return Left.GetPlayerId() < Right.GetPlayerId();
			}
			return Left.GetName() < Right.GetName();
		});

	TArray<uint64> NewMarkerStateKeys;
	NewMarkerStateKeys.Reserve(PlayerStates.Num());
	for (const APdPlayerState* PlayerState : PlayerStates)
	{
		if (PlayerState)
		{
			NewMarkerStateKeys.Add(
				MakeMarkerStateKey(*PlayerState));
		}
	}

	if (bMarkerWidgetsComplete
		&& MarkerStateKeys == NewMarkerStateKeys)
	{
		return;
	}

	for (UHorizontalBox* MarkerBox : {Boxes.Dome, Boxes.Windmill, Boxes.Temple})
	{
		if (MarkerBox)
		{
			MarkerBox->ClearChildren();
		}
	}

	int32 RequiredMarkerCount = 0;
	int32 AddedMarkerCount = 0;
	for (const APdPlayerState* PlayerState : PlayerStates)
	{
		const UPlayerMatchComponent* MatchComponent =
			PlayerState
				? PlayerState->GetPlayerMatchComponent()
				: nullptr;
		if (!MatchComponent)
		{
			continue;
		}

		UHorizontalBox* MarkerBox = Boxes.Resolve(
			MatchComponent->GetPlayerMapRegion());
		if (!MarkerBox)
		{
			continue;
		}
		++RequiredMarkerCount;

		UImage* MarkerImage = NewObject<UImage>(&Outer);
		if (!MarkerImage
			|| !MarkImages.ApplyTeamMark(
				MarkerImage,
				MatchComponent->GetMatchTeamColorIndex()))
		{
			continue;
		}

		MarkerImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		MarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UHorizontalBoxSlot* MarkerSlot =
			MarkerBox->AddChildToHorizontalBox(MarkerImage))
		{
			MarkerSlot->SetHorizontalAlignment(HAlign_Center);
			MarkerSlot->SetVerticalAlignment(VAlign_Center);
			++AddedMarkerCount;
		}
	}

	MarkerStateKeys = MoveTemp(NewMarkerStateKeys);
	bMarkerWidgetsComplete =
		AddedMarkerCount == RequiredMarkerCount;
}

void FMapAreaRegionMarkers::Reset()
{
	MarkerStateKeys.Reset();
	bMarkerWidgetsComplete = false;
}
