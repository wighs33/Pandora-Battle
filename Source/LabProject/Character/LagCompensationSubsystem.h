#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "LagCompensationSubsystem.generated.h"

class ACharacterBase;
class AController;
class APawn;
class USkeletalMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogPdLagCompensation, Log, All);

/** 한 서버 시각에 기록한 캐릭터 판정 메시의 월드 위치와 회전. */
struct FPdHitboxSnapshot
{
	double ServerTime = 0.0;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
};

/** 캐릭터별 최근 판정 위치 기록. 오래된 기록부터 최신 기록 순서로 보관한다. */
struct FPdCharacterHitHistory
{
	TWeakObjectPtr<ACharacterBase> Character;
	TArray<FPdHitboxSnapshot> Snapshots;
};

/** 클라이언트가 보고한 시각을 서버가 허용 범위로 보정한 결과. */
struct FPdRewindRequest
{
	/** 되감아 판정할 서버 시각. 음수이면 현재 월드로 판정한다. */
	double RewindServerTime = -1.0;
	double RewindMs = 0.0;
	/** 클라이언트 시각이 허용 범위를 벗어나 잘렸는지 여부. */
	bool bClamped = false;

	bool IsRewinding() const { return RewindServerTime >= 0.0; }
};

/** 지연 보상 전후 판정 비교 통계. pd.LagComp.Stats가 켜져 있을 때만 누적한다. */
struct FPdLagCompensationStats
{
	int32 ServerShots = 0;
	int32 RewoundShots = 0;
	int32 ClampedShots = 0;
	int32 CurrentCharacterHits = 0;
	int32 RewoundCharacterHits = 0;
	int32 RewoundOnlyHits = 0;
	int32 CurrentOnlyHits = 0;
	double TotalRewindMs = 0.0;
	double MaxRewindMs = 0.0;
	int32 ClientShots = 0;
	int32 ClientPerceivedHits = 0;
};

namespace PdLagCompensation
{
	/** 서버 되감기 판정 사용 여부(pd.LagComp.Enabled). */
	LABPROJECT_API bool IsEnabled();

	/** 되감기 전후 판정 비교 통계 수집 여부(pd.LagComp.Stats). */
	LABPROJECT_API bool IsStatsEnabled();

	/** 되감기 결과를 서버와 사격한 클라이언트에 그릴지 여부(pd.LagComp.DebugDraw). */
	LABPROJECT_API bool IsDebugDrawEnabled();

	/**
	 * 소유 클라이언트가 공격하는 순간 화면에 보고 있던 서버 시각을 반환한다.
	 * 서버·Listen Server 호스트이거나 시간 동기화 전이면 음수를 반환해 현재 월드로 판정하게 한다.
	 */
	LABPROJECT_API double GetClientViewServerTime(const APawn* LocalPawn);
}

/**
 * 서버에서 캐릭터 판정 메시의 최근 위치를 틱마다 기록하고, 과거 시각 기준의 판정 trace를 제공한다.
 *
 * 되감기는 액터를 실제로 옮기지 않는다. 대상의 과거 위치 기준으로 쏜 사선을 현재 메시 좌표계로 옮겨
 * 해당 메시에만 trace하고, 결과를 다시 과거 위치 기준으로 되돌린다. 강체 변환이므로 액터를 되감았다가
 * 복원하는 방식과 같은 결과이면서 overlap·물리·부착 컴포넌트에 부작용이 없다.
 * 애니메이션 포즈는 되감지 않고 현재 포즈를 사용한다.
 *
 * 판정 규칙(primary mesh만 피해 대상)은 PdCharacterHitValidation을 그대로 따른다.
 */
UCLASS()
class LABPROJECT_API ULagCompensationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	static ULagCompensationSubsystem* Get(const UObject* WorldContextObject);

	void RegisterCharacter(ACharacterBase* Character);
	void UnregisterCharacter(const ACharacterBase* Character);

	/** 클라이언트가 보낸 시각을 최대 되감기 한도와 측정된 ping 범위로 제한한다. */
	FPdRewindRequest ResolveRewindRequest(const AController* ShooterController, double ClientViewServerTime) const;

	/** 월드는 현재 상태로, 기록된 캐릭터 메시는 지정 시각 위치로 line trace한다. */
	bool LineTraceSingleAtTime(
		double RewindServerTime,
		const FVector& Start,
		const FVector& End,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		EDrawDebugTrace::Type DebugDrawType,
		FHitResult& OutHit) const;

	/** 월드는 현재 상태로, 기록된 캐릭터 메시는 지정 시각 위치로 sphere trace하고 거리순으로 반환한다. */
	bool SphereTraceMultiAtTime(
		double RewindServerTime,
		const FVector& Start,
		const FVector& End,
		float Radius,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		EDrawDebugTrace::Type DebugDrawType,
		TArray<FHitResult>& OutHits) const;

	/** 지정 시각의 캐릭터 이동 캡슐 중심을 계산한다. 디버그 표시용이다. */
	bool GetCapsuleCenterAtTime(const ACharacterBase* Character, double ServerTime, FVector& OutCenter) const;

	void RecordServerShot(
		const AController* ShooterController,
		const FPdRewindRequest& RewindRequest,
		const ACharacterBase* CurrentHitCharacter,
		const ACharacterBase* RewoundHitCharacter);
	void RecordClientShot(bool bPerceivedCharacterHit);
	const FPdLagCompensationStats& GetStats() const { return Stats; }
	void ResetStats();
	FString DescribeStats() const;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool ShouldRecordHistory() const;
	void RecordSnapshots();
	const FPdCharacterHitHistory* FindHistory(const ACharacterBase* Character) const;
	bool FindSnapshotAtTime(const FPdCharacterHitHistory& History, double ServerTime, FPdHitboxSnapshot& OutSnapshot) const;
	void AppendRewoundCharacters(TArray<AActor*>& InOutActorsToIgnore) const;
	void TraceRewoundCharacters(
		double RewindServerTime,
		const FVector& Start,
		const FVector& End,
		float Radius,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		TArray<FHitResult>& OutHits) const;

private:
	TArray<FPdCharacterHitHistory> Histories;
	FPdLagCompensationStats Stats;
};
