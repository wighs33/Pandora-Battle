#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// 치명타 판정값이 확률보다 작을 때만 치명타가 나고, 배율은 2배에 치명타 수치 1%씩을 더한 값인지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCriticalDamageTest, "LabProject.Combat.Damage.CriticalHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdCriticalDamageTest::RunTest(const FString& Parameters)
{
	bool bCriticalHit = true;
	TestEqual(TEXT("no critical chance"), PdDamageRules::CalculateCriticalDamage(100.f, 0.f, 0.f, bCriticalHit), 100.f);
	TestFalse(TEXT("no critical chance flag"), bCriticalHit);

	TestEqual(TEXT("roll below chance"), PdDamageRules::CalculateCriticalDamage(100.f, 30.f, 29.9f, bCriticalHit), 230.f);
	TestTrue(TEXT("roll below chance flag"), bCriticalHit);

	TestEqual(TEXT("roll at chance"), PdDamageRules::CalculateCriticalDamage(100.f, 30.f, 30.f, bCriticalHit), 100.f);
	TestFalse(TEXT("roll at chance flag"), bCriticalHit);

	// 확률은 100%까지만 반영하지만 배율은 치명타 수치 전체를 쓴다.
	TestEqual(TEXT("chance above 100"), PdDamageRules::CalculateCriticalDamage(10.f, 150.f, 99.9f, bCriticalHit), 35.f);
	TestTrue(TEXT("chance above 100 flag"), bCriticalHit);

	// 피해가 없거나 잘못된 값이면 치명타도 없다.
	TestEqual(TEXT("zero damage"), PdDamageRules::CalculateCriticalDamage(0.f, 100.f, 0.f, bCriticalHit), 0.f);
	TestFalse(TEXT("zero damage flag"), bCriticalHit);
	TestEqual(TEXT("nan damage"),
		PdDamageRules::CalculateCriticalDamage(std::numeric_limits<float>::quiet_NaN(), 100.f, 0.f, bCriticalHit), 0.f);
	TestFalse(TEXT("nan damage flag"), bCriticalHit);
	return true;
}

// 들어온 피해가 상태 저항, 방어력, 보호막 순으로 줄어드는 규칙을 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdDamageMitigationTest, "LabProject.Combat.Damage.Mitigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdDamageMitigationTest::RunTest(const FString& Parameters)
{
	// 상태 저항은 0~100%로 잘라 남는 비율만큼만 피해를 받는다.
	TestEqual(TEXT("status resistance 25%"), PdDamageRules::MitigateByStatusResistance(12.f, 25.f), 9.f);
	TestEqual(TEXT("status resistance above 100%"), PdDamageRules::MitigateByStatusResistance(9.f, 130.f), 0.f);
	TestEqual(TEXT("negative status damage"), PdDamageRules::MitigateByStatusResistance(-5.f, 10.f), 0.f);

	// 방어력은 맞는 쪽 근력 반영 무기 피해의 비율만큼 깎고, 피해는 0 아래로 내려가지 않는다.
	TestEqual(TEXT("armor 20% of strength 5"), PdDamageRules::MitigateByArmor(10.f, 20.f, 5.f), 9.f);
	TestEqual(TEXT("armor larger than damage"), PdDamageRules::MitigateByArmor(1.f, 100.f, 50.f), 0.f);
	TestEqual(TEXT("negative armor"), PdDamageRules::MitigateByArmor(10.f, -20.f, 5.f), 10.f);

	// 보호막이 먼저 받고 남은 피해만 체력으로 넘어간다.
	const PdDamageRules::FShieldAbsorption Broken = PdDamageRules::AbsorbByShield(45.f, 30.f);
	TestEqual(TEXT("broken shield damage"), Broken.ShieldDamage, 30.f);
	TestEqual(TEXT("broken shield health damage"), Broken.HealthDamage, 15.f);
	TestEqual(TEXT("broken shield remaining"), Broken.RemainingShield, 0.f);
	const PdDamageRules::FShieldAbsorption Held = PdDamageRules::AbsorbByShield(4.f, 40.f);
	TestEqual(TEXT("held shield damage"), Held.ShieldDamage, 4.f);
	TestEqual(TEXT("held shield health damage"), Held.HealthDamage, 0.f);
	TestEqual(TEXT("held shield remaining"), Held.RemainingShield, 36.f);
	const PdDamageRules::FShieldAbsorption NoShield = PdDamageRules::AbsorbByShield(10.f, 0.f);
	TestEqual(TEXT("no shield damage"), NoShield.ShieldDamage, 0.f);
	TestEqual(TEXT("no shield health damage"), NoShield.HealthDamage, 10.f);

	// 상태 이상 피해는 상태별 증가율(%)과 추가 배율을 곱하고, 음수 배율은 0으로 본다.
	TestEqual(TEXT("status bonus 50% x2"), PdDamageRules::CalculateStatusEffectDamage(10.f, 50.f, 2.f), 30.f);
	TestEqual(TEXT("negative status scale"), PdDamageRules::CalculateStatusEffectDamage(10.f, 50.f, -1.f), 0.f);
	return true;
}

#endif
