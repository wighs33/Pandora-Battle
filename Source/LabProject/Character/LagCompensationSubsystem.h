#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "LagCompensationSubsystem.generated.h"

class ACharacterBase;
class AController;

/**
 * 원격 플레이어의 총격을 그 플레이어 화면에 보이던 위치로 판정하는 서버 지연 보상.
 *
 * - 서버는 캐릭터 판정 메시의 위치를 틱마다 기록한다.
 * - 사수 화면의 상대는 서버보다 편도 지연만큼 늦고, 사격 요청은 다시 편도 지연만큼 늦게 도착한다.
 *   그래서 요청이 도착한 시각에서 왕복 지연(서버가 잰 핑)만큼 되감으면 사수가 보던 위치가 된다.
 * - 되감는 폭은 서버가 잰 값이라 클라이언트가 바꿀 수 없다. 핑이 아주 큰 사수 때문에 이미 엄폐한 상대가
 *   오래전 위치로 맞지 않도록 pd.LagComp.MaxRewindMs로 자른다(0이면 끈다).
 *
 * 되감기는 액터를 옮기지 않는다. 사선을 대상 메시의 지금 위치 기준으로 옮겨 그 메시에만 판정하고, 결과를 과거 위치
 * 기준으로 되돌린다. 강체 변환이라 액터를 옮겼다 되돌리는 것과 결과가 같고, 물리·겹침·부착 컴포넌트에 부작용이 없다.
 * 애니메이션 포즈는 되감지 않는다.
 */
UCLASS()
class LABPROJECT_API ULagCompensationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	static ULagCompensationSubsystem* Get(const UObject* WorldContextObject);

	/** 개발 빌드에서 pd.LagComp.DebugDraw가 켜져 있으면 되감아 맞힌 위치를 화면에 그린다. */
	static bool IsDebugDrawEnabled();

	void RegisterCharacter(ACharacterBase* Character);
	void UnregisterCharacter(const ACharacterBase* Character);

	/** Shooter의 사격을 판정할 서버 시각. 되감을 필요가 없으면(서버·호스트 자신의 사격, AI, 핑을 아직 모름) 비어 있다. */
	TOptional<double> GetRewindTime(const AController* Shooter) const;

	/**
	 * 월드는 지금 상태로, 기록된 캐릭터는 RewindTime의 위치로 판정해 가까운 순서로 돌려준다.
	 * Radius가 0이면 선으로 판정한다.
	 */
	void TraceAtTime(
		double RewindTime,
		const FVector& Start,
		const FVector& End,
		float Radius,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		EDrawDebugTrace::Type DebugDrawType,
		TArray<FHitResult>& OutHits) const;

	/** 디버그 표시용. Character의 이동 캡슐이 Time에 있던 중심. */
	TOptional<FVector> GetCapsuleCenterAtTime(const ACharacterBase& Character, double Time) const;

private:
	/** 한 서버 시각의 판정 메시 위치와 회전. */
	struct FSnapshot
	{
		double Time = 0.0;
		FTransform MeshTransform;
	};

	/** 캐릭터 하나의 최근 기록. 오래된 것부터 시간 순서로 둔다. */
	struct FHistory
	{
		TWeakObjectPtr<ACharacterBase> Character;
		TArray<FSnapshot> Snapshots;
	};

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool IsServerWorld() const;
	void RecordSnapshots();
	const FHistory* FindHistory(const ACharacterBase* Character) const;
	TOptional<FTransform> FindMeshTransformAtTime(const FHistory& History, double Time) const;
	void TraceRewoundCharacters(
		double RewindTime,
		const FVector& Start,
		const FVector& End,
		float Radius,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		TArray<FHitResult>& OutHits) const;

	TArray<FHistory> Histories;
};
