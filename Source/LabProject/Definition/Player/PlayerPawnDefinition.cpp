#include "Definition/Player/PlayerPawnDefinition.h"

#include "Common/LabGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerPawnDefinition)

namespace
{
	constexpr const TCHAR* DefaultPlayerPawnDefinitionPath =
		TEXT("/Game/Data/DA_PlayerPawn.DA_PlayerPawn");

	bool RepairLegacyPitchEncodedYaw(FRotator& RotationRate, const float ExpectedYaw)
	{
		if (!FMath::IsNearlyEqual(RotationRate.Pitch, ExpectedYaw)
			|| !FMath::IsNearlyZero(RotationRate.Yaw)
			|| !FMath::IsNearlyZero(RotationRate.Roll))
		{
			return false;
		}

		RotationRate = FRotator(0.0f, ExpectedYaw, 0.0f);
		return true;
	}
}

UPlayerPawnDefinition::UPlayerPawnDefinition()
{
	Camera.OcclusionSurfaceObjectTypes =
	{
		ECC_WorldStatic,
		ECC_WorldDynamic
	};

	ActionPolicy.MovementHitReactCancelTags.AddTag(LabGameplayTags::Action_HitReact);
	ActionPolicy.MovementHitReactCancelTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
}

void UPlayerPawnDefinition::PostLoad()
{
	Super::PostLoad();

	// DA_PlayerPawn was once saved with the intended yaw values in Pitch.
	// Repair only that exact legacy shape; arbitrary invalid designer values
	// remain visible to IsDataValid instead of being silently overwritten.
	const bool bRepairedDefault =
		RepairLegacyPitchEncodedYaw(Aim.DefaultRotationRate, 500.0f);
	const bool bRepairedAiming =
		RepairLegacyPitchEncodedYaw(Aim.AimingRotationRate, 3000.0f);

#if WITH_EDITOR
	if ((bRepairedDefault || bRepairedAiming) && !IsRunningCommandlet())
	{
		MarkPackageDirty();
	}
#endif
}

FPrimaryAssetId UPlayerPawnDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PlayerPawnDefinition"), GetFName());
}

FSoftObjectPath UPlayerPawnDefinition::GetDefaultDefinitionPath()
{
	return FSoftObjectPath(DefaultPlayerPawnDefinitionPath);
}

#if WITH_EDITOR
EDataValidationResult UPlayerPawnDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	auto MarkInvalid = [&Context, &Result](const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	};

	if (Interaction.BoxExtent.ContainsNaN()
		|| Interaction.BoxExtent.X <= 0.0f
		|| Interaction.BoxExtent.Y <= 0.0f
		|| Interaction.BoxExtent.Z <= 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidInteractionExtent",
			"Interaction BoxExtent must contain finite positive values."));
	}

	if (!FMath::IsFinite(Interaction.ServerValidationDistance)
		|| Interaction.ServerValidationDistance < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidInteractionValidationDistance",
			"Interaction ServerValidationDistance must be finite and non-negative."));
	}

	if (!FMath::IsFinite(Camera.TargetArmLength) || Camera.TargetArmLength < 0.0f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidCameraArmLength",
			"Camera TargetArmLength must be finite and non-negative."));
	}

	if (!FMath::IsFinite(Aim.ReplicationInterval) || Aim.ReplicationInterval < 0.05f)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidAimReplicationInterval",
			"Aim ReplicationInterval must be finite and at least 0.05 seconds."));
	}

	const auto HasUsableYawRotationRate = [](const FRotator& RotationRate)
	{
		return FMath::IsFinite(RotationRate.Pitch)
			&& FMath::IsFinite(RotationRate.Yaw)
			&& FMath::IsFinite(RotationRate.Roll)
			&& !FMath::IsNearlyZero(RotationRate.Yaw);
	};
	if (!HasUsableYawRotationRate(Aim.DefaultRotationRate))
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidDefaultRotationRate",
			"Aim DefaultRotationRate must be finite and have a non-zero Yaw rate."));
	}
	if (!HasUsableYawRotationRate(Aim.AimingRotationRate))
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidAimingRotationRate",
			"Aim AimingRotationRate must be finite and have a non-zero Yaw rate."));
	}

	if (!FMath::IsFinite(Camera.OcclusionDisableDistance)
		|| !FMath::IsFinite(Camera.OcclusionReenableDistance)
		|| Camera.OcclusionDisableDistance < 0.0f
		|| Camera.OcclusionReenableDistance < Camera.OcclusionDisableDistance)
	{
		MarkInvalid(NSLOCTEXT(
			"PlayerPawnDefinition",
			"InvalidOcclusionDistances",
			"Camera occlusion distances must be finite and ReenableDistance must not be smaller than DisableDistance."));
	}

	if (ActionPolicy.MovementHitReactCancelTags.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT(
			"PlayerPawnDefinition",
			"EmptyHitReactCancelTags",
			"MovementHitReactCancelTags is empty; the runtime native fallback tags will be used."));
	}

	return Result;
}
#endif
