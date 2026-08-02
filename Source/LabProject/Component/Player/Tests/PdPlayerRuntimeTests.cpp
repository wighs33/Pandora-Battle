#if WITH_DEV_AUTOMATION_TESTS

#include "Character/PdPlayer.h"

#include "Component/Player/PlayerAimComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdPlayerPawnDefinitionAimAssetTest,
	"LabProject.Player.Data.PlayerPawnDefinitionAim",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdPlayerPawnDefinitionAimAssetTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const UPlayerPawnDefinition* Definition = LoadObject<UPlayerPawnDefinition>(
		nullptr,
		TEXT("/Game/Data/DA_PlayerPawn.DA_PlayerPawn"));
	TestNotNull(TEXT("DA_PlayerPawn loads"), Definition);
	if (!Definition)
	{
		return false;
	}

	const FPlayerAimSettings& AimSettings = Definition->GetAimSettings();
	TestTrue(
		TEXT("Default rotation rate is stored on Yaw"),
		FMath::IsNearlyZero(AimSettings.DefaultRotationRate.Pitch)
			&& FMath::IsNearlyEqual(AimSettings.DefaultRotationRate.Yaw, 500.0f)
			&& FMath::IsNearlyZero(AimSettings.DefaultRotationRate.Roll));
	TestTrue(
		TEXT("Aiming rotation rate is stored on Yaw"),
		FMath::IsNearlyZero(AimSettings.AimingRotationRate.Pitch)
			&& FMath::IsNearlyEqual(AimSettings.AimingRotationRate.Yaw, 3000.0f)
			&& FMath::IsNearlyZero(AimSettings.AimingRotationRate.Roll));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdPlayerAimMovementContractTest,
	"LabProject.Player.Runtime.AimMovementContract",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdPlayerAimMovementContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		TEXT("PdPlayerRuntimeTestWorld"));
	TestNotNull(TEXT("Test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	APdPlayer* Player = TestWorld->SpawnActor<APdPlayer>();
	TestNotNull(TEXT("Player"), Player);
	UPlayerAimComponent* AimComponent =
		Player ? Player->GetPlayerAimComponent() : nullptr;
	UCharacterMovementComponent* MovementComponent =
		Player ? Player->GetCharacterMovement() : nullptr;
	TestNotNull(TEXT("Aim component"), AimComponent);
	TestNotNull(TEXT("Movement component"), MovementComponent);

	if (Player && AimComponent && MovementComponent)
	{
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion = false;

		FPlayerAimSettings InvalidSettings;
		InvalidSettings.DefaultRotationRate = FRotator::ZeroRotator;
		InvalidSettings.AimingRotationRate = FRotator::ZeroRotator;
		AimComponent->ApplySettings(InvalidSettings);
		AimComponent->SetWeaponAimActive(true, FWeaponAimCameraSettings());

		TestFalse(
			TEXT("Aiming does not orient toward movement"),
			MovementComponent->bOrientRotationToMovement);
		TestTrue(
			TEXT("Aiming rotates toward controller direction"),
			MovementComponent->bUseControllerDesiredRotation);
		TestTrue(
			TEXT("Aiming has a usable fallback yaw rate"),
			MovementComponent->RotationRate.Yaw > UE_SMALL_NUMBER);
		TestTrue(
			TEXT("Aiming can rotate while a root-motion montage is active"),
			MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion);

		// Temporary gameplay systems may own movement rotation while active.
		// Once released, the player's current steady-state contract must win.
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion = false;
		Player->ReapplyCurrentRotationPolicy();

		TestFalse(
			TEXT("Current aim policy replaces stale movement orientation"),
			MovementComponent->bOrientRotationToMovement);
		TestTrue(
			TEXT("Current aim policy restores controller rotation"),
			MovementComponent->bUseControllerDesiredRotation);
		TestTrue(
			TEXT("Current aim policy replaces a stale zero rotation rate"),
			MovementComponent->RotationRate.Yaw > UE_SMALL_NUMBER);
		TestTrue(
			TEXT("Current aim policy restores root-motion physics rotation"),
			MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion);

		AimComponent->SetWeaponAimActive(false, FWeaponAimCameraSettings());
		TestTrue(
			TEXT("Normal locomotion again orients toward movement"),
			MovementComponent->bOrientRotationToMovement);
		TestFalse(
			TEXT("Normal locomotion stops controller-desired rotation"),
			MovementComponent->bUseControllerDesiredRotation);
		TestTrue(
			TEXT("Normal locomotion has a usable fallback yaw rate"),
			MovementComponent->RotationRate.Yaw > UE_SMALL_NUMBER);
		TestFalse(
			TEXT("Normal locomotion restores the root-motion rotation default"),
			MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion);
	}

	TestWorld->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
