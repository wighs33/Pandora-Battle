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
		const UPlayerMatchComponent* MatchComponent = PlayerState.GetPlayerMatchComponent();
		const uint32 PlayerId = static_cast<uint32>(PlayerState.GetPlayerId());
		const uint16 TeamColorIndex = static_cast<uint16>(
			MatchComponent
				? MatchComponent->GetMatchTeamColorIndex() + 1
				: 0);
		const uint8 MapRegion = static_cast<uint8>(
			MatchComponent
				? MatchComponent->GetPlayerMapRegion()
				: EPlayerMapRegion::Dome);
		return (static_cast<uint64>(PlayerId) << 32) | (static_cast<uint64>(MapRegion) << 16) | TeamColorIndex;
	}

	// 표식 순서가 매번 같도록 플레이어 ID, 같으면 이름 순으로 정렬한다.
	TArray<APdPlayerState*> CollectSortedPlayerStates(const AGameStateBase& GameState)
	{
		TArray<APdPlayerState*> PlayerStates;
		for (APlayerState* PlayerState : GameState.PlayerArray)
		{
			if (APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
			{
				PlayerStates.Add(PdPlayerState);
			}
		}
		PlayerStates.Sort([](const APdPlayerState& Left, const APdPlayerState& Right)
		{
			if (Left.GetPlayerId() != Right.GetPlayerId())
			{
				return Left.GetPlayerId() < Right.GetPlayerId();
			}
			return Left.GetName() < Right.GetName();
		});
		return PlayerStates;
	}

	// 플레이어가 있는 지역의 줄에 팀 색 표식을 하나 넣는다. 그 플레이어에게 표식이 필요하면 bOutRequired가 true다.
	bool AddRegionMarker(UObject& Outer, const FMapAreaRegionBoxes& Boxes, const FMapMarkImages& MarkImages,
		const APdPlayerState& PlayerState, bool& bOutRequired)
	{
		bOutRequired = false;
		const UPlayerMatchComponent* MatchComponent = PlayerState.GetPlayerMatchComponent();
		UHorizontalBox* MarkerBox = MatchComponent ? Boxes.Resolve(MatchComponent->GetPlayerMapRegion()) : nullptr;
		if (!MarkerBox)
		{
			return false;
		}
		bOutRequired = true;

		UImage* MarkerImage = NewObject<UImage>(&Outer);
		if (!MarkerImage || !MarkImages.ApplyTeamMark(MarkerImage, MatchComponent->GetMatchTeamColorIndex()))
		{
			return false;
		}

		MarkerImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		MarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		UHorizontalBoxSlot* MarkerSlot = MarkerBox->AddChildToHorizontalBox(MarkerImage);
		if (!MarkerSlot)
		{
			return false;
		}
		MarkerSlot->SetHorizontalAlignment(HAlign_Center);
		MarkerSlot->SetVerticalAlignment(VAlign_Center);
		return true;
	}
}

UHorizontalBox* FMapAreaRegionBoxes::Resolve(const EPlayerMapRegion MapRegion) const
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
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	for (UHorizontalBox* MarkerBox : {Dome, Windmill, Temple})
	{
		if (MarkerBox)
		{
			MarkerBox->SetVisibility(NewVisibility);
		}
	}
}

void FMapAreaRegionMarkers::Refresh(UObject& Outer, const FMapAreaRegionBoxes& Boxes, const AGameStateBase* GameState,
	const FMapMarkImages& MarkImages)
{
	if (!GameState)
	{
		Reset();
		return;
	}

	const TArray<APdPlayerState*> PlayerStates = CollectSortedPlayerStates(*GameState);
	TArray<uint64> NewMarkerStateKeys;
	NewMarkerStateKeys.Reserve(PlayerStates.Num());
	for (const APdPlayerState* PlayerState : PlayerStates)
	{
		NewMarkerStateKeys.Add(MakeMarkerStateKey(*PlayerState));
	}
	if (bMarkerWidgetsComplete && MarkerStateKeys == NewMarkerStateKeys)
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
		bool bRequired = false;
		AddedMarkerCount += AddRegionMarker(Outer, Boxes, MarkImages, *PlayerState, bRequired) ? 1 : 0;
		RequiredMarkerCount += bRequired ? 1 : 0;
	}

	MarkerStateKeys = MoveTemp(NewMarkerStateKeys);
	bMarkerWidgetsComplete = AddedMarkerCount == RequiredMarkerCount;
}

void FMapAreaRegionMarkers::Reset()
{
	MarkerStateKeys.Reset();
	bMarkerWidgetsComplete = false;
}
