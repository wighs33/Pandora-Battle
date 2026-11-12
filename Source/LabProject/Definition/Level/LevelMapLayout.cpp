#include "Definition/Level/LevelMapLayout.h"

#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelMapLayout)

namespace
{
	/** 월드 평면의 벡터를 그림 축(오른쪽 = 방향각, 아래 = 방향각 + 90도) 성분으로 나눈다. */
	FVector2D ToPageAxes(const FVector2D& WorldOffset, const float WorldYaw)
	{
		float Sin = 0.0f;
		float Cos = 1.0f;
		FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(WorldYaw));
		return FVector2D(WorldOffset.X * Cos + WorldOffset.Y * Sin, -WorldOffset.X * Sin + WorldOffset.Y * Cos);
	}
}

FVector2D FLevelMapPage::ToMapUV(const FVector& WorldLocation) const
{
	const FVector2D Local = ToPageAxes(FVector2D(WorldLocation) - WorldCenter, WorldYaw);
	const FVector2D SafeSize(FMath::Max(WorldSize.X, 1.0f), FMath::Max(WorldSize.Y, 1.0f));
	return FVector2D(FMath::Clamp(Local.X / SafeSize.X + 0.5f, 0.0f, 1.0f), FMath::Clamp(Local.Y / SafeSize.Y + 0.5f, 0.0f, 1.0f));
}

float FLevelMapPage::ToMapAngle(const FVector& WorldForward) const
{
	FVector2D Local = ToPageAxes(FVector2D(WorldForward), WorldYaw);
	if (!Local.Normalize())
	{
		Local = FVector2D(0.0f, 1.0f);
	}
	return -FMath::RadiansToDegrees(FMath::Atan2(Local.X, Local.Y));
}

FVector2D FLevelMapPage::FitViewSize(const FVector2D& MaxSize) const
{
	const float Aspect = FMath::Max(WorldSize.X, 1.0f) / FMath::Max(WorldSize.Y, 1.0f);
	return MaxSize.X / MaxSize.Y > Aspect ? FVector2D(MaxSize.Y * Aspect, MaxSize.Y) : FVector2D(MaxSize.X, MaxSize.X / Aspect);
}

FVector2D FLevelMapAreaView::ToMapUV(const FVector& WorldLocation) const
{
	const FRotator Rotation(Pitch, Yaw, 0.0f);
	const FRotationMatrix Axes(Rotation);
	const FVector Offset = WorldLocation - Center;
	const float SafeWidth = FMath::Max(WorldWidth, 1.0f);
	// 정사영이라 깊이는 버리고, 카메라 오른쪽·아래 성분만 그림 좌표가 된다.
	return FVector2D(Offset.Dot(Axes.GetUnitAxis(EAxis::Y)) / SafeWidth + 0.5f, -Offset.Dot(Axes.GetUnitAxis(EAxis::Z)) / SafeWidth + 0.5f);
}

int32 FLevelMapLayout::FindPageIndex(const EPlayerMapRegion Region) const
{
	return Pages.IndexOfByPredicate([Region](const FLevelMapPage& Page) { return Page.Region == Region; });
}

void FLevelMapLayout::CollectAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	const auto AddPath = [&OutPaths](const TSoftObjectPtr<UTexture2D>& Texture)
	{
		if (!Texture.IsNull())
		{
			OutPaths.AddUnique(Texture.ToSoftObjectPath());
		}
	};
	for (const FLevelMapPage& Page : Pages)
	{
		AddPath(Page.Texture);
		AddPath(Page.AreaHighlight);
	}
	if (HasAreaView())
	{
		AddPath(AreaView.Texture);
	}
}
