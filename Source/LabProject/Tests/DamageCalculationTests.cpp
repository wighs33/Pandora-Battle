#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// 치명타 판정값이 확률보다 작을 때만 치명타가 나고, 배율은 2배에 치명타 수치 1%씩을 더한 값인지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCriticalDamageTest, "LabProject.Combat.Damage.CriticalHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdCriticalDamageTest::RunTest(const FString& Parameters)
{
	bool bCriticalHit = true;
	TestEqual(TEXT("no critical chance"), UBasicAttributeSet::CalculateCriticalDamage(100.f, 0.f, 0.f, bCriticalHit), 100.f);
	TestFalse(TEXT("no critical chance flag"), bCriticalHit);

	TestEqual(TEXT("roll below chance"), UBasicAttributeSet::CalculateCriticalDamage(100.f, 30.f, 29.9f, bCriticalHit), 230.f);
	TestTrue(TEXT("roll below chance flag"), bCriticalHit);

	TestEqual(TEXT("roll at chance"), UBasicAttributeSet::CalculateCriticalDamage(100.f, 30.f, 30.f, bCriticalHit), 100.f);
	TestFalse(TEXT("roll at chance flag"), bCriticalHit);

	// 확률은 100%까지만 반영하지만 배율은 치명타 수치 전체를 쓴다.
	TestEqual(TEXT("chance above 100"), UBasicAttributeSet::CalculateCriticalDamage(10.f, 150.f, 99.9f, bCriticalHit), 35.f);
	TestTrue(TEXT("chance above 100 flag"), bCriticalHit);

	// 피해가 없거나 잘못된 값이면 치명타도 없다.
	TestEqual(TEXT("zero damage"), UBasicAttributeSet::CalculateCriticalDamage(0.f, 100.f, 0.f, bCriticalHit), 0.f);
	TestFalse(TEXT("zero damage flag"), bCriticalHit);
	TestEqual(TEXT("nan damage"),
		UBasicAttributeSet::CalculateCriticalDamage(std::numeric_limits<float>::quiet_NaN(), 100.f, 0.f, bCriticalHit), 0.f);
	TestFalse(TEXT("nan damage flag"), bCriticalHit);
	return true;
}

#endif
