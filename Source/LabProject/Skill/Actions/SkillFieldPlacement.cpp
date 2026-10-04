#include "Skill/Actions/SkillFieldPlacement.h"

#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/AbilitySystem/SkillActorFieldSettings.h"
#include "Skill/SkillGroundProjection.h"

namespace
{
	USkeletalMeshComponent* ResolveSocketMesh(const AActor& Avatar, const FName SocketName)
	{
		if (SocketName.IsNone())
		{
			return nullptr;
		}

		const ACharacterBase* Character = Cast<ACharacterBase>(&Avatar);
		USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
		return CharacterMesh && CharacterMesh->DoesSocketExist(SocketName) ? CharacterMesh : nullptr;
	}
}

TArray<FName> PdSkillFieldPlacement::GetSpawnSocketNames(const FSkillActorFieldSettings& Settings)
{
	TArray<FName> SocketNames;
	if (Settings.bUseSpawnSockets)
	{
		for (const FName& SocketName : Settings.SpawnSocketNames)
		{
			if (!SocketName.IsNone())
			{
				SocketNames.Add(SocketName);
			}
		}
	}

	if (SocketNames.IsEmpty())
	{
		SocketNames.Add(NAME_None);
	}

	return SocketNames;
}

FTransform PdSkillFieldPlacement::ResolveSpawnTransform(AActor& Avatar, const FSkillActorFieldSettings& Settings,
	const FName SocketName)
{
	FTransform BaseTransform = Avatar.GetActorTransform();
	if (Settings.bUseSpawnSockets)
	{
		if (const USkeletalMeshComponent* SocketMesh = ResolveSocketMesh(Avatar, SocketName))
		{
			BaseTransform = SocketMesh->GetSocketTransform(SocketName, RTS_World);
		}
	}
	else
	{
		BaseTransform.SetLocation(PdSkillGroundProjection::ResolveActorFeetLocation(&Avatar));
	}

	// 소켓 기준이면 오프셋도 소켓 방향을 따라 돌린다.
	const FVector SpawnOffset = Settings.bUseSpawnSockets
		? BaseTransform.GetRotation().RotateVector(Settings.SpawnLocationOffset)
		: Settings.SpawnLocationOffset;
	FVector SpawnLocation = BaseTransform.GetLocation() + SpawnOffset;
	const FRotator SpawnRotation = BaseTransform.Rotator() + Settings.SpawnRotationOffset;

	if (Settings.bProjectSpawnToGround)
	{
		const FVector TraceBaseLocation =
			Settings.bUseSpawnSockets ? BaseTransform.GetLocation() : Avatar.GetActorLocation();

		TArray<AActor*> ActorsToIgnore;
		PdSkillGroundProjection::AddIgnoredActorAndAttachments(ActorsToIgnore, &Avatar);

		PdSkillGroundProjection::FGroundProjectionResult GroundProjection;
		if (PdSkillGroundProjection::TryProjectToGround(Avatar.GetWorld(), TraceBaseLocation,
			Settings.GroundTraceChannel, Settings.GroundTraceStartHeight, Settings.GroundTraceDepth, ActorsToIgnore,
			GroundProjection))
		{
			SpawnLocation = GroundProjection.Location + SpawnOffset;
		}
	}

	return FTransform(SpawnRotation, SpawnLocation, BaseTransform.GetScale3D());
}

bool PdSkillFieldPlacement::AttachToSpawnSocket(AActor& FieldActor, const AActor& Avatar,
	const FSkillActorFieldSettings& Settings, const FName SocketName)
{
	if (!Settings.bUseSpawnSockets || !Settings.bAttachSpawnedActorToSocket)
	{
		return false;
	}

	USkeletalMeshComponent* SocketMesh = ResolveSocketMesh(Avatar, SocketName);
	if (!SocketMesh)
	{
		return false;
	}

	FieldActor.AttachToComponent(SocketMesh, FAttachmentTransformRules::KeepWorldTransform, SocketName);
	return true;
}
