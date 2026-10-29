#pragma once

#include "CoreMinimal.h"

namespace TeamColorUtils
{
	inline constexpr int32 TeamColorCount = 6;

	inline int32 NormalizeTeamColorIndex(const int32 TeamColorIndex)
	{
		return TeamColorIndex == INDEX_NONE
			? 0
			: FMath::Clamp(TeamColorIndex, 0, TeamColorCount - 1);
	}

	inline FLinearColor GetTeamColor(const int32 TeamColorIndex)
	{
		switch (NormalizeTeamColorIndex(TeamColorIndex))
		{
		case 0:
			return FLinearColor(0.95f, 0.08f, 0.06f, 1.0f);
		case 1:
			return FLinearColor(0.08f, 0.28f, 1.0f, 1.0f);
		case 2:
			return FLinearColor(1.0f, 0.78f, 0.08f, 1.0f);
		case 3:
			return FLinearColor(0.58f, 0.18f, 0.95f, 1.0f);
		case 4:
			return FLinearColor(0.08f, 0.72f, 0.24f, 1.0f);
		case 5:
			return FLinearColor(1.0f, 0.42f, 0.04f, 1.0f);
		default:
			return FLinearColor::White;
		}
	}

	/** 문자열 테이블 MenuText에서 팀 이름을 찾는 키. 팀이 없으면 None이다. */
	inline FName GetTeamTextKey(const int32 TeamColorIndex)
	{
		static const FName Keys[TeamColorCount] = {
			TEXT("Team.Red"), TEXT("Team.Blue"), TEXT("Team.Yellow"), TEXT("Team.Purple"), TEXT("Team.Green"), TEXT("Team.Orange")};
		return TeamColorIndex >= 0 && TeamColorIndex < TeamColorCount ? Keys[TeamColorIndex] : NAME_None;
	}

	inline FLinearColor GetTeamColorTint(const int32 TeamColorIndex)
	{
		FLinearColor HsvColor = GetTeamColor(TeamColorIndex).LinearRGBToHSV();
		HsvColor.G = 0.9f;

		FLinearColor Tint = HsvColor.HSVToLinearRGB();
		Tint.A = 0.9f;
		return Tint;
	}
}
