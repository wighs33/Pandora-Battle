#include "AI/Monster/BossRaidBenchmarkSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AI/Monster/MonsterAIController.h"
#include "AI/Monster/MonsterCharacter.h"
#include "Character/CharacterBase.h"
#include "Common/PdGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "RenderTimer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BossRaidBenchmarkSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogBossRaidBenchmark, Log, All);

namespace
{
	/** 보스가 플레이어를 알아채고 싸움이 자리 잡을 때까지 기다리는 시간. 이 구간은 재지 않는다. */
	constexpr double WarmupSeconds = 10.0;
	constexpr double BeforeSummonSeconds = 5.0;
	/** 소환 직후 구간. 스폰, 첫 복제, AI 시작이 모두 이 안에 들어간다. */
	constexpr double SummonSeconds = 5.0;
	constexpr double AfterSummonSeconds = 20.0;
	/** 졸개 일제 사망 구간. 사망 처리와 시체가 사라지는(DeathDestroyDelay) 프레임까지 들어가게 잡는다. */
	constexpr double MassDeathSeconds = 10.0;
	constexpr double StartTimeoutSeconds = 180.0;

	/** 보스 캡슐 가장자리에서 플레이어를 세우는 거리. 보스 공격 범위 안이고, 졸개가 둘러싸는 자리다. */
	constexpr float PlayerDistanceFromBoss = 500.0f;

	/** 클라이언트는 몬스터가 이만큼 늘면 소환이 시작된 것으로 본다. 몬스터 수는 이 간격으로만 센다. */
	constexpr int32 ClientSummonDetectCount = 20;
	constexpr double ClientMonsterCountInterval = 0.5;
	/** 클라이언트는 서버보다 소환을 늦게 알아채므로 교전 구간을 짧게 잡아, 서버가 끝나 접속이 끊기기 전에 보고한다. */
	constexpr double ClientAfterSummonSeconds = 15.0;

	const TCHAR* GetStageName(const int32 StageIndex)
	{
		static const TCHAR* const Names[] = { TEXT("WaitingForPlayer"), TEXT("Warmup"), TEXT("BeforeSummon"), TEXT("Summon"),
			TEXT("AfterSummon"), TEXT("MassDeath"), TEXT("Done") };
		return Names[StageIndex];
	}

	double GetPercentile(TArray<double> Values, const double Ratio)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::CeilToInt(Ratio * Values.Num()) - 1, 0, Values.Num() - 1)];
	}
}

bool UBossRaidBenchmarkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("BossRaidBenchmark")) && Super::ShouldCreateSubsystem(Outer);
#endif
}

void UBossRaidBenchmarkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickStartHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &ThisClass::HandleWorldTickStart);
	StageStartTime = FPlatformTime::Seconds();
	TRACE_BEGIN_REGION(GetStageName(static_cast<int32>(Stage)));
}

void UBossRaidBenchmarkSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldTickStart.Remove(TickStartHandle);
	Super::Deinitialize();
}

bool UBossRaidBenchmarkSubsystem::IsClient() const
{
	return GetWorld()->GetNetMode() == NM_Client;
}

void UBossRaidBenchmarkSubsystem::HandleWorldTickStart(UWorld* TickedWorld, ELevelTick TickType, float DeltaSeconds)
{
	const ENetMode NetMode = TickedWorld->GetNetMode();
	if (TickedWorld != GetWorld() || Stage == EStage::Done || (NetMode != NM_Client && NetMode != NM_DedicatedServer))
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (LastTickStartTime > 0.0)
	{
		StageSamples.Add(MeasureFrame(Now));
	}
	LastTickStartTime = Now;
	if (NetMode == NM_Client)
	{
		AdvanceClientStage(Now, Now - StageStartTime);
	}
	else
	{
		AdvanceServerStage(Now - StageStartTime);
	}
}

// 서버: 직전 월드 틱 시작부터 지금까지의 간격에서 최대 틱 속도를 맞추려고 쉰 시간을 빼면, 게임 스레드가 한 프레임 동안 실제로 일한 시간이다.
// 월드 틱 밖의 가비지 컬렉션과 액터 정리도 들어간다.
// 클라이언트: 프레임 제한 없이 돌리므로 간격이 곧 플레이어가 보는 프레임 시간이다. 병목을 가리려고 stat unit과 같은 스레드별 시간도 남긴다.
UBossRaidBenchmarkSubsystem::FFrameSample UBossRaidBenchmarkSubsystem::MeasureFrame(const double Now) const
{
	FFrameSample Sample;
	Sample.Time = Now;
	if (IsClient())
	{
		Sample.FrameMs = (Now - LastTickStartTime) * 1000.0;
		Sample.GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
		Sample.RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
		Sample.GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	}
	else
	{
		Sample.FrameMs = (Now - LastTickStartTime - FApp::GetIdleTime()) * 1000.0;
	}
	return Sample;
}

void UBossRaidBenchmarkSubsystem::AdvanceServerStage(const double StageSeconds)
{
	switch (Stage)
	{
	case EStage::WaitingForPlayer:
		if (TryPlacePlayerBeforeBoss())
		{
			EnterStage(EStage::Warmup);
		}
		else if (StageSeconds > StartTimeoutSeconds)
		{
			Finish(TEXT("no player or boss"));
		}
		break;
	case EStage::Warmup:
		if (StageSeconds >= WarmupSeconds)
		{
			EnterStage(EStage::BeforeSummon);
		}
		break;
	case EStage::BeforeSummon:
		if (StageSeconds >= BeforeSummonSeconds)
		{
			ReportStage(TEXT("BeforeSummon"));
			EnterStage(EStage::Summon);
			// 이번 프레임에 소환하므로 그 비용은 다음 프레임 시작에 잰 시간으로 들어간다.
			AMonsterCharacter* BossCharacter = Boss.Get();
			if (!BossCharacter || !BossCharacter->TriggerPhaseSummon())
			{
				Finish(TEXT("boss could not summon"));
			}
		}
		break;
	case EStage::Summon:
		if (StageSeconds >= SummonSeconds)
		{
			ReportStage(TEXT("Summon"));
			EnterStage(EStage::AfterSummon);
		}
		break;
	case EStage::AfterSummon:
		if (StageSeconds >= AfterSummonSeconds)
		{
			ReportStage(TEXT("AfterSummon"));
			EnterStage(EStage::MassDeath);
			KillMinions();
		}
		break;
	case EStage::MassDeath:
		if (StageSeconds >= MassDeathSeconds)
		{
			ReportStage(TEXT("MassDeath"));
			Finish(TEXT("complete"));
		}
		break;
	default:
		break;
	}
}

// 범위 공격으로 졸개 수백 마리가 같은 프레임에 죽는 경우를 만든다. 실제 피해와 같은 피해 효과를 적용해 사망 능력, 사망 복제, 래그돌, 시체 제거가 모두 돈다.
void UBossRaidBenchmarkSubsystem::KillMinions()
{
	for (TActorIterator<AMonsterCharacter> It(GetWorld()); It; ++It)
	{
		AMonsterCharacter* Monster = *It;
		UAbilitySystemComponent* AbilitySystem = Monster->GetAbilitySystemComponent();
		const UEnemyBaseDefinition* Definition = Monster->GetMonsterDefinition();
		const TSubclassOf<UGameplayEffect> DamageEffect = Definition ? Definition->GetMonsterPresentationSettings().ContactDamageEffectClass : nullptr;
		if (Monster == Boss.Get() || !AbilitySystem || !DamageEffect)
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(DamageEffect, 1.0f, AbilitySystem->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(PdGameplayTags::Data_Damage, 1.0e6f);
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

// 클라이언트는 서버의 측정 순서를 모르므로, 기다리는 동안 최근 몇 초만 남겨 두다가 몬스터가 크게 늘면 그 기록을 소환 전 구간으로 보고한다.
void UBossRaidBenchmarkSubsystem::AdvanceClientStage(const double Now, const double StageSeconds)
{
	switch (Stage)
	{
	case EStage::WaitingForPlayer:
	{
		StageSamples.RemoveAll([Now](const FFrameSample& Sample) { return Sample.Time < Now - BeforeSummonSeconds; });
		if (Now < NextMonsterCountTime)
		{
			break;
		}
		NextMonsterCountTime = Now + ClientMonsterCountInterval;
		const int32 MonsterCount = CountMonsters();
		const APlayerController* LocalPlayer = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (ClientMonstersBeforeSummon == INDEX_NONE && MonsterCount > 0 && LocalPlayer && LocalPlayer->GetPawn())
		{
			ClientMonstersBeforeSummon = MonsterCount;
		}
		if (ClientMonstersBeforeSummon != INDEX_NONE && MonsterCount >= ClientMonstersBeforeSummon + ClientSummonDetectCount)
		{
			ReportStage(TEXT("ClientBeforeSummon"));
			EnterStage(EStage::Summon);
		}
		else if (StageSeconds > StartTimeoutSeconds)
		{
			Finish(TEXT("no summon"));
		}
		break;
	}
	case EStage::Summon:
		if (StageSeconds >= SummonSeconds)
		{
			ReportStage(TEXT("ClientSummon"));
			EnterStage(EStage::AfterSummon);
		}
		break;
	case EStage::AfterSummon:
		if (StageSeconds >= ClientAfterSummonSeconds)
		{
			ReportStage(TEXT("ClientAfterSummon"));
			Finish(TEXT("complete"));
		}
		break;
	default:
		break;
	}
}

// 보스가 준비되고 플레이어 캐릭터가 생기면, 플레이어를 보스 정면에 보스를 바라보게 세우고 체력이 깎이지 않게 한다.
// 피해 판정은 그대로 돌아서 졸개가 계속 쫓고 때리는 가장 무거운 상황이 유지된다.
bool UBossRaidBenchmarkSubsystem::TryPlacePlayerBeforeBoss()
{
	UWorld* World = GetWorld();
	if (!Boss.IsValid())
	{
		for (TActorIterator<AMonsterCharacter> It(World); It; ++It)
		{
			if (It->HasPendingPhaseSummon())
			{
				Boss = *It;
				break;
			}
		}
	}
	const AMonsterCharacter* BossCharacter = Boss.Get();
	if (!BossCharacter || !BossCharacter->IsMonsterReadyForAI())
	{
		return false;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
		UAbilitySystemComponent* AbilitySystem = Player ? Player->GetAbilitySystemComponent() : nullptr;
		if (!AbilitySystem)
		{
			continue;
		}
		AbilitySystem->AddLooseGameplayTag(PdGameplayTags::State_DefenseField_Invulnerable);
		const FVector Forward = BossCharacter->GetActorForwardVector().GetSafeNormal2D();
		FVector Location = BossCharacter->GetActorLocation()
			+ Forward * (BossCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() + PlayerDistanceFromBoss);
		FNavLocation NavLocation;
		const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(Location, NavLocation, FVector(300.0f, 300.0f, 1000.0f)))
		{
			Location = NavLocation.Location + FVector(0.0f, 0.0f, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		}
		const FRotator FacingBoss = (-Forward).Rotation();
		Player->TeleportTo(Location, FacingBoss);
		PlayerController->ClientSetRotation(FacingBoss, true);
		UE_LOG(LogBossRaidBenchmark, Display, TEXT("BENCH placed %s in front of %s."), *Player->GetName(), *BossCharacter->GetName());
		return true;
	}
	return false;
}

void UBossRaidBenchmarkSubsystem::EnterStage(const EStage NewStage)
{
	TRACE_END_REGION(GetStageName(static_cast<int32>(Stage)));
	Stage = NewStage;
	StageStartTime = FPlatformTime::Seconds();
	StageSamples.Reset();
	const UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	StageStartOutBytes = NetDriver ? NetDriver->OutTotalBytes : 0;
	TRACE_BEGIN_REGION(GetStageName(static_cast<int32>(Stage)));
}

int32 UBossRaidBenchmarkSubsystem::CountMonsters(int32* OutTargetingCount) const
{
	int32 MonsterCount = 0;
	int32 TargetingCount = 0;
	for (TActorIterator<AMonsterCharacter> It(GetWorld()); It; ++It)
	{
		++MonsterCount;
		const AMonsterAIController* MonsterController = Cast<AMonsterAIController>(It->GetController());
		TargetingCount += MonsterController && MonsterController->GetPerceivedPlayerPawn() ? 1 : 0;
	}
	if (OutTargetingCount)
	{
		*OutTargetingCount = TargetingCount;
	}
	return MonsterCount;
}

// 서버의 프레임 예산은 최대 틱 속도로 정한다. 예산을 넘은 시간의 합은 그만큼 서버 시뮬레이션이 밀렸다는 뜻이다.
// 최적화가 AI를 망가뜨리지 않았는지 보려고, 플레이어를 표적으로 잡은 몬스터 수도 남긴다.
void UBossRaidBenchmarkSubsystem::ReportStage(const TCHAR* Label) const
{
	TArray<double> FrameMs;
	double TotalMs = 0.0;
	double MaxMs = 0.0;
	double GameThreadMs = 0.0;
	double RenderThreadMs = 0.0;
	double GpuMs = 0.0;
	for (const FFrameSample& Sample : StageSamples)
	{
		FrameMs.Add(Sample.FrameMs);
		TotalMs += Sample.FrameMs;
		MaxMs = FMath::Max(MaxMs, Sample.FrameMs);
		GameThreadMs += Sample.GameThreadMs;
		RenderThreadMs += Sample.RenderThreadMs;
		GpuMs += Sample.GpuMs;
	}
	const int32 Frames = FMath::Max(StageSamples.Num(), 1);
	int32 TargetingCount = 0;
	const int32 MonsterCount = CountMonsters(&TargetingCount);
	if (IsClient())
	{
		UE_LOG(LogBossRaidBenchmark, Display,
			TEXT("BENCH %s frames=%d avg_ms=%.2f p95_ms=%.2f max_ms=%.2f game_ms=%.2f render_ms=%.2f gpu_ms=%.2f monsters=%d"),
			Label, StageSamples.Num(), TotalMs / Frames, GetPercentile(FrameMs, 0.95), MaxMs, GameThreadMs / Frames, RenderThreadMs / Frames,
			GpuMs / Frames, MonsterCount);
		return;
	}
	const float MaxTickRate = GEngine->GetMaxTickRate(0.0f, false);
	const double BudgetMs = MaxTickRate > 0.0f ? 1000.0 / MaxTickRate : 1000.0 / 30.0;
	double OverBudgetMs = 0.0;
	int32 OverBudgetFrames = 0;
	for (const double Ms : FrameMs)
	{
		if (Ms > BudgetMs)
		{
			++OverBudgetFrames;
			OverBudgetMs += Ms - BudgetMs;
		}
	}
	const double Seconds = FMath::Max(FPlatformTime::Seconds() - StageStartTime, UE_SMALL_NUMBER);
	const UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	const double OutKBps = NetDriver ? static_cast<uint32>(NetDriver->OutTotalBytes - StageStartOutBytes) / 1024.0 / Seconds : 0.0;
	UE_LOG(LogBossRaidBenchmark, Display,
		TEXT("BENCH %s frames=%d avg_ms=%.2f p95_ms=%.2f max_ms=%.2f over_budget_frames=%d over_budget_ms=%.1f out_kbps=%.1f monsters=%d targeting=%d"),
		Label, StageSamples.Num(), TotalMs / Frames, GetPercentile(FrameMs, 0.95), MaxMs, OverBudgetFrames, OverBudgetMs, OutKBps,
		MonsterCount, TargetingCount);
}

// 클라이언트는 서버 측정이 끝날 때까지 접속해 있어야 하므로 스스로 끝나지 않는다. 스크립트가 서버 종료 뒤 닫는다.
void UBossRaidBenchmarkSubsystem::Finish(const TCHAR* Reason)
{
	TRACE_END_REGION(GetStageName(static_cast<int32>(Stage)));
	Stage = EStage::Done;
	UE_LOG(LogBossRaidBenchmark, Display, TEXT("BENCH done (%s)."), Reason);
	if (!IsClient())
	{
		FPlatformMisc::RequestExit(false, TEXT("BossRaidBenchmark"));
	}
}
