#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BossRaidBenchmarkSubsystem.generated.h"

class AMonsterCharacter;

/**
 * 보스 레이드 대규모 소환의 부하를 잰다. 개발 빌드를 -BossRaidBenchmark로 띄울 때만 만들어진다.
 *
 * 전용 서버: 첫 플레이어를 보스 앞에 무적으로 세운 뒤 소환 전 → 소환 직후 → 소환 뒤 교전 → 졸개 일제 사망 구간마다
 * 게임 스레드가 한 프레임에 일한 시간과 서버 송신량을 로그에 남기고 서버를 끝낸다.
 * 클라이언트: 몬스터 수가 갑자기 늘면 소환으로 보고 같은 구간의 프레임 시간(게임·렌더 스레드, GPU)을 남긴다.
 * 서버와 클라이언트 실행, 결과 수집은 Scripts/RunPerfBenchmark.ps1이 맡는다.
 */
UCLASS()
class LABPROJECT_API UBossRaidBenchmarkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	enum class EStage : uint8
	{
		WaitingForPlayer,
		Warmup,
		BeforeSummon,
		Summon,
		AfterSummon,
		MassDeath,
		Done
	};

	/** 한 프레임의 측정값. 서버는 게임 스레드가 일한 시간만, 클라이언트는 프레임 간격과 스레드별 시간을 채운다. */
	struct FFrameSample
	{
		double Time = 0.0;
		double FrameMs = 0.0;
		double GameThreadMs = 0.0;
		double RenderThreadMs = 0.0;
		double GpuMs = 0.0;
	};

	void HandleWorldTickStart(UWorld* TickedWorld, ELevelTick TickType, float DeltaSeconds);
	FFrameSample MeasureFrame(double Now) const;
	void AdvanceServerStage(double StageSeconds);
	void AdvanceClientStage(double Now, double StageSeconds);
	bool TryPlacePlayerBeforeBoss();
	void KillMinions();
	void EnterStage(EStage NewStage);
	void ReportStage(const TCHAR* Label) const;
	void Finish(const TCHAR* Reason);
	int32 CountMonsters(int32* OutTargetingCount = nullptr) const;
	bool IsClient() const;

	FDelegateHandle TickStartHandle;
	TWeakObjectPtr<AMonsterCharacter> Boss;
	EStage Stage = EStage::WaitingForPlayer;
	double StageStartTime = 0.0;
	double LastTickStartTime = 0.0;
	uint32 StageStartOutBytes = 0;
	TArray<FFrameSample> StageSamples;

	/** 클라이언트가 소환 전에 본 몬스터 수. 이보다 크게 늘면 소환이 시작된 것으로 본다. */
	int32 ClientMonstersBeforeSummon = INDEX_NONE;
	double NextMonsterCountTime = 0.0;
};
