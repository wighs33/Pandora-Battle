#include "Component/Player/ControllerLagCompensationComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerLagCompensationComponent)

namespace
{
	constexpr int32 MaxClockSamples = 8;
	constexpr int32 FastClockSyncRequestCount = 8;
	constexpr float FastClockSyncIntervalSeconds = 0.25f;
	constexpr float SteadyClockSyncIntervalSeconds = 2.0f;
	constexpr double MaxAcceptedRoundTripSeconds = 2.0;
	constexpr float RewindDebugDrawSeconds = 3.0f;
}

UControllerLagCompensationComponent::UControllerLagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = FastClockSyncIntervalSeconds;
	SetIsReplicatedByDefault(true);
}

// 원격 클라이언트의 로컬 컨트롤러만 서버 시간을 측정한다. 서버와 Listen Server 호스트는 자신의 시간이 기준이다.
void UControllerLagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	const bool bShouldSync = ShouldSyncServerClock();
	SetComponentTickEnabled(bShouldSync);
	if (bShouldSync)
	{
		RequestServerTime();
	}
}

// 처음에는 짧은 간격으로 표본을 모으고, 이후에는 긴 간격으로 지연 변화를 따라간다.
void UControllerLagCompensationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ShouldSyncServerClock())
	{
		SetComponentTickEnabled(false);
		return;
	}

	RequestServerTime();
}

double UControllerLagCompensationComponent::GetEstimatedServerTime() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = GetOwningController();
	if (!World || !Controller)
	{
		return -1.0;
	}

	if (Controller->HasAuthority())
	{
		return World->GetTimeSeconds();
	}

	return HasValidClockSample() ? World->GetTimeSeconds() + ServerTimeOffset : -1.0;
}

double UControllerLagCompensationComponent::GetViewServerTime() const
{
	const APlayerController* Controller = GetOwningController();
	if (Controller && Controller->HasAuthority())
	{
		return GetEstimatedServerTime();
	}

	if (!HasValidClockSample())
	{
		return -1.0;
	}

	// 서버 상태는 편도 지연만큼 늦게 도착하므로, 화면의 다른 캐릭터는 그만큼 과거의 서버 시각에 있다.
	return GetEstimatedServerTime() - RoundTripSeconds * 0.5;
}

void UControllerLagCompensationComponent::ShowRewindDebug(
	const FVector& CurrentCenter,
	const FVector& RewoundCenter,
	const float CapsuleHalfHeight,
	const float CapsuleRadius,
	const double RewindMs)
{
	APlayerController* Controller = GetOwningController();
	if (!Controller)
	{
		return;
	}

	if (Controller->IsLocalController())
	{
		DrawRewindDebug(CurrentCenter, RewoundCenter, CapsuleHalfHeight, CapsuleRadius, RewindMs);
		return;
	}

	ClientDrawRewindDebug(CurrentCenter, RewoundCenter, CapsuleHalfHeight, CapsuleRadius, static_cast<float>(RewindMs));
}

// Network RPCs --------------------------------------------------------------------------------------------------------

void UControllerLagCompensationComponent::ServerRequestServerTime_Implementation(const double ClientRequestTime)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ClientReportServerTime(ClientRequestTime, World->GetTimeSeconds());
}

void UControllerLagCompensationComponent::ClientReportServerTime_Implementation(
	const double ClientRequestTime,
	const double ServerReceiveTime)
{
	const UWorld* World = GetWorld();
	if (!World || ClockSampleWorld.Get() != World)
	{
		return;
	}

	const double ClientReceiveTime = World->GetTimeSeconds();
	const double MeasuredRoundTripSeconds = ClientReceiveTime - ClientRequestTime;
	if (MeasuredRoundTripSeconds < 0.0 || MeasuredRoundTripSeconds > MaxAcceptedRoundTripSeconds)
	{
		return;
	}

	// 서버가 요청을 받은 시각은 왕복의 중간이라고 보고 현재 서버 시간을 추정한다.
	const double EstimatedServerTimeNow = ServerReceiveTime + MeasuredRoundTripSeconds * 0.5;
	AddClockSample(MeasuredRoundTripSeconds, EstimatedServerTimeNow - ClientReceiveTime);
}

void UControllerLagCompensationComponent::ClientDrawRewindDebug_Implementation(
	const FVector_NetQuantize CurrentCenter,
	const FVector_NetQuantize RewoundCenter,
	const float CapsuleHalfHeight,
	const float CapsuleRadius,
	const float RewindMs)
{
	DrawRewindDebug(CurrentCenter, RewoundCenter, CapsuleHalfHeight, CapsuleRadius, RewindMs);
}

// Internal Helpers ----------------------------------------------------------------------------------------------------

APlayerController* UControllerLagCompensationComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool UControllerLagCompensationComponent::ShouldSyncServerClock() const
{
	const APlayerController* Controller = GetOwningController();
	return Controller && Controller->IsLocalController() && !Controller->HasAuthority();
}

bool UControllerLagCompensationComponent::HasValidClockSample() const
{
	return !ClockSamples.IsEmpty() && ClockSampleWorld.Get() == GetWorld();
}

void UControllerLagCompensationComponent::RequestServerTime()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 맵 이동 후에는 월드 시간이 새로 시작하므로 이전 표본을 버리고 빠른 측정부터 다시 한다.
	if (ClockSampleWorld.Get() != World)
	{
		ClockSamples.Reset();
		ClockSampleWorld = World;
		ClockRequestCount = 0;
		SetComponentTickInterval(FastClockSyncIntervalSeconds);
	}

	++ClockRequestCount;
	if (ClockRequestCount == FastClockSyncRequestCount)
	{
		SetComponentTickInterval(SteadyClockSyncIntervalSeconds);
	}

	ServerRequestServerTime(World->GetTimeSeconds());
}

// 시계 차이는 RTT가 가장 짧은 표본을, 편도 지연은 최근 표본의 평균 RTT를 사용한다.
void UControllerLagCompensationComponent::AddClockSample(
	const double InRoundTripSeconds,
	const double InServerTimeOffset)
{
	FClockSample& Sample = ClockSamples.AddDefaulted_GetRef();
	Sample.RoundTripSeconds = InRoundTripSeconds;
	Sample.ServerTimeOffset = InServerTimeOffset;
	if (ClockSamples.Num() > MaxClockSamples)
	{
		ClockSamples.RemoveAt(0);
	}

	const FClockSample* BestSample = nullptr;
	double TotalRoundTripSeconds = 0.0;
	for (const FClockSample& ClockSample : ClockSamples)
	{
		TotalRoundTripSeconds += ClockSample.RoundTripSeconds;
		if (!BestSample || ClockSample.RoundTripSeconds < BestSample->RoundTripSeconds)
		{
			BestSample = &ClockSample;
		}
	}

	ServerTimeOffset = BestSample->ServerTimeOffset;
	RoundTripSeconds = TotalRoundTripSeconds / ClockSamples.Num();
}

void UControllerLagCompensationComponent::DrawRewindDebug(
	const FVector& CurrentCenter,
	const FVector& RewoundCenter,
	const float CapsuleHalfHeight,
	const float CapsuleRadius,
	const double RewindMs) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugCapsule(World, CurrentCenter, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Red,
		false, RewindDebugDrawSeconds, 0, 1.5f);
	DrawDebugCapsule(World, RewoundCenter, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity, FColor::Green,
		false, RewindDebugDrawSeconds, 0, 1.5f);
	DrawDebugString(World, RewoundCenter + FVector(0.0, 0.0, CapsuleHalfHeight + 20.0),
		FString::Printf(TEXT("rewind %.0f ms"), RewindMs), nullptr, FColor::Green, RewindDebugDrawSeconds);
#else
	static_cast<void>(CurrentCenter);
	static_cast<void>(RewoundCenter);
	static_cast<void>(CapsuleHalfHeight);
	static_cast<void>(CapsuleRadius);
	static_cast<void>(RewindMs);
#endif
}
