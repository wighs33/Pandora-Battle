#pragma once

#include "CoreMinimal.h"
#include "MapPawnMarkers.generated.h"

class APawn;
class UImage;
class UPanelWidget;
struct FMapMarkImages;

// 지역 평면의 월드 위치와 방향을 지도 그림 좌표와 표식 회전각으로 옮긴다.
struct FMapMarkProjection
{
	FTransform PlaneTransform = FTransform::Identity;
	FVector2D MapSize = FVector2D::ZeroVector;
	float PlaneLocalSize = 100.0f;
	bool bRotateToForward = true;
	float RotationOffsetDegrees = 0.0f;

	FVector2D ToMapPosition(const FVector& WorldLocation) const;
	float ToMarkAngle(const FVector& WorldForward) const;
};

// 표식을 만들고 놓을 때 지도 위젯이 넘기는 현재 보기 정보
struct FMapMarkerCanvas
{
	UObject* Outer = nullptr;
	UPanelWidget* ParentPanel = nullptr;
	const FMapMarkImages* MarkImages = nullptr;
	FMapMarkProjection Projection;
};

// 지금 보이는 지역 지도 위에 내 표식과 다른 플레이어 표식을 그린다.
// 표식 위젯은 처음 필요할 때 만들고, 보이지 않을 때는 숨겨 두었다가 다음 갱신에 다시 쓴다.
USTRUCT()
struct FMapPawnMarkers
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	// 내 위치에는 팀 색 표식 위에 캐릭터 표식을 겹친다. 내가 다른 지역에 있으면 둘 다 숨긴다.
	void UpdateSelfMarks(const FMapMarkerCanvas& Canvas, const APawn& OwningPawn, bool bOnShownRegion);

	// 보이는 지역에 있는 다른 플레이어마다 팀 색 표식을 하나씩 쓴다.
	void UpdateRemoteMarks(const FMapMarkerCanvas& Canvas, TConstArrayView<const APawn*> PawnsOnShownRegion);

	void HideSelfMarks() const;
	void HideRemoteMarks() const;
	void RemoveAll();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void EnsureRemoteMarkCount(const FMapMarkerCanvas& Canvas, int32 RequiredCount);
	static UImage* CreateMark(const FMapMarkerCanvas& Canvas, FName MarkName, int32 ZOrder);
	static bool ApplyTeamMark(UImage* Mark, const APawn& Pawn, const FMapMarkImages& MarkImages);
	static bool PlaceMark(UImage* Mark, const APawn& Pawn, const FMapMarkProjection& Projection, bool bRotateToPawnForward);

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelfTeamMark;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelfCharacterMark;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RemoteTeamMarks;
};
