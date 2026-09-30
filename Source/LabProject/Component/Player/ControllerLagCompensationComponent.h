#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "ControllerLagCompensationComponent.generated.h"

class APlayerController;
class UWorld;

/**
 * 소유 클라이언트의 서버 시간 추정과 지연 보상 디버그 표시를 담당한다.
 *
 * 클라이언트는 주기적으로 서버 시간을 요청해 왕복 지연(RTT)과 시계 차이를 측정한다. 여러 표본 중 RTT가 가장
 * 짧은 표본을 사용해 큐잉 지연의 영향을 줄인다. 공격 시에는 "다른 캐릭터가 화면에 보이는 서버 시각"을 함께
 * 보내고, 실제 되감기 허용 범위는 서버의 ULagCompensationSubsystem이 결정한다.
 */
UCLASS(ClassGroup = (PlayerController))
class LABPROJECT_API UControllerLagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UControllerLagCompensationComponent();

	/** 현재 서버 월드 시간의 추정치. 서버에서는 자신의 월드 시간이다. 동기화 전 클라이언트는 음수. */
	double GetEstimatedServerTime() const;

	/** 다른 캐릭터가 화면에 보이는 서버 시각(추정 서버 시간 - 편도 지연). 동기화 전이면 음수. */
	double GetViewServerTime() const;

	/** 마지막으로 선택한 표본의 왕복 지연(초). */
	double GetRoundTripSeconds() const { return RoundTripSeconds; }

	/** 서버가 판정한 현재·되감기 캡슐을 사격한 플레이어의 화면에 표시한다. */
	void ShowRewindDebug(
		const FVector& CurrentCenter,
		const FVector& RewoundCenter,
		float CapsuleHalfHeight,
		float CapsuleRadius,
		double RewindMs);

protected:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Unreliable)
	void ServerRequestServerTime(double ClientRequestTime);

	UFUNCTION(Client, Unreliable)
	void ClientReportServerTime(double ClientRequestTime, double ServerReceiveTime);

	UFUNCTION(Client, Unreliable)
	void ClientDrawRewindDebug(
		FVector_NetQuantize CurrentCenter,
		FVector_NetQuantize RewoundCenter,
		float CapsuleHalfHeight,
		float CapsuleRadius,
		float RewindMs);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	APlayerController* GetOwningController() const;
	bool ShouldSyncServerClock() const;
	bool HasValidClockSample() const;
	void RequestServerTime();
	void AddClockSample(double InRoundTripSeconds, double InServerTimeOffset);
	void DrawRewindDebug(
		const FVector& CurrentCenter,
		const FVector& RewoundCenter,
		float CapsuleHalfHeight,
		float CapsuleRadius,
		double RewindMs) const;

private:
	struct FClockSample
	{
		double RoundTripSeconds = 0.0;
		double ServerTimeOffset = 0.0;
	};

	/** 최근 표본. RTT가 가장 짧은 표본의 시계 차이를 사용한다. */
	TArray<FClockSample> ClockSamples;

	/** 표본을 측정한 월드. 맵 이동으로 월드 시간이 초기화되면 표본을 버린다. */
	TWeakObjectPtr<UWorld> ClockSampleWorld;

	double ServerTimeOffset = 0.0;
	double RoundTripSeconds = 0.0;
	int32 ClockRequestCount = 0;
};
