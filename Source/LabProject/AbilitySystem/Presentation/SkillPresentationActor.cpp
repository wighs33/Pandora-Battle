#include "AbilitySystem/Presentation/SkillPresentationActor.h"

#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillPresentationActor)

namespace
{
	bool HasPresentationFlag(const uint8 Flags, const ESkillPresentationFlags Flag)
	{
		return (Flags & static_cast<uint8>(Flag)) != 0;
	}

	FName NormalizeNiagaraUserParameterName(const FName RawParameterName)
	{
		if (RawParameterName.IsNone())
		{
			return NAME_None;
		}

		const FString RawName = RawParameterName.ToString();
		return RawName.StartsWith(TEXT("User."))
			? RawParameterName
			: FName(*FString::Printf(TEXT("User.%s"), *RawName));
	}

	UNiagaraComponent* FindNiagaraComponentByNameOrTag(const AActor* Owner, const FName ComponentName)
	{
		if (!Owner || ComponentName.IsNone())
		{
			return nullptr;
		}

		TArray<UNiagaraComponent*> NiagaraComponents;
		Owner->GetComponents<UNiagaraComponent>(NiagaraComponents);
		for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
		{
			if (NiagaraComponent
				&& (NiagaraComponent->GetFName() == ComponentName
					|| NiagaraComponent->ComponentTags.Contains(ComponentName)))
			{
				return NiagaraComponent;
			}
		}

		return nullptr;
	}
}

ASkillPresentationActor::ASkillPresentationActor()
{
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(10.0f);
	PrimaryActorTick.bCanEverTick = false;
}

void ASkillPresentationActor::InitializePresentation(
	ACharacterBase* InSourceCharacter,
	USkillDefinition* InSkillDefinition,
	const ESkillPresentationFlags InFlags,
	const FVector& InMissileTargetLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceCharacter = InSourceCharacter;
	SkillDefinition = InSkillDefinition;
	PresentationFlags = static_cast<uint8>(InFlags);
	MissileTargetLocation = InMissileTargetLocation;
}

void ASkillPresentationActor::SetPresentationEnabled(
	const ESkillPresentationFlags Flag,
	const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	const uint8 FlagValue = static_cast<uint8>(Flag);
	const uint8 NewFlags = bEnabled
		? PresentationFlags | FlagValue
		: PresentationFlags & ~FlagValue;
	if (NewFlags == PresentationFlags)
	{
		return;
	}

	PresentationFlags = NewFlags;
	RefreshLocalPresentation();
	ForceNetUpdate();
}

void ASkillPresentationActor::SetMissileTargetLocation(const FVector& InTargetLocation)
{
	if (!HasAuthority() || FVector(MissileTargetLocation).Equals(InTargetLocation, 1.0f))
	{
		return;
	}

	MissileTargetLocation = InTargetLocation;
	ApplyMissileTargetLocation();
	ForceNetUpdate();
}

bool ASkillPresentationActor::HasAnyPresentation() const
{
	return PresentationFlags != static_cast<uint8>(ESkillPresentationFlags::None);
}

void ASkillPresentationActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && IsValid(SourceCharacter))
	{
		SourceCharacter->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSourceDestroyed);
	}

	RefreshLocalPresentation();
}

void ASkillPresentationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && IsValid(SourceCharacter))
	{
		SourceCharacter->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSourceDestroyed);
	}

	CleanupLocalPresentation();
	Super::EndPlay(EndPlayReason);
}

void ASkillPresentationActor::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, SourceCharacter);
	DOREPLIFETIME(ThisClass, SkillDefinition);
	DOREPLIFETIME(ThisClass, PresentationFlags);
	DOREPLIFETIME(ThisClass, MissileTargetLocation);
}

void ASkillPresentationActor::OnRep_PresentationState()
{
	RefreshLocalPresentation();
}

void ASkillPresentationActor::HandleSourceDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority() && DestroyedActor == SourceCharacter)
	{
		Destroy();
	}
}

void ASkillPresentationActor::RefreshLocalPresentation()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ACharacterBase* ResolvedSourceCharacter = ResolveSourceCharacter();
	if (!ResolvedSourceCharacter || !SkillDefinition)
	{
		return;
	}

	if (LocalPresentationSourceCharacter.Get() != ResolvedSourceCharacter)
	{
		CleanupLocalPresentation();
		LocalPresentationSourceCharacter = ResolvedSourceCharacter;
	}

	const bool bWantsDefaultFX =
		HasPresentationFlag(PresentationFlags, ESkillPresentationFlags::DefaultFX);
	if (bWantsDefaultFX && !bDefaultAuraApplied
		&& !LocalDefaultSocketNiagaraComponent
		&& !LocalBorrowedSocketNiagaraComponent)
	{
		StartDefaultFX();
	}
	else if (!bWantsDefaultFX
		&& (bDefaultAuraApplied
			|| LocalDefaultSocketNiagaraComponent
			|| LocalBorrowedSocketNiagaraComponent))
	{
		StopDefaultFX();
	}

	const bool bWantsOverlay =
		HasPresentationFlag(PresentationFlags, ESkillPresentationFlags::CharacterOverlay);
	if (bWantsOverlay && !bCharacterOverlayApplied)
	{
		StartCharacterOverlay();
	}
	else if (!bWantsOverlay && bCharacterOverlayApplied)
	{
		StopCharacterOverlay();
	}

	const bool bWantsMissile =
		HasPresentationFlag(PresentationFlags, ESkillPresentationFlags::Missile);
	if (bWantsMissile && !LocalMissileNiagaraComponent)
	{
		StartMissile();
	}
	else if (!bWantsMissile && LocalMissileNiagaraComponent)
	{
		StopMissile();
	}

	if (bWantsMissile)
	{
		ApplyMissileTargetLocation();
	}
}

void ASkillPresentationActor::CleanupLocalPresentation()
{
	StopMissile();
	StopCharacterOverlay();
	StopDefaultFX();
	LocalPresentationSourceCharacter.Reset();
}

void ASkillPresentationActor::StartDefaultFX()
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (!Character || !SkillDefinition)
	{
		return;
	}

	const FSkillNiagaraSettings& NiagaraSettings = SkillDefinition->Niagara;
	if (NiagaraSettings.AuraNiagaraSystem)
	{
		Character->ApplyBodyAuraNiagaraWithOffset(
			NiagaraSettings.AuraNiagaraComponentName,
			NiagaraSettings.AuraNiagaraSystem.Get(),
			true,
			true,
			NiagaraSettings.AuraLocationOffset,
			NiagaraSettings.AuraScale);
		bDefaultAuraApplied = true;
	}

	if (!NiagaraSettings.SocketNiagaraSystem)
	{
		return;
	}

	if (UNiagaraComponent* ExistingComponent =
		FindNiagaraComponentByNameOrTag(Character, NiagaraSettings.SocketNiagaraComponentName))
	{
		LocalBorrowedSocketNiagaraComponent = ExistingComponent;
		BorrowedSocketPreviousAsset = ExistingComponent->GetAsset();
		BorrowedSocketPreviousScale = ExistingComponent->GetRelativeScale3D();
		bBorrowedSocketWasActive = ExistingComponent->IsActive();
		ExistingComponent->SetAsset(NiagaraSettings.SocketNiagaraSystem.Get());
		ExistingComponent->SetRelativeScale3D(NiagaraSettings.SocketScale);
		ExistingComponent->ResetSystem();
		ExistingComponent->Activate(true);
		return;
	}

	if (NiagaraSettings.bSpawnSocketNiagaraAtCharacterLocation)
	{
		const FTransform CharacterTransform = Character->GetActorTransform();
		const FVector SpawnLocation =
			ResolveCharacterFloorLocation(Character)
			+ CharacterTransform.TransformVector(NiagaraSettings.SocketLocationOffset);
		const FRotator SpawnRotation =
			(Character->GetActorRotation() + NiagaraSettings.SocketRotationOffset).GetNormalized();

		LocalDefaultSocketNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Character,
			NiagaraSettings.SocketNiagaraSystem.Get(),
			SpawnLocation,
			SpawnRotation,
			NiagaraSettings.SocketScale,
			false,
			true);
		return;
	}

	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	if (!MeshComponent
		|| NiagaraSettings.SocketName.IsNone()
		|| !MeshComponent->DoesSocketExist(NiagaraSettings.SocketName))
	{
		return;
	}

	LocalDefaultSocketNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSettings.SocketNiagaraSystem.Get(),
		MeshComponent,
		NiagaraSettings.SocketName,
		NiagaraSettings.SocketLocationOffset,
		NiagaraSettings.SocketRotationOffset,
		EAttachLocation::KeepRelativeOffset,
		false,
		true);
	if (LocalDefaultSocketNiagaraComponent)
	{
		LocalDefaultSocketNiagaraComponent->SetRelativeScale3D(NiagaraSettings.SocketScale);
	}
}

void ASkillPresentationActor::StopDefaultFX()
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (bDefaultAuraApplied && Character && SkillDefinition)
	{
		Character->ClearBodyAuraNiagaraIfMatching(
			SkillDefinition->Niagara.AuraNiagaraComponentName,
			SkillDefinition->Niagara.AuraNiagaraSystem.Get());
	}
	bDefaultAuraApplied = false;

	if (LocalDefaultSocketNiagaraComponent)
	{
		LocalDefaultSocketNiagaraComponent->Deactivate();
		LocalDefaultSocketNiagaraComponent->DestroyComponent();
		LocalDefaultSocketNiagaraComponent = nullptr;
	}

	if (LocalBorrowedSocketNiagaraComponent)
	{
		UNiagaraComponent* BorrowedComponent = LocalBorrowedSocketNiagaraComponent.Get();
		const UNiagaraSystem* AppliedSystem = SkillDefinition
			? SkillDefinition->Niagara.SocketNiagaraSystem.Get()
			: nullptr;
		if (!AppliedSystem || BorrowedComponent->GetAsset() == AppliedSystem)
		{
			BorrowedComponent->Deactivate();
			BorrowedComponent->SetAsset(BorrowedSocketPreviousAsset);
			BorrowedComponent->SetRelativeScale3D(BorrowedSocketPreviousScale);
			if (BorrowedSocketPreviousAsset && bBorrowedSocketWasActive)
			{
				BorrowedComponent->Activate(true);
			}
		}
	}

	LocalBorrowedSocketNiagaraComponent = nullptr;
	BorrowedSocketPreviousAsset = nullptr;
	BorrowedSocketPreviousScale = FVector::OneVector;
	bBorrowedSocketWasActive = false;
}

void ASkillPresentationActor::StartCharacterOverlay()
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (!Character
		|| !SkillDefinition
		|| !SkillDefinition->Overlay.bUseCharacterOverlay
		|| !SkillDefinition->Overlay.CharacterOverlayMaterial)
	{
		return;
	}

	Character->ApplySkillPresentationOverlay(
		this,
		SkillDefinition->Overlay.CharacterOverlayMaterial.Get());
	bCharacterOverlayApplied = true;
}

void ASkillPresentationActor::StopCharacterOverlay()
{
	if (bCharacterOverlayApplied)
	{
		if (ACharacterBase* Character = ResolveSourceCharacter())
		{
			Character->ClearSkillPresentationOverlay(this);
		}
	}
	bCharacterOverlayApplied = false;
}

void ASkillPresentationActor::StartMissile()
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (!Character || !SkillDefinition || !SkillDefinition->Missile.MissileSystem)
	{
		return;
	}

	const FMissileSkillConfig& MissileConfig = SkillDefinition->Missile;
	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	if (MeshComponent
		&& !MissileConfig.NiagaraSpawnSocketName.IsNone()
		&& MeshComponent->DoesSocketExist(MissileConfig.NiagaraSpawnSocketName))
	{
		LocalMissileNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			MissileConfig.MissileSystem,
			MeshComponent,
			MissileConfig.NiagaraSpawnSocketName,
			MissileConfig.NiagaraSpawnLocationOffset,
			MissileConfig.NiagaraSpawnRotationOffset,
			MissileConfig.NiagaraScale,
			EAttachLocation::KeepRelativeOffset,
			false,
			ENCPoolMethod::None,
			false,
			false);
	}
	else
	{
		const FVector SpawnLocation =
			Character->GetActorLocation()
			+ Character->GetActorForwardVector() * MissileConfig.NiagaraSpawnLocationOffset.X
			+ Character->GetActorRightVector() * MissileConfig.NiagaraSpawnLocationOffset.Y
			+ Character->GetActorUpVector() * MissileConfig.NiagaraSpawnLocationOffset.Z;
		const FRotator SpawnRotation =
			Character->GetActorRotation() + MissileConfig.NiagaraSpawnRotationOffset;
		LocalMissileNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Character,
			MissileConfig.MissileSystem,
			SpawnLocation,
			SpawnRotation,
			MissileConfig.NiagaraScale,
			false,
			false,
			ENCPoolMethod::None,
			false);
	}

	if (LocalMissileNiagaraComponent)
	{
		ApplyMissileTargetLocation();
		LocalMissileNiagaraComponent->Activate(true);
	}
}

void ASkillPresentationActor::StopMissile()
{
	if (LocalMissileNiagaraComponent)
	{
		LocalMissileNiagaraComponent->Deactivate();
		LocalMissileNiagaraComponent->DestroyComponent();
		LocalMissileNiagaraComponent = nullptr;
	}
}

void ASkillPresentationActor::ApplyMissileTargetLocation()
{
	if (!LocalMissileNiagaraComponent || !SkillDefinition)
	{
		return;
	}

	const FName ParameterName =
		NormalizeNiagaraUserParameterName(SkillDefinition->Missile.AimPositionParameterName);
	if (ParameterName.IsNone())
	{
		return;
	}

	const FVector TargetLocation = MissileTargetLocation;
	LocalMissileNiagaraComponent->SetVariablePosition(ParameterName, TargetLocation);
	LocalMissileNiagaraComponent->SetVariableVec3(ParameterName, TargetLocation);
}

ACharacterBase* ASkillPresentationActor::ResolveSourceCharacter() const
{
	if (IsValid(SourceCharacter))
	{
		return SourceCharacter;
	}

	return Cast<ACharacterBase>(GetOwner());
}

FVector ASkillPresentationActor::ResolveCharacterFloorLocation(
	const ACharacterBase* Character) const
{
	if (!Character)
	{
		return FVector::ZeroVector;
	}

	FVector FloorLocation = Character->GetActorLocation();
	if (const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent())
	{
		FloorLocation.Z -= CapsuleComponent->GetScaledCapsuleHalfHeight();
	}

	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	if (MovementComponent
		&& MovementComponent->CurrentFloor.IsWalkableFloor())
	{
		FloorLocation.Z = MovementComponent->CurrentFloor.HitResult.ImpactPoint.Z;
	}

	return FloorLocation;
}
