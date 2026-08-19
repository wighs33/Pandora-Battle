#include "AbilitySystem/Presentation/SkillPresentationActor.h"

#include "AbilitySystem/SkillGroundProjection.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
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
	constexpr double DefaultFXGroundTraceStartHeight = 150.0;
	constexpr double DefaultFXGroundTraceDepth = 5000.0;

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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.05f;
}

void ASkillPresentationActor::InitializePresentation(
	ACharacterBase* InSourceCharacter,
	USkillDefinition* InSkillDefinition,
	const ESkillPresentationFlags InFlags)
{
	if (!HasAuthority())
	{
		return;
	}

	SourceCharacter = InSourceCharacter;
	SkillDefinition = InSkillDefinition;
	PresentationFlags = static_cast<uint8>(InFlags);
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

void ASkillPresentationActor::SetMissileTargetActors(
	const TArray<AActor*>& InTargetActors)
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<TObjectPtr<AActor>> NewTargetActors;
	NewTargetActors.Reserve(InTargetActors.Num());
	for (AActor* TargetActor : InTargetActors)
	{
		if (IsValid(TargetActor))
		{
			NewTargetActors.AddUnique(TargetActor);
		}
	}

	if (MissileTargetActors == NewTargetActors)
	{
		return;
	}

	MissileTargetActors = MoveTemp(NewTargetActors);
	RefreshLocalPresentation();
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

void ASkillPresentationActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLocalMissileTargets();
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
	DOREPLIFETIME(ThisClass, MissileTargetActors);
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

	const bool bWantsGroundFX =
		HasPresentationFlag(PresentationFlags, ESkillPresentationFlags::GroundFX);
	if (bWantsGroundFX && !LocalDefaultGroundNiagaraComponent)
	{
		StartGroundFX();
	}
	else if (!bWantsGroundFX && LocalDefaultGroundNiagaraComponent)
	{
		StopGroundFX();
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
	if (bWantsMissile)
	{
		RefreshLocalMissiles();
	}
	else if (!LocalMissileNiagaraComponents.IsEmpty())
	{
		StopMissiles();
	}
}

void ASkillPresentationActor::CleanupLocalPresentation()
{
	StopMissiles();
	StopCharacterOverlay();
	StopGroundFX();
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

void ASkillPresentationActor::StartGroundFX()
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (!Character || !SkillDefinition
		|| !SkillDefinition->Niagara.GroundNiagaraSystem)
	{
		return;
	}

	const FSkillNiagaraSettings& NiagaraSettings = SkillDefinition->Niagara;
	if (NiagaraSettings.bGroundNiagaraFollowsCharacter)
	{
		USkeletalMeshComponent* MeshComponent = Character->GetMesh();
		if (MeshComponent
			&& !NiagaraSettings.GroundFollowSocketName.IsNone()
			&& MeshComponent->DoesSocketExist(NiagaraSettings.GroundFollowSocketName))
		{
			LocalDefaultGroundNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NiagaraSettings.GroundNiagaraSystem.Get(),
				MeshComponent,
				NiagaraSettings.GroundFollowSocketName,
				NiagaraSettings.GroundLocationOffset,
				NiagaraSettings.GroundRotationOffset,
				EAttachLocation::KeepRelativeOffset,
				false,
				true);
			if (LocalDefaultGroundNiagaraComponent)
			{
				LocalDefaultGroundNiagaraComponent->SetRelativeScale3D(
					NiagaraSettings.GroundScale);
			}
			return;
		}
	}

	const FTransform CharacterTransform = Character->GetActorTransform();
	const FVector SpawnLocation =
		ResolveCharacterFloorLocation(Character)
		+ CharacterTransform.TransformVector(NiagaraSettings.GroundLocationOffset);
	const FRotator SpawnRotation =
		(Character->GetActorRotation() + NiagaraSettings.GroundRotationOffset).GetNormalized();

	LocalDefaultGroundNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		Character,
		NiagaraSettings.GroundNiagaraSystem.Get(),
		SpawnLocation,
		SpawnRotation,
		NiagaraSettings.GroundScale,
		false,
		true);
}

void ASkillPresentationActor::StopGroundFX()
{
	if (!LocalDefaultGroundNiagaraComponent)
	{
		return;
	}

	LocalDefaultGroundNiagaraComponent->Deactivate();
	LocalDefaultGroundNiagaraComponent->DestroyComponent();
	LocalDefaultGroundNiagaraComponent = nullptr;
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

void ASkillPresentationActor::RefreshLocalMissiles()
{
	for (int32 Index = LocalMissileTargetActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* LocalTargetActor = LocalMissileTargetActors[Index];
		if (!IsValid(LocalTargetActor) || !MissileTargetActors.Contains(LocalTargetActor))
		{
			StopMissileAtIndex(Index);
		}
	}

	for (AActor* TargetActor : MissileTargetActors)
	{
		if (!IsValid(TargetActor) || LocalMissileTargetActors.Contains(TargetActor))
		{
			continue;
		}

		if (UNiagaraComponent* MissileComponent = StartMissileForTarget(TargetActor))
		{
			LocalMissileTargetActors.Add(TargetActor);
			LocalMissileNiagaraComponents.Add(MissileComponent);
		}
	}

	SetActorTickEnabled(!LocalMissileNiagaraComponents.IsEmpty());
	UpdateLocalMissileTargets();
}

UNiagaraComponent* ASkillPresentationActor::StartMissileForTarget(AActor* TargetActor)
{
	ACharacterBase* Character = ResolveSourceCharacter();
	if (!Character
		|| !IsValid(TargetActor)
		|| !SkillDefinition
		|| !SkillDefinition->Missile.MissileSystem)
	{
		return nullptr;
	}

	const FMissileSkillConfig& MissileConfig = SkillDefinition->Missile;
	UNiagaraComponent* MissileComponent = nullptr;
	USkeletalMeshComponent* MeshComponent = Character->GetMesh();
	if (MeshComponent
		&& !MissileConfig.NiagaraSpawnSocketName.IsNone()
		&& MeshComponent->DoesSocketExist(MissileConfig.NiagaraSpawnSocketName))
	{
		MissileComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
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
		MissileComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
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

	if (MissileComponent)
	{
		ApplyMissileTargetLocation(TargetActor, MissileComponent);
		MissileComponent->Activate(true);
	}

	return MissileComponent;
}

void ASkillPresentationActor::StopMissileAtIndex(const int32 Index)
{
	if (!LocalMissileNiagaraComponents.IsValidIndex(Index)
		|| !LocalMissileTargetActors.IsValidIndex(Index))
	{
		return;
	}

	if (UNiagaraComponent* MissileComponent = LocalMissileNiagaraComponents[Index])
	{
		MissileComponent->Deactivate();
		MissileComponent->DestroyComponent();
	}

	LocalMissileTargetActors.RemoveAt(Index);
	LocalMissileNiagaraComponents.RemoveAt(Index);
}

void ASkillPresentationActor::StopMissiles()
{
	for (int32 Index = LocalMissileNiagaraComponents.Num() - 1; Index >= 0; --Index)
	{
		StopMissileAtIndex(Index);
	}

	LocalMissileTargetActors.Reset();
	LocalMissileNiagaraComponents.Reset();
	SetActorTickEnabled(false);
}

void ASkillPresentationActor::UpdateLocalMissileTargets()
{
	for (int32 Index = LocalMissileTargetActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* TargetActor = LocalMissileTargetActors[Index];
		UNiagaraComponent* MissileComponent =
			LocalMissileNiagaraComponents.IsValidIndex(Index)
				? LocalMissileNiagaraComponents[Index]
				: nullptr;
		if (!IsValid(TargetActor) || !IsValid(MissileComponent))
		{
			StopMissileAtIndex(Index);
			continue;
		}

		ApplyMissileTargetLocation(TargetActor, MissileComponent);
	}

	if (LocalMissileNiagaraComponents.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}

void ASkillPresentationActor::ApplyMissileTargetLocation(
	AActor* TargetActor,
	UNiagaraComponent* MissileComponent) const
{
	if (!IsValid(TargetActor) || !MissileComponent || !SkillDefinition)
	{
		return;
	}

	const FName ParameterName = NormalizeNiagaraUserParameterName(
		SkillDefinition->Missile.AimPositionParameterName);
	FVector TargetLocation = FVector::ZeroVector;
	if (ParameterName.IsNone()
		|| !ResolveMissileTargetLocation(TargetActor, TargetLocation))
	{
		return;
	}

	MissileComponent->SetVariablePosition(ParameterName, TargetLocation);
	MissileComponent->SetVariableVec3(ParameterName, TargetLocation);
}

bool ASkillPresentationActor::ResolveMissileTargetLocation(
	const AActor* TargetActor,
	FVector& OutTargetLocation) const
{
	if (!IsValid(TargetActor) || !SkillDefinition)
	{
		return false;
	}

	const FName TargetSocketName = SkillDefinition->Missile.TargetSocketName;
	if (!TargetSocketName.IsNone())
	{
		if (const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
			TargetCharacter
			&& TargetCharacter->GetMesh()
			&& TargetCharacter->GetMesh()->DoesSocketExist(TargetSocketName))
		{
			OutTargetLocation = TargetCharacter->GetMesh()->GetSocketLocation(TargetSocketName);
			return true;
		}

		if (const USkeletalMeshComponent* TargetMesh =
			Cast<USkeletalMeshComponent>(TargetActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
			TargetMesh && TargetMesh->DoesSocketExist(TargetSocketName))
		{
			OutTargetLocation = TargetMesh->GetSocketLocation(TargetSocketName);
			return true;
		}
	}

	OutTargetLocation = TargetActor->GetActorLocation();
	return true;
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

	TArray<AActor*> ActorsToIgnore;
	PdSkillGroundProjection::AddIgnoredActorAndAttachments(
		ActorsToIgnore,
		const_cast<ACharacterBase*>(Character));
	PdSkillGroundProjection::FGroundProjectionResult GroundProjection;
	if (PdSkillGroundProjection::TryProjectToGround(
		Character->GetWorld(),
		Character->GetActorLocation(),
		LabCollisionChannels::VisibilityTrace(),
		DefaultFXGroundTraceStartHeight,
		DefaultFXGroundTraceDepth,
		ActorsToIgnore,
		GroundProjection))
	{
		return GroundProjection.Location;
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
