#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/Effects/EquipmentStatsEffect.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// 시험용 게임 월드에 BasicAttributeSet을 가진 ASC 액터 하나를 둔다. 기본 최대 체력 100, 체력 100.
	struct FEquipmentTestAbilitySystem
	{
		UWorld* World = nullptr;
		UPdAbilitySystemComponent* AbilitySystem = nullptr;

		FEquipmentTestAbilitySystem()
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

		~FEquipmentTestAbilitySystem()
		{
			World->DestroyWorld(false);
		}

		float Get(const FGameplayAttribute& Attribute) const { return AbilitySystem->GetNumericAttribute(Attribute); }

		FActiveGameplayEffectHandle Apply(const TMap<FGameplayTag, float>& StatMagnitudes, const FGameplayTag GrantedTag = FGameplayTag()) const
		{
			const FGameplayEffectSpecHandle Spec = UEquipmentStatsEffect::MakeSpec(*AbilitySystem, StatMagnitudes, nullptr);
			if (GrantedTag.IsValid())
			{
				Spec.Data->DynamicGrantedTags.AddTag(GrantedTag);
			}
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	};
}

// 장비 효과는 기본값을 건드리지 않고 수정자로 더해지며, 핸들을 지우면 원래 값과 태그로 돌아오는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdEquipmentStatsEffectTest, "LabProject.Unit.Equipment.StatsEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdEquipmentStatsEffectTest::RunTest(const FString& Parameters)
{
	FEquipmentTestAbilitySystem Test;
	const FGameplayTag ItemTag = LabGameplayTags::Action_Attack;

	const FActiveGameplayEffectHandle Handle = Test.Apply({
		{LabGameplayTags::Status_Resource_MaxHealth, 25.f},
		{LabGameplayTags::Status_Defense_Armor, 5.f},
	}, ItemTag);
	TestTrue(TEXT("applied as an active effect"), Handle.IsValid());
	TestEqual(TEXT("max health includes equipment"), Test.Get(UBasicAttributeSet::GetMaxHealthAttribute()), 125.f);
	TestEqual(TEXT("max health base is untouched"),
		Test.AbilitySystem->GetNumericAttributeBase(UBasicAttributeSet::GetMaxHealthAttribute()), 100.f);
	TestEqual(TEXT("armor from equipment"), Test.Get(UBasicAttributeSet::GetArmorAttribute()), 5.f);
	TestEqual(TEXT("stats the item lacks stay zero"), Test.Get(UBasicAttributeSet::GetIntelligenceAttribute()), 0.f);
	TestTrue(TEXT("item tag granted"), Test.AbilitySystem->HasMatchingGameplayTag(ItemTag));

	Test.AbilitySystem->RemoveActiveGameplayEffect(Handle);
	TestEqual(TEXT("max health restored"), Test.Get(UBasicAttributeSet::GetMaxHealthAttribute()), 100.f);
	TestEqual(TEXT("armor restored"), Test.Get(UBasicAttributeSet::GetArmorAttribute()), 0.f);
	TestFalse(TEXT("item tag removed"), Test.AbilitySystem->HasMatchingGameplayTag(ItemTag));
	return true;
}

// 최대 자원이 바뀌는 동안 현재 자원의 비율을 유지하고, 가득 차 있던 자원은 새 최대값까지 채우는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdScopedResourceRatioTest, "LabProject.Unit.Equipment.ResourceRatio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdScopedResourceRatioTest::RunTest(const FString& Parameters)
{
	FEquipmentTestAbilitySystem Test;
	FActiveGameplayEffectHandle Handle;
	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Handle = Test.Apply({{LabGameplayTags::Status_Resource_MaxHealth, 100.f}});
	}
	TestEqual(TEXT("full health fills the new maximum"), Test.Get(UBasicAttributeSet::GetHealthAttribute()), 200.f);

	Test.AbilitySystem->SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), 100.f);
	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Test.AbilitySystem->RemoveActiveGameplayEffect(Handle);
	}
	TestEqual(TEXT("half health stays half"), Test.Get(UBasicAttributeSet::GetHealthAttribute()), 50.f);

	{
		FScopedResourceRatio KeepRatio(Test.AbilitySystem);
		Test.Apply({{LabGameplayTags::Status_Defense_Armor, 10.f}});
	}
	TestEqual(TEXT("unrelated stat leaves health alone"), Test.Get(UBasicAttributeSet::GetHealthAttribute()), 50.f);
	return true;
}

#endif
