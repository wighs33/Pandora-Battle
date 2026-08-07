#include "Component/Character/CharacterDeathComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/CharacterAbilityRuntimeComponent.h"
#include "Component/Character/CharacterHealthBarComponent.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/UI/DamageIndicatorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdHUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterDeathComponent)

UCharacterDeathComponent::UCharacterDeathComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterDeathComponent::ApplySettings(
	const FCharacterDeathSettings& InSettings)
{
	Settings = InSettings;
}

void UCharacterDeathComponent::InitializeDeathRuntime()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	CacheInitialRespawnState();
	ConfigureWeaponDamageMesh(Character->GetMesh());
	ResetDeathDissolve();
}

void UCharacterDeathComponent::ShutdownDeathRuntime()
{
	DeathDissolveMaterialInstances.Reset();
	bDeathDissolveActive = false;
}

void UCharacterDeathComponent::TickRuntime(const float DeltaSeconds)
{
	if (bDeathDissolveActive)
	{
		UpdateDeathDissolve(DeltaSeconds);
	}
}

void UCharacterDeathComponent::HandleDeadTagChanged(
	const int32 NewCount,
	UAbilitySystemComponent* BoundAbilitySystemComponent)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	if (NewCount <= 0)
	{
		const bool bHadDeathVisualState = bDeathHandled;
		bDeathHandled = false;
		if (!Character->HasAuthority() && bHadDeathVisualState)
		{
			Character->ResetDeathStateForRespawn();
		}
		return;
	}

	if (!Character->HasAuthority() && BoundAbilitySystemComponent)
	{
		const float CurrentHealth =
			BoundAbilitySystemComponent->GetNumericAttribute(
				UBasicAttributeSet::GetHealthAttribute());
		if (CurrentHealth > 0.0f && !bDeathHandled)
		{
			return;
		}
	}

	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;

	if (UPdAbilitySystemComponent* PdASC =
		Character->GetPdAbilitySystemComponent())
	{
		PdASC->ResetAbilityRuntimeStateForDeath();
	}

	Character->HandleDeath();
	if (!Character->HasAuthority())
	{
		return;
	}

	Character->MulticastHandleDeath();
	AController* DeathController = Character->GetController();
	if (!DeathController || !DeathController->IsPlayerController())
	{
		return;
	}

	if (AExperienceGameMode* ExperienceGameMode =
		Character->GetWorld()
			? Character->GetWorld()->GetAuthGameMode<AExperienceGameMode>()
			: nullptr)
	{
		ExperienceGameMode->RequestPlayerRespawn(
			DeathController,
			Character);
	}
	else if (ALobbyGameMode* LobbyGameMode =
		Character->GetWorld()
			? Character->GetWorld()->GetAuthGameMode<ALobbyGameMode>()
			: nullptr)
	{
		LobbyGameMode->RequestLobbyPlayerRespawn(
			DeathController,
			Character);
	}
}

void UCharacterDeathComponent::HandleRemoteDeath()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->HasAuthority() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	if (UPdAbilitySystemComponent* PdASC =
		Character->GetPdAbilitySystemComponent())
	{
		PdASC->ResetAbilityRuntimeStateForDeath();
	}
	Character->HandleDeath();
}

void UCharacterDeathComponent::ApplyDeathPhysics()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	if (UCharacterHealthBarComponent* HealthBar =
		Character->GetCharacterHealthBarComponent())
	{
		HealthBar->SetVisibility(false, true);
	}

	if (UCharacterMovementComponent* MovementComponent =
		Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
	{
		CharacterMesh->SetCollisionEnabled(
			ECollisionEnabled::QueryAndPhysics);
		CharacterMesh->SetAllBodiesSimulatePhysics(true);
		CharacterMesh->SetSimulatePhysics(true);
		CharacterMesh->WakeAllRigidBodies();
		CharacterMesh->bBlendPhysics = true;

		const FVector DeathImpulse =
			(-Character->GetActorForwardVector()
				* Settings.ImpulseHorizontalStrength)
			+ (Character->GetActorRightVector()
				* Settings.ImpulseSideStrength)
			+ (FVector::UpVector * Settings.ImpulseUpwardStrength);
		const FVector DeathImpulseLocation =
			Character->GetActorLocation()
			+ FVector(0.0f, 0.0f, Settings.ImpulseLocationZOffset);
		CharacterMesh->AddImpulseAtLocation(
			DeathImpulse,
			DeathImpulseLocation);
	}
}

void UCharacterDeathComponent::ResetDeathStateForRespawn()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	CacheInitialRespawnState();
	if (Character->HasAuthority())
	{
		if (UPdAbilitySystemComponent* PdASC =
			Character->GetPdAbilitySystemComponent())
		{
			PdASC->ClearStatusEffectsForRespawn();
		}
	}

	if (UCharacterAbilityRuntimeComponent* AbilityRuntime =
		Character->GetCharacterAbilityRuntimeComponent())
	{
		AbilityRuntime->ClearFrozenStateForRespawn();
	}

	bDeathHandled = false;
	Character->SetActorHiddenInGame(false);
	Character->SetActorEnableCollision(true);

	if (AController* Controller = Character->GetController())
	{
		Controller->ResetIgnoreMoveInput();
		Controller->ResetIgnoreLookInput();
	}

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(InitialCapsuleCollisionEnabled);
	}

	if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
	{
		CharacterMesh->SetHiddenInGame(false, true);
		CharacterMesh->SetVisibility(true, true);
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetAllBodiesSimulatePhysics(false);
		CharacterMesh->bBlendPhysics = false;
		CharacterMesh->SetPhysicsBlendWeight(0.0f);
		CharacterMesh->PutAllRigidBodiesToSleep();
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			CharacterMesh->AttachToComponent(
				Capsule,
				FAttachmentTransformRules::KeepRelativeTransform);
		}
		CharacterMesh->SetRelativeTransform(
			InitialMeshRelativeTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		CharacterMesh->SetCollisionEnabled(InitialMeshCollisionEnabled);
		ConfigureWeaponDamageMesh(CharacterMesh);
		CharacterMesh->SetComponentTickEnabled(true);
	}

	Character->ApplyCameraCollisionIgnoreToCharacterComponents();
	ResetDeathDissolve();
	Character->ApplyTeamOverlayMaterial();

	if (UCharacterMovementComponent* MovementComponent =
		Character->GetCharacterMovement())
	{
		if (!MovementComponent->UpdatedComponent)
		{
			MovementComponent->SetUpdatedComponent(
				Character->GetCapsuleComponent());
		}
		MovementComponent->Activate(true);
		MovementComponent->SetComponentTickEnabled(true);
		MovementComponent->StopMovementImmediately();
		MovementComponent->ClearAccumulatedForces();
		const EMovementMode RestoredMovementMode =
			InitialRespawnMovementMode != MOVE_None
				? InitialRespawnMovementMode.GetValue()
				: MOVE_Walking;
		MovementComponent->SetMovementMode(RestoredMovementMode);
	}

	if (UCharacterAbilityRuntimeComponent* AbilityRuntime =
		Character->GetCharacterAbilityRuntimeComponent())
	{
		AbilityRuntime->ApplyMovementSpeedFromAttribute();
	}

	Character->InitializeAbilitySystemActorInfo();
	if (UPdAbilitySystemComponent* PdASC =
		Character->GetPdAbilitySystemComponent())
	{
		PdASC->ReactivateAutoActivatedAbilities();
	}

	if (UEquipmentComponent* EquipmentComponent =
		Character->GetEquipmentComponent())
	{
		EquipmentComponent->RefreshCurrentWeaponAnimationLayer();
	}
	else
	{
		Character->ResetAnimationToDefault();
	}

	if (UCharacterHealthBarComponent* HealthBar =
		Character->GetCharacterHealthBarComponent())
	{
		HealthBar->RefreshViewModel();
		HealthBar->SetVisibleForLocalViewer(false);
	}
}

float UCharacterDeathComponent::GetSafeDissolveDuration(
	const float RequestedDuration) const
{
	return RequestedDuration > 0.0f
		? RequestedDuration
		: FMath::Max(Settings.DissolveFallbackDuration, 0.01f);
}

void UCharacterDeathComponent::InitializeDeathDissolveMaterials()
{
	DeathDissolveMaterialInstances.Reset();
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| !Settings.bUseDissolve
		|| Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	const int32 MaterialCount = CharacterMesh->GetNumMaterials();
	DeathDissolveMaterialInstances.Reserve(MaterialCount);
	for (int32 MaterialIndex = 0;
		MaterialIndex < MaterialCount;
		++MaterialIndex)
	{
		UMaterialInterface* Material =
			CharacterMesh->GetMaterial(MaterialIndex);
		if (!Material)
		{
			DeathDissolveMaterialInstances.Add(nullptr);
			continue;
		}

		UMaterialInstanceDynamic* DynamicMaterial =
			Cast<UMaterialInstanceDynamic>(Material);
		if (!DynamicMaterial)
		{
			DynamicMaterial = CharacterMesh->CreateDynamicMaterialInstance(
				MaterialIndex,
				Material);
		}
		DeathDissolveMaterialInstances.Add(DynamicMaterial);
	}
}

void UCharacterDeathComponent::ResetDeathDissolve()
{
	bDeathDissolveActive = false;
	DeathDissolveElapsedSeconds = 0.0f;
	DeathDissolveDurationSeconds = 0.0f;
	InitializeDeathDissolveMaterials();
	SetDeathDissolveValue(Settings.DissolveInitialValue);

	if (ACharacterBase* Character = GetCharacterOwner())
	{
		Character->RefreshCharacterTickEnabled();
	}
}

void UCharacterDeathComponent::StartDeathDissolveLocal(
	const float DurationSeconds)
{
	ClearCharacterOverlayMaterialLocal();
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character
		|| !Settings.bUseDissolve
		|| Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	InitializeDeathDissolveMaterials();
	if (DeathDissolveMaterialInstances.IsEmpty())
	{
		return;
	}

	DeathDissolveElapsedSeconds = 0.0f;
	DeathDissolveDurationSeconds = FMath::Max(DurationSeconds, 0.01f);
	bDeathDissolveActive = true;
	SetDeathDissolveValue(Settings.DissolveInitialValue);
	Character->RefreshCharacterTickEnabled();
}

void UCharacterDeathComponent::UpdateDeathDissolve(
	const float DeltaSeconds)
{
	if (!bDeathDissolveActive)
	{
		return;
	}

	DeathDissolveElapsedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float DissolveAlpha = DeathDissolveDurationSeconds > 0.0f
		? FMath::Clamp(
			DeathDissolveElapsedSeconds / DeathDissolveDurationSeconds,
			0.0f,
			1.0f)
		: 1.0f;
	SetDeathDissolveValue(FMath::Lerp(
		Settings.DissolveInitialValue,
		Settings.DissolveTargetValue,
		DissolveAlpha));

	if (DissolveAlpha >= 1.0f)
	{
		bDeathDissolveActive = false;
		if (ACharacterBase* Character = GetCharacterOwner())
		{
			Character->RefreshCharacterTickEnabled();
		}
	}
}

void UCharacterDeathComponent::SetDeathDissolveValue(
	const float DissolveValue)
{
	if (!Settings.bUseDissolve
		|| Settings.DissolveScalarParameterName.IsNone())
	{
		return;
	}

	for (UMaterialInstanceDynamic* DynamicMaterial :
		DeathDissolveMaterialInstances)
	{
		if (DynamicMaterial)
		{
			DynamicMaterial->SetScalarParameterValue(
				Settings.DissolveScalarParameterName,
				DissolveValue);
		}
	}
}

void UCharacterDeathComponent::ClearCharacterOverlayMaterialLocal()
{
	if (ACharacterBase* Character = GetCharacterOwner())
	{
		if (UCharacterPresentationComponent* Presentation =
			Character->GetCharacterPresentationComponent())
		{
			Presentation->ClearCharacterOverlayMaterialLocal();
		}
	}
}

void UCharacterDeathComponent::CacheInitialRespawnState()
{
	if (bHasCachedRespawnInitialState)
	{
		return;
	}

	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	const ACharacterBase* DefaultCharacter =
		Character->GetClass()
			? Character->GetClass()->GetDefaultObject<ACharacterBase>()
			: nullptr;

	const USkeletalMeshComponent* SourceMesh =
		DefaultCharacter ? DefaultCharacter->GetMesh() : nullptr;
	if (!SourceMesh)
	{
		SourceMesh = Character->GetMesh();
	}
	if (SourceMesh)
	{
		InitialMeshRelativeTransform = SourceMesh->GetRelativeTransform();
		InitialMeshCollisionEnabled = SourceMesh->GetCollisionEnabled();
	}

	const UCapsuleComponent* SourceCapsule =
		DefaultCharacter ? DefaultCharacter->GetCapsuleComponent() : nullptr;
	if (!SourceCapsule)
	{
		SourceCapsule = Character->GetCapsuleComponent();
	}
	if (SourceCapsule)
	{
		InitialCapsuleCollisionEnabled = SourceCapsule->GetCollisionEnabled();
	}

	const UCharacterMovementComponent* SourceMovement =
		DefaultCharacter
			? DefaultCharacter->GetCharacterMovement()
			: nullptr;
	if (!SourceMovement)
	{
		SourceMovement = Character->GetCharacterMovement();
	}
	if (SourceMovement)
	{
		InitialRespawnMovementMode =
			SourceMovement->DefaultLandMovementMode != MOVE_None
				? SourceMovement->DefaultLandMovementMode.GetValue()
				: MOVE_Walking;
	}
	bHasCachedRespawnInitialState = true;
}

void UCharacterDeathComponent::HandleDamageTaken(
	const float DamageAmount,
	const bool bCriticalHit)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (Character && Character->HasAuthority())
	{
		Character->MulticastHandleDamageTaken(
			FMath::Max(DamageAmount, 0.0f),
			bCriticalHit,
			Character->GetDamageIndicatorWorldLocation());
	}
}

void UCharacterDeathComponent::HandleRemoteDamageTaken(
	const float DamageAmount,
	const bool bCriticalHit,
	const FVector WorldLocation)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float DisplayDamageAmount = FMath::Max(DamageAmount, 0.0f);
	if (UDamageIndicatorComponent* DamageIndicator =
		Character->GetDamageIndicatorComponent())
	{
		DamageIndicator->ShowDamageIndicator(
			DisplayDamageAmount,
			WorldLocation,
			bCriticalHit);
	}

	if (DisplayDamageAmount > 0.0f && Character->IsLocallyControlled())
	{
		if (APlayerController* PlayerController =
			Cast<APlayerController>(Character->GetController()))
		{
			if (APdHUD* PdHUD = PlayerController->GetHUD<APdHUD>())
			{
				PdHUD->ShowDamageScreenEffect(DisplayDamageAmount);
			}
		}
	}
	Character->OnDamageTaken(
		DisplayDamageAmount,
		bCriticalHit,
		WorldLocation);
}

void UCharacterDeathComponent::ConfigureWeaponDamageMesh(
	USkeletalMeshComponent* CharacterMesh) const
{
	if (!CharacterMesh)
	{
		return;
	}

	CharacterMesh->SetCollisionObjectType(LabCollisionChannels::HitableBody());
	CharacterMesh->SetCollisionResponseToChannel(LabCollisionChannels::Projectile(), ECR_Block);
	if (CharacterMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

ACharacterBase* UCharacterDeathComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

const ACharacterBase* UCharacterDeathComponent::GetCharacterOwnerConst() const
{
	return Cast<ACharacterBase>(GetOwner());
}
