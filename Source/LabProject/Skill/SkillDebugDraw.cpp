#include "Skill/SkillDebugDraw.h"

#include "DrawDebugHelpers.h"

void LabSkillDebug::DrawAreaRadius(
	const UWorld* World,
	const FVector& Center,
	const float Radius,
	const FColor& CircleColor,
	const FColor& SphereColor,
	const float DrawTime)
{
	if (!World)
	{
		return;
	}

	const uint8 DepthPriority = 1;
	constexpr int32 CircleSegments = 128;
	constexpr int32 HemisphereSegments = 24;
	constexpr int32 HemisphereMeridians = 8;
	constexpr int32 LatitudeRings = 4;
	constexpr float LineThickness = 2.0f;

	DrawDebugCircle(
		World,
		Center,
		Radius,
		CircleSegments,
		CircleColor,
		false,
		DrawTime,
		DepthPriority,
		4.0f,
		FVector::ForwardVector,
		FVector::RightVector,
		false);

	// 위도 고리: 반지름을 높이에 따라 줄여 반구 윤곽을 만든다.
	for (int32 RingIndex = 1; RingIndex <= LatitudeRings; ++RingIndex)
	{
		const float Alpha = static_cast<float>(RingIndex) / static_cast<float>(LatitudeRings + 1);
		const float Angle = Alpha * HALF_PI;
		const float RingRadius = FMath::Cos(Angle) * Radius;
		const float RingHeight = FMath::Sin(Angle) * Radius;

		DrawDebugCircle(
			World,
			Center + FVector(0.0, 0.0, RingHeight),
			RingRadius,
			CircleSegments,
			SphereColor,
			false,
			DrawTime,
			DepthPriority,
			LineThickness,
			FVector::ForwardVector,
			FVector::RightVector,
			false);
	}

	// 경도선: 바닥 원에서 꼭대기까지 사분원을 이어 그린다.
	for (int32 MeridianIndex = 0; MeridianIndex < HemisphereMeridians; ++MeridianIndex)
	{
		const float Azimuth = (static_cast<float>(MeridianIndex) / static_cast<float>(HemisphereMeridians)) * TWO_PI;
		const FVector HorizontalDirection(
			FMath::Cos(Azimuth),
			FMath::Sin(Azimuth),
			0.0f);

		FVector PreviousPoint = Center + HorizontalDirection * Radius;
		for (int32 SegmentIndex = 1; SegmentIndex <= HemisphereSegments; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(HemisphereSegments);
			const float Angle = Alpha * HALF_PI;
			const FVector CurrentPoint = Center
				+ HorizontalDirection * (FMath::Cos(Angle) * Radius)
				+ FVector(0.0, 0.0, FMath::Sin(Angle) * Radius);

			DrawDebugLine(
				World,
				PreviousPoint,
				CurrentPoint,
				SphereColor,
				false,
				DrawTime,
				DepthPriority,
				LineThickness);

			PreviousPoint = CurrentPoint;
		}
	}
}
