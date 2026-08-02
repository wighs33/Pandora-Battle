#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Character/CharacterHitValidation.h"
#include "Character/PdPlayer.h"
#include "Component/Player/PlayerInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Item/RewardChest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillCollisionPolicyTest,
	"LabProject.AbilitySystem.SkillCollisionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillCollisionPolicyTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	APdPlayer* PlayerCDO = GetMutableDefault<APdPlayer>();
	TestNotNull(TEXT("Player CDO exists"), PlayerCDO);
	if (!PlayerCDO)
	{
		return false;
	}

	UPlayerInteractionComponent* InteractionSensor =
		PlayerCDO->GetPlayerInteractionComponent();
	USkeletalMeshComponent* CharacterMesh = PlayerCDO->GetMesh();
	UCapsuleComponent* CharacterCapsule = PlayerCDO->GetCapsuleComponent();
	TestNotNull(TEXT("Interaction sensor exists"), InteractionSensor);
	TestNotNull(TEXT("Character mesh exists"), CharacterMesh);
	TestNotNull(TEXT("Character capsule exists"), CharacterCapsule);

	if (InteractionSensor)
	{
		TestTrue(
			TEXT("Interaction sensor uses the dedicated OverlapBox object channel"),
			InteractionSensor->GetCollisionObjectType() == ECC_GameTraceChannel3);
		TestTrue(
			TEXT("Interaction sensor ignores native skill projectiles"),
			InteractionSensor->GetCollisionResponseToChannel(ECC_GameTraceChannel2)
				== ECR_Ignore);
	}

	if (CharacterMesh)
	{
		TestTrue(
			TEXT("Primary character mesh uses the HitableBody object channel"),
			CharacterMesh->GetCollisionObjectType() == ECC_GameTraceChannel1);
		TestTrue(
			TEXT("Primary character mesh blocks native skill projectiles"),
			CharacterMesh->GetCollisionResponseToChannel(ECC_GameTraceChannel2)
				== ECR_Block);
		TestTrue(
			TEXT("Primary mesh resolves as valid character damage geometry"),
			PdCharacterHitValidation::ResolveDirectMeshHit(PlayerCDO, CharacterMesh)
				== PlayerCDO);
	}

	if (CharacterCapsule)
	{
		TestTrue(
			TEXT("Movement capsule ignores native skill projectiles"),
			CharacterCapsule->GetCollisionResponseToChannel(ECC_GameTraceChannel2)
				== ECR_Ignore);
	}

	if (InteractionSensor)
	{
		TestTrue(
			TEXT("Interaction sensor is rejected as character damage geometry"),
			PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(
				PlayerCDO,
				InteractionSensor));
	}

	AProjectileBase* ProjectileCDO = GetMutableDefault<AProjectileBase>();
	USphereComponent* ProjectileCollision = ProjectileCDO
		? ProjectileCDO->FindComponentByClass<USphereComponent>()
		: nullptr;
	TestNotNull(TEXT("Native skill projectile collision exists"), ProjectileCollision);
	if (ProjectileCollision)
	{
		TestTrue(
			TEXT("Skill projectiles ignore character movement capsules"),
			ProjectileCollision->GetCollisionResponseToChannel(ECC_Pawn)
				== ECR_Ignore);
		TestTrue(
			TEXT("Skill projectiles block the primary character mesh channel"),
			ProjectileCollision->GetCollisionResponseToChannel(ECC_GameTraceChannel1)
				== ECR_Block);
		TestTrue(
			TEXT("Skill projectiles ignore interaction sensor volumes"),
			ProjectileCollision->GetCollisionResponseToChannel(ECC_GameTraceChannel3)
				== ECR_Ignore);
	}

	ARewardChest* RewardChestCDO = GetMutableDefault<ARewardChest>();
	USkeletalMeshComponent* RewardChestCollision = RewardChestCDO
		? RewardChestCDO->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	TestNotNull(TEXT("Reward chest collision exists"), RewardChestCollision);
	if (RewardChestCollision)
	{
		TestTrue(
			TEXT("Reward chest overlaps the dedicated interaction sensor channel"),
			RewardChestCollision->GetCollisionResponseToChannel(ECC_GameTraceChannel3)
				== ECR_Overlap);
	}

	USkillDefinition* IcicleDefinition = LoadObject<USkillDefinition>(
		nullptr,
		TEXT("/Game/Pandora/Skill/Ice/DA_Skill_Icicle.DA_Skill_Icicle"));
	TestNotNull(TEXT("Icicle skill definition loads"), IcicleDefinition);
	if (IcicleDefinition)
	{
		TestTrue(
			TEXT("Icicle remains embedded after a real impact"),
			IcicleDefinition->ProjectileSettings.bStickOnImpact);
		TestTrue(
			TEXT("Icicle embedded lifetime is one second"),
			FMath::IsNearlyEqual(
				IcicleDefinition->ProjectileSettings.PostImpactLifeSpan,
				1.0));
	}

	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
