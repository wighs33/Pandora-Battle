#pragma once

#include "CoreMinimal.h"

class UImage;
struct FMapWidgetSettings;

// 지도 표식에 쓸 그림을 팀 색마다 고른다.
// 팀 그림이 없는 팀은 캐릭터 표식 그림을, 그것도 없으면 디자이너가 둔 팀 표식 그림을 팀 색으로 물들여 쓴다.
struct FMapMarkImages
{
public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Configure(const FMapWidgetSettings& Settings);
	void SetDesignerTeamMark(const UImage* InDesignerTeamMark);

	bool ApplyCharacterMark(UImage* MarkWidget) const;
	bool ApplyTeamMark(UImage* MarkWidget, int32 TeamColorIndex) const;

	static FVector2D GetDefaultMarkSize();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static bool ApplyImage(UImage* MarkWidget, UObject* ResourceObject, const FVector2D& DesiredImageSize);

	TMap<int32, TSoftObjectPtr<UObject>> TeamMarkImagesByTeamColorIndex;
	TMap<int32, FVector2D> TeamMarkImageSizesByTeamColorIndex;
	TSoftObjectPtr<UObject> CharacterMarkImage;
	FVector2D CharacterMarkImageSize = FVector2D::ZeroVector;
	TWeakObjectPtr<const UImage> DesignerTeamMark;
};
