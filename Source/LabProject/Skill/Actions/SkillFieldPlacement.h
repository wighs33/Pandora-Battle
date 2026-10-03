#pragma once

#include "CoreMinimal.h"

class AActor;
struct FSkillActorFieldSettings;

/** 배치 액터를 어디에 어떤 방향으로 놓을지 정한다. 소켓을 쓰면 시전자 메시 소켓 기준, 아니면 시전자 발밑 기준이다. */
namespace PdSkillFieldPlacement
{
	/** 배치할 소켓 이름. 소켓을 쓰지 않거나 이름이 모두 비어 있으면 시전자 위치 하나(NAME_None)만 돌려준다. */
	LABPROJECT_API TArray<FName> GetSpawnSocketNames(const FSkillActorFieldSettings& Settings);

	/** 기준 위치에 설정 오프셋을 더하고, 설정이 있으면 지면에 붙인 배치 위치. */
	LABPROJECT_API FTransform ResolveSpawnTransform(AActor& Avatar, const FSkillActorFieldSettings& Settings, FName SocketName);

	/** 설정이 소켓 부착을 요구하면 시전자 메시 소켓에 붙인다. */
	LABPROJECT_API bool AttachToSpawnSocket(
		AActor& FieldActor,
		const AActor& Avatar,
		const FSkillActorFieldSettings& Settings,
		FName SocketName);
}
