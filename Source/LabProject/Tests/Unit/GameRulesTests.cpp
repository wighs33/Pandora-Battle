#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "AbilitySystem/Effects/EquipmentStatsEffect.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Match/MatchOutcomeRules.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

// 게임 규칙 단위 테스트: 피해 계산, 경기 승패와 승리 골드, 최대 자원이 바뀔 때의 현재 자원 비율.
// 훈련장 연결 테스트로는 만들기 어려운 경계값(치명타 확률 경계, NaN, 동점, 음수 점수, 가득 찬 자원)을 본다.

namespace
{
	// 시험용 게임 월드에 BasicAttributeSet을 가진 ASC 액터 하나를 둔다. 기본 최대 체력 100, 체력 100.
	struct FResourceTestAbilitySystem
	{
		UWorld* World = nullptr;
		UPdAbilitySystemComponent* AbilitySystem = nullptr;

		FResourceTestAbilitySystem()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			AActor* Owner = World->SpawnActor<AActor>();
			AbilitySystem = NewObject<UPdAbilitySystemComponent>(Owner);
			AbilitySystem->RegisterComponent();
			AbilitySystem->AddSpawnedAttribute(NewObject<UBasicAttributeSet>(Owner));
			AbilitySystem->InitAbilityActorInfo(Owner, Owner);
			AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetMaxHealthAttribute(), 100.f);
			AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), 100.f);
		}

		~FResourceTestAbilitySystem()
		{
			World->DestroyWorld(false);
		}

		float GetHealth() const { return AbilitySystem->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()); }

		FActiveGameplayEffectHandle ApplyEquipmentStats(const TMap<FGameplayTag, float>& StatMagnitudes) const
		{
			const FGameplayEffectSpecHandle Spec = UEquipmentStatsEffect::MakeSpec(*AbilitySystem, StatMagnitudes, nullptr);
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	};
}

// 치명타 판정값이 확률보다 작을 때만 치명타가 나고, 배율은 2배에 치명타 수치 1%씩을 더한 값인지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdCriticalDamageTest, "LabProject.Unit.Rules.CriticalDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdDamageMitigationTest, "LabProject.Unit.Rules.DamageMitigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
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

// 최고 점수자가 한 명이면 그 사람이, 같은 팀끼리 나눠 가지면 그 팀이 이기고, 다른 팀끼리 나눠 가지면 골든킬로 넘어가는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdMatchOutcomeTest, "LabProject.Unit.Rules.MatchOutcome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdMatchOutcomeTest::RunTest(const FString& Parameters)
{
	const FMatchOutcome Empty = MatchOutcomeRules::Resolve({});
	TestTrue(TEXT("no players"), Empty.Type == EMatchOutcomeType::NoPlayers && !Empty.HasWinner());

	const FMatchStanding Unique[] = {{3, 0}, {5, 1}, {4, 0}};
	const FMatchOutcome UniqueOutcome = MatchOutcomeRules::Resolve(Unique);
	TestTrue(TEXT("unique leader type"), UniqueOutcome.Type == EMatchOutcomeType::UniqueLeader);
	TestEqual(TEXT("unique leader"), UniqueOutcome.WinnerIndex, 1);
	TestEqual(TEXT("unique top score"), UniqueOutcome.TopScore, 5);

	// 같은 팀 두 명이 공동 1위면 첫 번째 공동 1위를 승자로 보고 팀 승리로 끝낸다.
	const FMatchStanding SameTeam[] = {{2, 1}, {6, 0}, {6, 0}};
	const FMatchOutcome SameTeamOutcome = MatchOutcomeRules::Resolve(SameTeam);
	TestTrue(TEXT("same team tie type"), SameTeamOutcome.Type == EMatchOutcomeType::LeadingTeam);
	TestEqual(TEXT("same team tie winner"), SameTeamOutcome.WinnerIndex, 1);

	const FMatchStanding Opposing[] = {{6, 0}, {6, 1}};
	const FMatchOutcome OpposingOutcome = MatchOutcomeRules::Resolve(Opposing);
	TestTrue(TEXT("opposing tie type"), OpposingOutcome.Type == EMatchOutcomeType::OpposingTie);
	TestFalse(TEXT("opposing tie has no winner"), OpposingOutcome.HasWinner());
	TestEqual(TEXT("opposing tie top scorer"), OpposingOutcome.TopScorerIndex, 0);

	// 팀이 없는 참가자끼리는 같은 팀 번호가 아니어도 각자 다른 편이다.
	const FMatchStanding NoTeam[] = {{1, INDEX_NONE}, {1, INDEX_NONE}};
	TestTrue(TEXT("no team tie"), MatchOutcomeRules::Resolve(NoTeam).Type == EMatchOutcomeType::OpposingTie);

	// 음수 점수는 0점으로 본다.
	const FMatchStanding Negative[] = {{-3, 0}, {0, 1}};
	TestTrue(TEXT("negative scores tie at zero"), MatchOutcomeRules::Resolve(Negative).Type == EMatchOutcomeType::OpposingTie);
	return true;
}

// 승리 골드는 처치당 보상에서 사망 감점을 빼고 팀 인원 보상을 더하며, 0 아래로 내려가지 않는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdVictoryGoldTest, "LabProject.Unit.Rules.VictoryGold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdVictoryGoldTest::RunTest(const FString& Parameters)
{
	const FVictoryGoldRates Rates{100, 50, 3};
	TestEqual(TEXT("kills, deaths and team"), MatchOutcomeRules::CalculateVictoryGold(4, 2, 3, Rates), 400 - 100 + 9);
	TestEqual(TEXT("solo winner counts as one member"), MatchOutcomeRules::CalculateVictoryGold(1, 0, 0, Rates), 103);
	TestEqual(TEXT("never negative"), MatchOutcomeRules::CalculateVictoryGold(0, 10, 1, Rates), 0);

	const FMatchStanding Team[] = {{0, 2}, {0, 2}, {0, 1}};
	TestEqual(TEXT("team members"), MatchOutcomeRules::CountTeamMembers(Team, 2), 2);
	TestEqual(TEXT("no team"), MatchOutcomeRules::CountTeamMembers(Team, INDEX_NONE), 1);
	TestEqual(TEXT("missing team still counts the winner"), MatchOutcomeRules::CountTeamMembers(Team, 5), 1);
	return true;
}

// 장비나 능력치 강화로 최대 자원이 바뀌는 동안 현재 자원의 비율을 유지하고, 가득 차 있던 자원은 새 최대값까지 채우는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdResourceRatioTest, "LabProject.Unit.Rules.ResourceRatio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdResourceRatioTest::RunTest(const FString& Parameters)
{
	FResourceTestAbilitySystem Test;
	FActiveGameplayEffectHandle Handle;
	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Handle = Test.ApplyEquipmentStats({{LabGameplayTags::Status_Resource_MaxHealth, 100.f}});
	}
	TestEqual(TEXT("full health fills the new maximum"), Test.GetHealth(), 200.f);

	Test.AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), 100.f);
	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Test.AbilitySystem->RemoveActiveGameplayEffect(Handle);
	}
	TestEqual(TEXT("half health stays half"), Test.GetHealth(), 50.f);

	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Test.ApplyEquipmentStats({{LabGameplayTags::Status_Defense_Armor, 10.f}});
	}
	TestEqual(TEXT("unrelated stat leaves health alone"), Test.GetHealth(), 50.f);
	return true;
}

#endif
