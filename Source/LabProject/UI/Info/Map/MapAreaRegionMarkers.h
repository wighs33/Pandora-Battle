#pragma once

#include "CoreMinimal.h"

class AGameStateBase;
class UHorizontalBox;
struct FMapMarkImages;
enum class EPlayerMapRegion : uint8;

// 전체 지도에서 지역별 표식 줄을 담는 상자 세 개
struct FMapAreaRegionBoxes
{
	UHorizontalBox* Dome = nullptr;
	UHorizontalBox* Windmill = nullptr;
	UHorizontalBox* Temple = nullptr;

	UHorizontalBox* Resolve(EPlayerMapRegion MapRegion) const;
	void SetVisible(bool bVisible) const;
};

// 전체 지도의 지역마다 그 지역에 있는 플레이어의 팀 색 표식을 한 줄로 늘어놓는다.
// 플레이어·지역·팀 구성이 지난번과 같으면 표식을 다시 만들지 않는다.
struct FMapAreaRegionMarkers
{
public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Refresh(
		UObject& Outer,
		const FMapAreaRegionBoxes& Boxes,
		const AGameStateBase* GameState,
		const FMapMarkImages& MarkImages);
	void Reset();

private:
	TArray<uint64> MarkerStateKeys;
	bool bMarkerWidgetsComplete = false;
};
