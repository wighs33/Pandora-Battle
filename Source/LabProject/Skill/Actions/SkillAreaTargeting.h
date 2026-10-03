#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class UWorld;
struct FHitResult;
struct FSkillAreaSettings;

/**
 * 범위 스킬이 떨어질 지면 위치를 정한다.
 * 캐릭터·맵 경계 볼륨·가파른 면은 지면으로 보지 않고, 지면을 못 찾으면 대상의 발밑이나 조준 지점을 그대로 쓴다.
 */
namespace PdSkillAreaTargeting
{
	/** SourceLocation 위아래로 선을 그어 처음 만나는 지면. */
	LABPROJECT_API bool TryResolveGroundLocation(
		UWorld* World,
		const FVector& SourceLocation,
		const TArray<AActor*>& ActorsToIgnore,
		TEnumAsByte<ETraceTypeQuery> TraceType,
		float TraceDepth,
		FVector& OutGroundLocation);

	/** 공격 대상 아래 지면. 못 찾으면 대상의 발밑. */
	LABPROJECT_API FVector ResolveTargetLocation(AActor& Target, AActor* Avatar, const FSkillAreaSettings& Settings);

	/** 대상이 없을 때 시전자 앞쪽 사거리 안의 지면. 못 찾으면 그 지점 그대로. */
	LABPROJECT_API FVector ResolveForwardLocation(AActor& Avatar, const FSkillAreaSettings& Settings);

	/** 조준 결과의 지면 위치. 캐릭터를 조준했으면 그 캐릭터 아래 지면을 쓴다. */
	LABPROJECT_API FVector ResolveAimedLocation(
		const FHitResult& HitResult,
		const FVector& TargetDataEndPoint,
		AActor* Avatar,
		const FSkillAreaSettings& Settings);
}
