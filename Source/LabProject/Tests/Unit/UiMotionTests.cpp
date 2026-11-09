#include "Misc/AutomationTest.h"
#include "UI/Common/UiMotion.h"
#include "UI/HUD/Ability/SlotCooldownDisplay.h"

#if WITH_DEV_AUTOMATION_TESTS

// UI 움직임 단위 테스트: 체력 꼬리의 머묾과 따라옴, 스프링이 목표에 자리 잡는지, 쿨다운 그늘 비율의 경계값.
// 화면을 띄워서는 프레임마다 값을 확인하기 어려워 식만 따로 본다.

namespace
{
	constexpr float FrameSeconds = 1.f / 60.f;

	void StepFor(UiMotion::FTrailingValue& Trail, const float Seconds, const float CatchUpSpeed)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += FrameSeconds)
		{
			Trail.Step(FrameSeconds, CatchUpSpeed);
		}
	}
}

// 깎이면 머물렀다 따라오고, 머무는 동안 또 깎이면 다시 머물고, 회복은 바로 따라가는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdDamageTrailTest, "LabProject.Unit.UiMotion.DamageTrail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdDamageTrailTest::RunTest(const FString& Parameters)
{
	constexpr float HoldSeconds = 0.3f;
	constexpr float CatchUpSpeed = 6.f;
	UiMotion::FTrailingValue Trail;
	Trail.Snap(1.f);

	TestTrue(TEXT("a drop is reported"), Trail.SetTarget(0.6f, HoldSeconds));
	StepFor(Trail, HoldSeconds * 0.5f, CatchUpSpeed);
	TestEqual(TEXT("the trail holds right after a hit"), Trail.Value, 1.f);

	TestTrue(TEXT("a second hit is reported"), Trail.SetTarget(0.4f, HoldSeconds));
	StepFor(Trail, HoldSeconds * 0.8f, CatchUpSpeed);
	TestEqual(TEXT("a second hit restarts the hold"), Trail.Value, 1.f);

	StepFor(Trail, 2.f, CatchUpSpeed);
	TestTrue(TEXT("the trail settles on the new value"), Trail.IsSettled());
	TestEqual(TEXT("the trail reaches the new value"), Trail.Value, 0.4f);

	TestFalse(TEXT("healing is not a drop"), Trail.SetTarget(0.9f, HoldSeconds));
	TestEqual(TEXT("healing moves the trail at once"), Trail.Value, 0.9f);
	TestTrue(TEXT("healing leaves nothing to animate"), Trail.IsSettled());
	return true;
}

// 감쇠비가 1보다 작은 스프링은 목표를 넘었다가 돌아와 자리 잡고, 긴 프레임에도 튀지 않는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdSpringSettleTest, "LabProject.Unit.UiMotion.SpringSettle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdSpringSettleTest::RunTest(const FString& Parameters)
{
	const UiMotion::FSpringSettings Bouncy{420.f, 18.f};
	UiMotion::FSpring Spring;
	Spring.Snap(0.f);

	float Peak = 0.f;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Spring.Step(1.f, FrameSeconds, Bouncy);
		Peak = FMath::Max(Peak, Spring.Value);
	}
	TestTrue(TEXT("an underdamped spring overshoots"), Peak > 1.f);
	TestTrue(TEXT("the spring settles within two seconds"), Spring.IsSettled(1.f, 0.01f));

	Spring.Snap(0.f);
	Spring.Step(1.f, 5.f, Bouncy);
	TestTrue(TEXT("a long hitch does not explode the spring"), FMath::Abs(Spring.Value) < 2.f);
	return true;
}

// 쿨다운 그늘은 남은 시간 비율만큼 덮고, 전체 시간을 모르거나 범위를 벗어나면 0~1로 묶이는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCooldownShadeTest, "LabProject.Unit.UiMotion.CooldownShade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdCooldownShadeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("half the time left covers half"), SlotCooldownDisplay::CalculateRemainingFraction(2.f, 4.0), 0.5f);
	TestEqual(TEXT("unknown duration shows ready"), SlotCooldownDisplay::CalculateRemainingFraction(2.f, 0.0), 0.f);
	TestEqual(TEXT("more time than the duration is clamped"), SlotCooldownDisplay::CalculateRemainingFraction(9.f, 4.0), 1.f);
	TestEqual(TEXT("negative time is clamped"), SlotCooldownDisplay::CalculateRemainingFraction(-1.f, 4.0), 0.f);
	return true;
}

#endif
