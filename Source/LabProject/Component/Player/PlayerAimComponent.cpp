#include "Component/Player/PlayerAimComponent.h"

#include "Character/PdPlayer.h"
#include "Component/Player/PlayerCameraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerAimComponent)

namespace
{
	FRotator SanitizeRotationRate(
		const FRotator& RotationRate,
		const FRotator& Fallback)
	{
		const bool bIsFinite =
			FMath::IsFinite(RotationRate.Pitch)
			&& FMath::IsFinite(RotationRate.Yaw)
			&& FMath::IsFinite(RotationRate.Roll);
		return bIsFinite && !FMath::IsNearlyZero(RotationRate.Yaw)
			? RotationRate
			: Fallback;
	}
}

UPlayerAimComponent::UPlayerAimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPlayerAimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopReplication();
	Super::EndPlay(EndPlayReason);
}

void UPlayerAimComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams StateParams;
	StateParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerAimComponent, bWeaponAimActive, StateParams);

	FDoRepLifetimeParams OffsetParams;
	OffsetParams.bIsPushBased = true;
	OffsetParams.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerAimComponent, ReplicatedAimYaw, OffsetParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerAimComponent, ReplicatedAimPitch, OffsetParams);
}

void UPlayerAimComponent::ApplySettings(const FPlayerAimSettings& Settings)
{
	const bool bRestartReplication =
		GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ReplicationTimerHandle);
	if (bRestartReplication)
	{
		StopReplication();
	}

	ReplicationInterval = FMath::Max(Settings.ReplicationInterval, 0.05f);
	DefaultRotationRate = SanitizeRotationRate(
		Settings.DefaultRotationRate,
		FRotator(0.0f, 500.0f, 0.0f));
	AimingRotationRate = SanitizeRotationRate(
		Settings.AimingRotationRate,
		FRotator(0.0f, 3000.0f, 0.0f));

	if (APdPlayer* Player = GetPlayerOwner();
		Player && !Player->IsStatusFrozen())
	{
		ApplyMovementSettings(Player->GetCharacterMovement());
	}

	if (bRestartReplication)
	{
		StartReplication();
	}
}

void UPlayerAimComponent::StartReplication()
{
	APdPlayer* Player = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!Player || !World || !Player->HasAuthority() || !Player->IsPlayerControlled())
	{
		return;
	}

	UpdateReplicatedAimOffset();
	if (!World->GetTimerManager().IsTimerActive(ReplicationTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			ReplicationTimerHandle,
			this,
			&ThisClass::UpdateReplicatedAimOffset,
			ReplicationInterval,
			true);
	}
}

void UPlayerAimComponent::StopReplication()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReplicationTimerHandle);
	}
}

void UPlayerAimComponent::SetWeaponAimActive(
	const bool bEnabled,
	const FWeaponAimCameraSettings& CameraSettings)
{
	const FWeaponAimCameraSettings SafeSettings =
		UPlayerCameraComponent::SanitizeAimCameraSettings(CameraSettings);
	ApplyWeaponAimState(bEnabled, SafeSettings);

	const APdPlayer* Player = GetPlayerOwner();
	if (Player
		&& Player->IsLocallyControlled()
		&& !Player->HasAuthority())
	{
		ServerSetWeaponAimActive(bEnabled, SafeSettings);
	}
}

void UPlayerAimComponent::ServerSetWeaponAimActive_Implementation(
	const bool bEnabled,
	FWeaponAimCameraSettings CameraSettings)
{
	APdPlayer* Player = GetPlayerOwner();
	const UEquipmentComponent* EquipmentComponent = Player
		? Player->GetEquipmentComponent()
		: nullptr;
	const AWeaponBase* CurrentWeapon = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponActor()
		: nullptr;
	if (bEnabled
		&& (!CurrentWeapon
			|| !CurrentWeapon->CanUseRangedWeapon(Player, false)))
	{
		ApplyWeaponAimState(
			false,
			FWeaponAimCameraSettings());
		return;
	}

	ApplyWeaponAimState(
		bEnabled,
		UPlayerCameraComponent::SanitizeAimCameraSettings(CameraSettings));
}

void UPlayerAimComponent::OnRep_WeaponAimActive()
{
	ApplyWeaponAimState(bWeaponAimActive, ActiveWeaponAimCameraSettings);
}

void UPlayerAimComponent::OnRep_ReplicatedAimOffset()
{
	if (APdPlayer* Player = GetPlayerOwner(); Player && !Player->IsLocallyControlled())
	{
		Player->SetAimOffsetForAnimation(
			FRotator::NormalizeAxis(FRotator::DecompressAxisFromByte(ReplicatedAimYaw)),
			FRotator::NormalizeAxis(FRotator::DecompressAxisFromByte(ReplicatedAimPitch)));
	}
}

APdPlayer* UPlayerAimComponent::GetPlayerOwner() const
{
	return Cast<APdPlayer>(GetOwner());
}

void UPlayerAimComponent::ApplyWeaponAimState(
	const bool bEnabled,
	const FWeaponAimCameraSettings& CameraSettings)
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player)
	{
		return;
	}

	if (bEnabled)
	{
		ActiveWeaponAimCameraSettings = CameraSettings;
	}

	if (UPlayerCameraComponent* CameraComponent = Player->GetPlayerCameraComponent())
	{
		CameraComponent->SetWeaponAimActive(bEnabled, CameraSettings);
	}

	const bool bStateChanged = bWeaponAimActive != bEnabled;
	bWeaponAimActive = bEnabled;
	if (Player->HasAuthority() && bStateChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			UPlayerAimComponent,
			bWeaponAimActive,
			this);
		Player->ForceNetUpdate();
	}

	if (bEnabled)
	{
		// Relinking after the state flip lets a newly created item layer read
		// IsWeaponAimActive() as true during NativeInitializeAnimation. This
		// also repairs layers left stale by an interrupted skill montage.
		if (UEquipmentComponent* EquipmentComponent =
			Player->GetEquipmentComponent())
		{
			EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
		}
	}

	if (!Player->IsStatusFrozen())
	{
		ApplyMovementSettings(Player->GetCharacterMovement());
	}
}

void UPlayerAimComponent::CacheMovementDefaults(
	UCharacterMovementComponent* MovementComponent)
{
	if (!MovementComponent || bHasCachedMovementDefaults)
	{
		return;
	}

	bDefaultAllowPhysicsRotationDuringAnimRootMotion =
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion;
	bHasCachedMovementDefaults = true;
}

void UPlayerAimComponent::ApplyMovementSettings(
	UCharacterMovementComponent* MovementComponent)
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player || !MovementComponent)
	{
		return;
	}

	CacheMovementDefaults(MovementComponent);

	MovementComponent->bOrientRotationToMovement = !bWeaponAimActive;
	MovementComponent->bUseControllerDesiredRotation = bWeaponAimActive;
	MovementComponent->RotationRate =
		bWeaponAimActive ? AimingRotationRate : DefaultRotationRate;
	MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion =
		bWeaponAimActive
		|| bDefaultAllowPhysicsRotationDuringAnimRootMotion;
	Player->bUseControllerRotationYaw = false;
}

void UPlayerAimComponent::UpdateReplicatedAimOffset()
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player || !Player->HasAuthority() || !Player->IsPlayerControlled())
	{
		return;
	}

	Player->UpdateAimOffsetForAnimation();
	const uint8 NewAimYaw =
		FRotator::CompressAxisToByte(Player->GetAimYawForAnimation());
	const uint8 NewAimPitch =
		FRotator::CompressAxisToByte(Player->GetAimPitchForAnimation());

	if (ReplicatedAimYaw != NewAimYaw)
	{
		ReplicatedAimYaw = NewAimYaw;
		MARK_PROPERTY_DIRTY_FROM_NAME(
			UPlayerAimComponent,
			ReplicatedAimYaw,
			this);
	}

	if (ReplicatedAimPitch != NewAimPitch)
	{
		ReplicatedAimPitch = NewAimPitch;
		MARK_PROPERTY_DIRTY_FROM_NAME(
			UPlayerAimComponent,
			ReplicatedAimPitch,
			this);
	}
}
