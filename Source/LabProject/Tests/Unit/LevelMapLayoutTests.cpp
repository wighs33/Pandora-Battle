#include "Misc/AutomationTest.h"
#include "Definition/Level/LevelMapLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

// 지역 지도 한 장: 그림 가운데·가장자리·회전·비율이 지도 그림을 찍은 정사영 카메라와 같은 규칙을 따른다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLevelMapPageProjectionTest, "LabProject.Unit.UI.LevelMapPageProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdLevelMapPageProjectionTest::RunTest(const FString& Parameters)
{
	FLevelMapPage Page;
	Page.WorldCenter = FVector2D(1000.0f, -500.0f);
	Page.WorldSize = FVector2D(4000.0f, 2000.0f);

	TestEqual(TEXT("the world center lands on the picture center"), Page.ToMapUV(FVector(1000.0f, -500.0f, 300.0f)), FVector2D(0.5f, 0.5f));
	TestEqual(TEXT("+X is the picture's right edge at half the width"), Page.ToMapUV(FVector(3000.0f, -500.0f, 0.0f)), FVector2D(1.0f, 0.5f));
	TestEqual(TEXT("+Y is the picture's bottom edge at half the height"), Page.ToMapUV(FVector(1000.0f, 500.0f, 0.0f)), FVector2D(0.5f, 1.0f));
	TestEqual(TEXT("a point outside sticks to the edge"), Page.ToMapUV(FVector(-9000.0f, -9000.0f, 0.0f)), FVector2D(0.0f, 0.0f));

	// 그림 오른쪽이 월드 +Y(방향각 90도)면 월드 +Y가 그림 오른쪽, 월드 -X가 그림 아래다.
	Page.WorldYaw = 90.0f;
	TestTrue(TEXT("a turned page maps world +Y to the right"), Page.ToMapUV(FVector(1000.0f, 1500.0f, 0.0f)).Equals(FVector2D(1.0f, 0.5f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("a turned page maps world -X to the bottom"), Page.ToMapUV(FVector(0.0f, -500.0f, 0.0f)).Equals(FVector2D(0.5f, 1.0f), KINDA_SMALL_NUMBER));
	TestEqual(TEXT("facing the picture's bottom is the mark's rest angle"), Page.ToMapAngle(FVector(-1.0f, 0.0f, 0.0f)), 0.0f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("facing the picture's right turns the mark 90 degrees counter-clockwise"), Page.ToMapAngle(FVector(0.0f, 1.0f, 0.0f)), -90.0f, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("a wide page fills the width of a wide box"), Page.FitViewSize(FVector2D(1200.0f, 900.0f)), FVector2D(1200.0f, 600.0f));
	Page.WorldSize = FVector2D(1000.0f, 1000.0f);
	TestEqual(TEXT("a square page fills the height"), Page.FitViewSize(FVector2D(1200.0f, 900.0f)), FVector2D(900.0f, 900.0f));
	return true;
}

// 전체 지도: 비스듬한 정사영이라 카메라 방향으로 멀어져도 그림 위치는 같고, 높이는 그림 위쪽으로 간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLevelMapAreaProjectionTest, "LabProject.Unit.UI.LevelMapAreaProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdLevelMapAreaProjectionTest::RunTest(const FString& Parameters)
{
	FLevelMapAreaView Area;
	Area.Center = FVector(0.0f, 0.0f, 0.0f);
	Area.Yaw = 0.0f;
	Area.Pitch = -40.0f;
	Area.WorldWidth = 10000.0f;

	TestEqual(TEXT("the look-at point is the picture center"), Area.ToMapUV(FVector::ZeroVector), FVector2D(0.5f, 0.5f));
	const FVector Forward = FRotator(Area.Pitch, Area.Yaw, 0.0f).Vector();
	TestTrue(TEXT("moving along the view direction does not move the point"), Area.ToMapUV(Forward * 3000.0f).Equals(FVector2D(0.5f, 0.5f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("camera right (+Y at yaw 0) is the picture's right"), Area.ToMapUV(FVector(0.0f, 2500.0f, 0.0f)).Equals(FVector2D(0.75f, 0.5f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("higher ground sits higher in the picture"), Area.ToMapUV(FVector(0.0f, 0.0f, 1000.0f)).Y < 0.5f);

	FLevelMapLayout Layout;
	TestFalse(TEXT("an empty layout has no map"), Layout.HasMap());
	Layout.Pages.AddDefaulted(2);
	Layout.Pages[1].Region = EPlayerMapRegion::Temple;
	TestFalse(TEXT("two pages without an area picture cannot choose a region"), Layout.HasAreaView());
	TestEqual(TEXT("pages are found by region"), Layout.FindPageIndex(EPlayerMapRegion::Temple), 1);
	return true;
}

#endif
