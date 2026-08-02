#if WITH_DEV_AUTOMATION_TESTS

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystem/Ability/HitReactAbility.h"
#include "AbilitySystem/Ability/ProjectileAbility.h"
#include "Component/AbilitySystem/PdAbilityAttributeRuntime.h"
#include "Component/AbilitySystem/PdAbilityCollectionRuntime.h"
#include "Component/AbilitySystem/PdAbilityResetRuntime.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdAbilitySystemRuntimeCompositionTest,
	"LabProject.AbilitySystem.RuntimeComposition",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdAbilitySystemRuntimeCompositionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const UPdAbilitySystemComponent* AbilitySystemComponent =
		NewObject<UPdAbilitySystemComponent>(GetTransientPackage());
	TestNotNull(TEXT("AbilitySystemComponent"), AbilitySystemComponent);
	if (!AbilitySystemComponent)
	{
		return false;
	}

	TestNotNull(TEXT("Attribute runtime"), AbilitySystemComponent->GetAttributeRuntime());
	TestNotNull(TEXT("Collection runtime"), AbilitySystemComponent->GetCollectionRuntime());
	TestNotNull(TEXT("Reset runtime"), AbilitySystemComponent->GetResetRuntime());

	const UProjectileAbility* ProjectileAbility = GetDefault<UProjectileAbility>();
	TestNotNull(TEXT("Projectile ability CDO"), ProjectileAbility);
	if (ProjectileAbility)
	{
		TestFalse(
			TEXT("Projectile skills require a separate fire confirmation"),
			ProjectileAbility->ShouldAutoConfirmOnInputRelease());
	}

	const UHitReactAbility* NonProjectileAbility = GetDefault<UHitReactAbility>();
	TestNotNull(TEXT("Non-projectile ability CDO"), NonProjectileAbility);
	if (NonProjectileAbility)
	{
		TestTrue(
			TEXT("Non-projectile abilities keep the shared release policy"),
			NonProjectileAbility->ShouldAutoConfirmOnInputRelease());
	}

	const UGameSettingDefinition* SettingDefinition =
		GetDefault<UGameSettingDefinition>();
	TestNotNull(TEXT("Game setting definition"), SettingDefinition);
	if (!SettingDefinition)
	{
		return false;
	}

	TestEqual(
		TEXT("Core attribute mapping count"),
		SettingDefinition->CoreAttributeConfig.AttributeMappings.Num(),
		28);

	TSet<FGameplayTag> UniqueTags;
	for (const FPdAttributeTagMapping& Mapping :
		SettingDefinition->CoreAttributeConfig.AttributeMappings)
	{
		TestTrue(
			*FString::Printf(TEXT("Valid mapping: %s"), *Mapping.StatTag.ToString()),
			Mapping.IsValid());
		UniqueTags.Add(Mapping.StatTag);
	}

	TestEqual(TEXT("Unique core attribute tags"), UniqueTags.Num(), 28);

	const UGameSettingDefinition* SettingAsset =
		LoadObject<UGameSettingDefinition>(
			nullptr,
			TEXT("/Game/Data/DA_Setting.DA_Setting"));
	TestNotNull(TEXT("DA_Setting"), SettingAsset);
	if (SettingAsset)
	{
		TestEqual(
			TEXT("DA_Setting core attribute mapping count"),
			SettingAsset->CoreAttributeConfig.AttributeMappings.Num(),
			28);
	}

	const USkillDefinition* IcicleSkill =
		LoadObject<USkillDefinition>(
			nullptr,
			TEXT("/Game/Pandora/Skill/Ice/DA_Skill_Icicle.DA_Skill_Icicle"));
	TestNotNull(TEXT("DA_Skill_Icicle"), IcicleSkill);
	if (IcicleSkill)
	{
		TestTrue(
			TEXT("Icicle persists after a real impact"),
			IcicleSkill->ProjectileSettings.bStickOnImpact);
		TestEqual(
			TEXT("Icicle post-impact lifespan"),
			IcicleSkill->ProjectileSettings.PostImpactLifeSpan,
			1.0);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdHitReactAbilityNetworkPolicyTest,
	"LabProject.AbilitySystem.HitReact.NetworkPolicy",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdHitReactAbilityNetworkPolicyTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);

	const UHitReactAbility* NativeHitReactAbility =
		GetDefault<UHitReactAbility>();
	TestNotNull(TEXT("Native HitReact ability"), NativeHitReactAbility);
	if (NativeHitReactAbility)
	{
		TestEqual(
			TEXT("Native HitReact keeps independent executions"),
			NativeHitReactAbility->GetInstancingPolicy(),
			EGameplayAbilityInstancingPolicy::InstancedPerExecution);
		TestEqual(
			TEXT("Native HitReact does not replicate unsupported per-execution instances"),
			NativeHitReactAbility->GetReplicationPolicy(),
			EGameplayAbilityReplicationPolicy::ReplicateNo);
		TestEqual(
			TEXT("Native HitReact remains server initiated"),
			NativeHitReactAbility->GetNetExecutionPolicy(),
			EGameplayAbilityNetExecutionPolicy::ServerInitiated);
	}

	UClass* ConfiguredHitReactClass = LoadClass<UHitReactAbility>(
		nullptr,
		TEXT("/Game/GAS/Ability/GA_HitReact.GA_HitReact_C"));
	const UHitReactAbility* ConfiguredHitReactAbility =
		ConfiguredHitReactClass
			? ConfiguredHitReactClass->GetDefaultObject<UHitReactAbility>()
			: nullptr;
	TestNotNull(TEXT("GA_HitReact configured ability"), ConfiguredHitReactAbility);
	if (ConfiguredHitReactAbility)
	{
		TestEqual(
			TEXT("GA_HitReact keeps independent executions"),
			ConfiguredHitReactAbility->GetInstancingPolicy(),
			EGameplayAbilityInstancingPolicy::InstancedPerExecution);
		TestEqual(
			TEXT("GA_HitReact does not override the safe replication policy"),
			ConfiguredHitReactAbility->GetReplicationPolicy(),
			EGameplayAbilityReplicationPolicy::ReplicateNo);
		TestEqual(
			TEXT("GA_HitReact remains server initiated"),
			ConfiguredHitReactAbility->GetNetExecutionPolicy(),
			EGameplayAbilityNetExecutionPolicy::ServerInitiated);
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdAbilityRuntimeResetInstanceEligibilityTest,
	"LabProject.AbilitySystem.RuntimeReset.InstanceEligibility",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdAbilityRuntimeResetInstanceEligibilityTest::RunTest(
	const FString& Parameters)
{
	static_cast<void>(Parameters);

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("PdAbilityRuntimeResetTestWorld"));
	AActor* OwnerActor = TestWorld ? TestWorld->SpawnActor<AActor>() : nullptr;
	UPdAbilitySystemComponent* AbilitySystemComponent =
		OwnerActor ? NewObject<UPdAbilitySystemComponent>(OwnerActor) : nullptr;
	UHitReactAbility* UnboundPerExecutionInstance = AbilitySystemComponent
		? NewObject<UHitReactAbility>(AbilitySystemComponent)
		: nullptr;
	TestNotNull(TEXT("Test world"), TestWorld);
	TestNotNull(TEXT("Owner actor"), OwnerActor);
	TestNotNull(TEXT("Ability system component"), AbilitySystemComponent);
	TestNotNull(
		TEXT("Unbound per-execution ability instance"),
		UnboundPerExecutionInstance);
	if (!TestWorld
		|| !OwnerActor
		|| !AbilitySystemComponent
		|| !UnboundPerExecutionInstance)
	{
		if (TestWorld)
		{
			TestWorld->DestroyWorld(false);
		}
		return false;
	}
	OwnerActor->AddInstanceComponent(AbilitySystemComponent);
	AbilitySystemComponent->RegisterComponent();
	AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, OwnerActor);

	// UE treats a valid InstancedPerExecution object as active even when this
	// client never initialized its CurrentActorInfo. This is the exact state
	// that used to reach SetCanBeCanceled during a replicated death reset.
	TestTrue(
		TEXT("Fixture per-execution instance is considered active"),
		UnboundPerExecutionInstance->IsActive());
	TestNull(
		TEXT("Fixture instance has no actor info"),
		UnboundPerExecutionInstance->GetCurrentActorInfo());

	FGameplayAbilitySpec AbilitySpec(UHitReactAbility::StaticClass());
	AbilitySpec.ActiveCount = 1;
	AbilitySpec.InputPressed = true;
	AbilitySpec.NonReplicatedInstances.Add(UnboundPerExecutionInstance);
	const FGameplayAbilitySpecHandle AbilityHandle = AbilitySpec.Handle;
	AbilitySystemComponent->GetActivatableAbilities().Add(MoveTemp(AbilitySpec));

	AbilitySystemComponent->ResetAbilityRuntimeStateForDeath();

	const FGameplayAbilitySpec* ResetSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
	TestNotNull(TEXT("Reset keeps the ability spec"), ResetSpec);
	if (ResetSpec)
	{
		TestFalse(TEXT("Reset clears held input"), ResetSpec->InputPressed);
	}
	TestFalse(
		TEXT("Reset does not mutate an active unbound instance"),
		UnboundPerExecutionInstance->CanBeCanceled());

	// The deliberately malformed replicated state must not leak into the ASC's
	// normal destruction path, which correctly expects every active instance to
	// have completed EndAbility.
	if (FGameplayAbilitySpec* FixtureSpec =
		AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle))
	{
		FixtureSpec->ActiveCount = 0;
		FixtureSpec->NonReplicatedInstances.Reset();
		FixtureSpec->ReplicatedInstances.Reset();
	}
	TestWorld->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
