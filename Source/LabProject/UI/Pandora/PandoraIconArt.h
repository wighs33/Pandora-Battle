#pragma once

#include "CoreMinimal.h"

/**
 * 판도라 아이콘 그림의 틀. 아이콘은 모두 같은 틀로 그려져 있다:
 * 너비를 꽉 채운 금테 원판 아래에 뾰족한 장식이 달려 세로가 조금 길다.
 * 원 모양 칸에는 금테 안쪽 그림 원만 꽉 차게 넣는다(틀째 넣으면 금테가 두 겹이 되고 투명한 모서리가 빈 곳으로 보인다).
 */
namespace PandoraIconArt
{
	/** 금테 안쪽 그림 원. 모두 그림 너비에 대한 비율이고, 가로 중심은 가운데다(아이콘 33개를 재어 정한 값). */
	struct FArtCircle
	{
		float CenterY = 0.475f;
		float Radius = 0.335f;
	};

	/** 그림 원을 감싸는 정사각형의 UV 영역. 그림 크기를 모르면 잘못된 영역(bIsValid 거짓)을 돌려 그림 전체를 쓰게 한다. */
	inline FBox2f MakeArtUVRegion(const FVector2f TextureSize, const FArtCircle& Circle = FArtCircle())
	{
		if (TextureSize.X <= 0.f || TextureSize.Y <= 0.f)
		{
			return FBox2f(ForceInit);
		}
		const float WidthPerHeight = TextureSize.X / TextureSize.Y;
		const FVector2f Center(0.5f, Circle.CenterY * WidthPerHeight);
		const FVector2f HalfSize(Circle.Radius, Circle.Radius * WidthPerHeight);
		return FBox2f(Center - HalfSize, Center + HalfSize);
	}
}
