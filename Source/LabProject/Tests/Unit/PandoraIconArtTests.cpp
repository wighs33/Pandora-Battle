#include "Misc/AutomationTest.h"
#include "UI/Pandora/PandoraIconArt.h"

#if WITH_DEV_AUTOMATION_TESTS

// 판도라 아이콘의 금테 안쪽 그림 원을 원 모양 칸에 넣는 UV 영역: 찌그러지지 않고, 그림 밖으로 나가지 않고, 크기를 모르면 그림 전체를 쓴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdPandoraIconArtRegionTest, "LabProject.Unit.UI.PandoraIconArtRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdPandoraIconArtRegionTest::RunTest(const FString& Parameters)
{
	// 불 판도라 아이콘의 크기(세로가 너비보다 긴 틀)
	const FVector2f FireIconSize(803.f, 856.f);
	const FBox2f Region = PandoraIconArt::MakeArtUVRegion(FireIconSize);
	TestTrue(TEXT("a known size gives a region"), Region.bIsValid);

	const FVector2f PixelSize = Region.GetSize() * FireIconSize;
	TestEqual(TEXT("the region is square in pixels, so the art is not squashed"), PixelSize.X, PixelSize.Y, 0.01f);

	const PandoraIconArt::FArtCircle Circle;
	TestEqual(TEXT("the region spans the art circle's diameter"), PixelSize.X, 2.f * Circle.Radius * FireIconSize.X, 0.01f);
	TestEqual(TEXT("the region is centered across"), Region.GetCenter().X, 0.5f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("the region is centered on the art circle"), Region.GetCenter().Y * FireIconSize.Y, Circle.CenterY * FireIconSize.X, 0.01f);

	// 재어 본 아이콘 가운데 가장 납작한 것과 가장 긴 것에서도 그림 안에 머문다.
	for (const FVector2f Size : { FVector2f(1034.f, 1083.f), FVector2f(1322.f, 1429.f) })
	{
		const FBox2f Inside = PandoraIconArt::MakeArtUVRegion(Size);
		TestTrue(TEXT("the region stays inside the picture"),
			Inside.Min.X >= 0.f && Inside.Min.Y >= 0.f && Inside.Max.X <= 1.f && Inside.Max.Y <= 1.f);
	}

	TestFalse(TEXT("an unknown size falls back to the whole picture"), PandoraIconArt::MakeArtUVRegion(FVector2f::ZeroVector).bIsValid);
	return true;
}

#endif
