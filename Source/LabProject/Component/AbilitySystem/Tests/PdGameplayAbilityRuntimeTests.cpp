#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/StaticAbility.h"

#include "Component/AbilitySystem/Ability/PdAbilityMovementRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilityPresentationRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilityResourceRuntime.h"
#include "Component/AbilitySystem/Ability/PdAbilitySourceRuntime.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdGameplayAbilityRuntimeCompositionTest,
	"LabProject.AbilitySystem.GameplayAbilityRuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdGameplayAbilityRuntimeCompositionTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);

	UStaticAbility* FirstAbility =
		NewObject<UStaticAbility>(GetTransientPackage());
	UStaticAbility* SecondAbility =
		NewObject<UStaticAbility>(GetTransientPackage());
	TestNotNull(TEXT("First gameplay ability"), FirstAbility);
	TestNotNull(TEXT("Second gameplay ability"), SecondAbility);
	if (!FirstAbility || !SecondAbility)
	{
		return false;
	}

	TestNotNull(TEXT("Resource runtime"), FirstAbility->GetResourceRuntime());
	TestNotNull(TEXT("Source runtime"), FirstAbility->GetSourceRuntime());
	TestNotNull(TEXT("Movement runtime"), FirstAbility->GetMovementRuntime());
	TestNotNull(
		TEXT("Presentation runtime"),
		FirstAbility->GetPresentationRuntime());
	if (!FirstAbility->GetResourceRuntime()
		|| !FirstAbility->GetSourceRuntime()
		|| !FirstAbility->GetMovementRuntime()
		|| !FirstAbility->GetPresentationRuntime())
	{
		return false;
	}

	TestTrue(
		TEXT("Resource runtime is ability-owned"),
		FirstAbility->GetResourceRuntime()->GetOuter() == FirstAbility);
	TestTrue(
		TEXT("Source runtime is ability-owned"),
		FirstAbility->GetSourceRuntime()->GetOuter() == FirstAbility);
	TestTrue(
		TEXT("Movement runtime is ability-owned"),
		FirstAbility->GetMovementRuntime()->GetOuter() == FirstAbility);
	TestTrue(
		TEXT("Presentation runtime is ability-owned"),
		FirstAbility->GetPresentationRuntime()->GetOuter() == FirstAbility);

	TestTrue(
		TEXT("Resource state is not shared between ability instances"),
		FirstAbility->GetResourceRuntime()
			!= SecondAbility->GetResourceRuntime());
	TestTrue(
		TEXT("Source state is not shared between ability instances"),
		FirstAbility->GetSourceRuntime() != SecondAbility->GetSourceRuntime());
	TestTrue(
		TEXT("Movement state is not shared between ability instances"),
		FirstAbility->GetMovementRuntime()
			!= SecondAbility->GetMovementRuntime());
	TestTrue(
		TEXT("Presentation state is not shared between ability instances"),
		FirstAbility->GetPresentationRuntime()
			!= SecondAbility->GetPresentationRuntime());

	FirstAbility->GetResourceRuntime()->MarkCooldownForAbilityEnd();
	TestFalse(
		TEXT("Pending cooldown remains isolated from another ability"),
		SecondAbility->GetResourceRuntime()->ConsumePendingCooldown());
	TestTrue(
		TEXT("Pending cooldown can be consumed by its owner"),
		FirstAbility->GetResourceRuntime()->ConsumePendingCooldown());
	TestFalse(
		TEXT("Pending cooldown is cleared after consumption"),
		FirstAbility->GetResourceRuntime()->ConsumePendingCooldown());

	FirstAbility->GetResourceRuntime()->MarkCooldownForAbilityEnd();
	TestFalse(
		TEXT("Cancelled execution does not consume a cooldown"),
		FirstAbility->GetResourceRuntime()->ConsumePendingCooldown(true));
	TestFalse(
		TEXT("Cancelled execution clears its stale pending cooldown"),
		FirstAbility->GetResourceRuntime()->ConsumePendingCooldown());

	FSkillGameplayEffectConfig DamageConfig;
	DamageConfig.Magnitude = 125.0;
	TestEqual(
		TEXT("Source runtime preserves configured base damage"),
		FirstAbility->GetSourceRuntime()
			->CalculateBaseSkillDamageMagnitude(DamageConfig),
		125.0f);
	DamageConfig.Magnitude = -25.0;
	TestEqual(
		TEXT("Source runtime clamps negative base damage"),
		FirstAbility->GetSourceRuntime()
			->CalculateBaseSkillDamageMagnitude(DamageConfig),
		0.0f);

	TestFalse(
		TEXT("Movement runtime starts unlocked"),
		FirstAbility->GetMovementRuntime()->IsMovementLocked());
	TestFalse(
		TEXT("Movement contact damage starts inactive"),
		FirstAbility->GetMovementRuntime()->IsContactDamageActive());
	TestNull(
		TEXT("Presentation runtime starts without an actor"),
		FirstAbility->GetPresentationRuntime()
			->GetActivePresentationActor());

	return !HasAnyErrors();
}

#endif
