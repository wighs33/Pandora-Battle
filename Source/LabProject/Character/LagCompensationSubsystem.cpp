#include "Character/LagCompensationSubsystem.h"

#include "Character/CharacterBase.h"
#include "Component/Player/ControllerLagCompensationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LagCompensationSubsystem)

DEFINE_LOG_CATEGORY(LogPdLagCompensation);

namespace
{
	TAutoConsoleVariable<int32> CVarLagCompensationEnabled(
		TEXT("pd.LagComp.Enabled"),
		1,
		TEXT("Rewinds character hitboxes to the time the shooting client saw.\n")
		TEXT("0: judge against the current server state, 1: server rewind (default)"),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarLagCompensationMaxRewindMs(
		TEXT("pd.LagComp.MaxRewindMs"),
		200.0f,
		TEXT("Upper bound of how far the server rewinds a shot, in milliseconds."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarLagCompensationPingToleranceMs(
		TEXT("pd.LagComp.PingToleranceMs"),
		50.0f,
		TEXT("Extra rewind allowed on top of the shooter's measured round-trip ping, in milliseconds.\n")
		TEXT("Limits clients that report an older time than their real latency."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarLagCompensationHistoryMs(
		TEXT("pd.LagComp.HistoryMs"),
		1000.0f,
		TEXT("How long the server keeps hitbox history, in milliseconds."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarLagCompensationTeleportDistance(
		TEXT("pd.LagComp.TeleportDistance"),
		300.0f,
		TEXT("Two consecutive samples farther apart than this are not interpolated (respawn, portal)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarLagCompensationStats(
		TEXT("pd.LagComp.Stats"),
		0,
		TEXT("Collects per-shot current vs rewound hit comparison and logs every shot.\n")
		TEXT("Print with pd.LagComp.PrintStats, clear with pd.LagComp.ResetStats."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarLagCompensationDebugDraw(
		TEXT("pd.LagComp.DebugDraw"),
		0,
		TEXT("Draws the current (red) and rewound (green) capsule of a hit character on the server\n")
		TEXT("and on the shooting client."),
		ECVF_Cheat);
#endif

	void ForEachLagCompensationSubsystem(TFunctionRef<void(ULagCompensationSubsystem&, const UWorld&)> Callback)
	{
		if (!GEngine)
		{
			return;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			const UWorld* World = WorldContext.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}

			if (ULagCompensationSubsystem* Subsystem = World->GetSubsystem<ULagCompensationSubsystem>())
			{
				Callback(*Subsystem, *World);
			}
		}
	}

	const TCHAR* GetNetModeLabel(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Standalone");
		}
	}

	FAutoConsoleCommand PrintLagCompensationStatsCommand(
		TEXT("pd.LagComp.PrintStats"),
		TEXT("Prints lag compensation hit comparison stats for every game world in this process."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachLagCompensationSubsystem([](ULagCompensationSubsystem& Subsystem, const UWorld& World)
			{
				UE_LOG(LogPdLagCompensation, Display, TEXT("[%s] %s"),
					GetNetModeLabel(World.GetNetMode()),
					*Subsystem.DescribeStats());
			});
		}));

	FAutoConsoleCommand ResetLagCompensationStatsCommand(
		TEXT("pd.LagComp.ResetStats"),
		TEXT("Clears lag compensation hit comparison stats for every game world in this process."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			ForEachLagCompensationSubsystem([](ULagCompensationSubsystem& Subsystem, const UWorld&)
			{
				Subsystem.ResetStats();
			});
		}));

	// 현재 메시 기준 좌표를 과거 메시 기준 좌표로 옮긴다. 두 변환 모두 scale 없는 강체 변환이다.
	FVector MapCurrentToRewound(const FTransform& CurrentTransform, const FTransform& RewoundTransform, const FVector& Position)
	{
		return RewoundTransform.TransformPosition(CurrentTransform.InverseTransformPosition(Position));
	}

	FVector MapRewoundToCurrent(const FTransform& CurrentTransform, const FTransform& RewoundTransform, const FVector& Position)
	{
		return CurrentTransform.TransformPosition(RewoundTransform.InverseTransformPosition(Position));
	}

	FVector MapCurrentToRewoundDirection(const FTransform& CurrentTransform, const FTransform& RewoundTransform, const FVector& Direction)
	{
		return RewoundTransform.TransformVectorNoScale(CurrentTransform.InverseTransformVectorNoScale(Direction));
	}
}

bool PdLagCompensation::IsEnabled()
{
	return CVarLagCompensationEnabled.GetValueOnGameThread() != 0;
}

bool PdLagCompensation::IsStatsEnabled()
{
	return CVarLagCompensationStats.GetValueOnGameThread() != 0;
}

bool PdLagCompensation::IsDebugDrawEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarLagCompensationDebugDraw.GetValueOnGameThread() != 0;
#endif
}

double PdLagCompensation::GetClientViewServerTime(const APawn* LocalPawn)
{
	const APlayerController* PlayerController = LocalPawn ? Cast<APlayerController>(LocalPawn->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController() || PlayerController->HasAuthority())
	{
		return -1.0;
	}

	const UControllerLagCompensationComponent* LagCompensationComponent =
		PlayerController->FindComponentByClass<UControllerLagCompensationComponent>();
	return LagCompensationComponent ? LagCompensationComponent->GetViewServerTime() : -1.0;
}

// Engine Overrides ----------------------------------------------------------------------------------------------------

void ULagCompensationSubsystem::Deinitialize()
{
	Histories.Reset();
	Super::Deinitialize();
}

// 모든 액터의 이동이 끝난 뒤 틱마다 서버의 캐릭터 판정 위치를 기록한다.
void ULagCompensationSubsystem::Tick(float DeltaTime)
{
	static_cast<void>(DeltaTime);
	RecordSnapshots();
}

TStatId ULagCompensationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULagCompensationSubsystem, STATGROUP_Tickables);
}

bool ULagCompensationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool ULagCompensationSubsystem::IsTickable() const
{
	return !Histories.IsEmpty() && ShouldRecordHistory();
}

// Public API ----------------------------------------------------------------------------------------------------------

ULagCompensationSubsystem* ULagCompensationSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULagCompensationSubsystem>() : nullptr;
}

// 서버 권한 캐릭터를 기록 대상으로 등록한다. 기록은 서버 net mode에서만 진행한다.
void ULagCompensationSubsystem::RegisterCharacter(ACharacterBase* Character)
{
	if (!IsValid(Character) || !Character->HasAuthority() || FindHistory(Character))
	{
		return;
	}

	FPdCharacterHitHistory& History = Histories.AddDefaulted_GetRef();
	History.Character = Character;
}

void ULagCompensationSubsystem::UnregisterCharacter(const ACharacterBase* Character)
{
	Histories.RemoveAllSwap([Character](const FPdCharacterHitHistory& History)
	{
		return !History.Character.IsValid() || History.Character.Get() == Character;
	});
}

FPdRewindRequest ULagCompensationSubsystem::ResolveRewindRequest(
	const AController* ShooterController,
	const double ClientViewServerTime) const
{
	FPdRewindRequest Request;
	const UWorld* World = GetWorld();
	if (!World || !PdLagCompensation::IsEnabled() || !ShouldRecordHistory() || ClientViewServerTime < 0.0)
	{
		return Request;
	}

	const double ServerTime = World->GetTimeSeconds();
	double MaxRewindSeconds = FMath::Max(0.0f, CVarLagCompensationMaxRewindMs.GetValueOnGameThread()) * 0.001;

	// 서버가 측정한 왕복 지연보다 오래된 시각은 받지 않는다. ping을 아직 모르면 최대 한도만 적용한다.
	if (const APlayerState* PlayerState = ShooterController ? ShooterController->PlayerState : nullptr)
	{
		const float PingMs = PlayerState->GetPingInMilliseconds();
		if (PingMs > 0.0f)
		{
			const double PingLimitSeconds =
				(PingMs + FMath::Max(0.0f, CVarLagCompensationPingToleranceMs.GetValueOnGameThread())) * 0.001;
			MaxRewindSeconds = FMath::Min(MaxRewindSeconds, PingLimitSeconds);
		}
	}

	const double EarliestServerTime = ServerTime - MaxRewindSeconds;
	const double ClampedServerTime = FMath::Clamp(ClientViewServerTime, EarliestServerTime, ServerTime);
	Request.bClamped = !FMath::IsNearlyEqual(ClampedServerTime, ClientViewServerTime, 0.0005);

	const double RewindSeconds = ServerTime - ClampedServerTime;
	if (RewindSeconds <= 0.001)
	{
		return Request;
	}

	Request.RewindServerTime = ClampedServerTime;
	Request.RewindMs = RewindSeconds * 1000.0;
	return Request;
}

bool ULagCompensationSubsystem::LineTraceSingleAtTime(
	const double RewindServerTime,
	const FVector& Start,
	const FVector& End,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore,
	const EDrawDebugTrace::Type DebugDrawType,
	FHitResult& OutHit) const
{
	OutHit = FHitResult();

	// 기록된 캐릭터는 현재 위치 대신 과거 위치로 따로 판정하므로 월드 trace에서 제외한다.
	TArray<AActor*> WorldActorsToIgnore = ActorsToIgnore;
	AppendRewoundCharacters(WorldActorsToIgnore);

	FHitResult WorldHit;
	const bool bWorldHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		Start,
		End,
		ObjectTypes,
		false,
		WorldActorsToIgnore,
		DebugDrawType,
		WorldHit,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);

	TArray<FHitResult> CharacterHits;
	TraceRewoundCharacters(RewindServerTime, Start, End, 0.0f, ObjectTypes, ActorsToIgnore, CharacterHits);

	const FHitResult* ClosestHit = bWorldHit ? &WorldHit : nullptr;
	for (const FHitResult& CharacterHit : CharacterHits)
	{
		if (!ClosestHit || CharacterHit.Time < ClosestHit->Time)
		{
			ClosestHit = &CharacterHit;
		}
	}

	if (!ClosestHit)
	{
		return false;
	}

	OutHit = *ClosestHit;
	return true;
}

bool ULagCompensationSubsystem::SphereTraceMultiAtTime(
	const double RewindServerTime,
	const FVector& Start,
	const FVector& End,
	const float Radius,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore,
	const EDrawDebugTrace::Type DebugDrawType,
	TArray<FHitResult>& OutHits) const
{
	OutHits.Reset();

	TArray<AActor*> WorldActorsToIgnore = ActorsToIgnore;
	AppendRewoundCharacters(WorldActorsToIgnore);

	UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		Start,
		End,
		Radius,
		ObjectTypes,
		false,
		WorldActorsToIgnore,
		DebugDrawType,
		OutHits,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);

	TArray<FHitResult> CharacterHits;
	TraceRewoundCharacters(RewindServerTime, Start, End, Radius, ObjectTypes, ActorsToIgnore, CharacterHits);
	OutHits.Append(CharacterHits);

	// 호출자는 첫 번째 유효 충돌을 고르므로 현재 월드 충돌과 과거 캐릭터 충돌을 거리순으로 합친다.
	OutHits.StableSort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Time < B.Time;
	});
	return !OutHits.IsEmpty();
}

bool ULagCompensationSubsystem::GetCapsuleCenterAtTime(
	const ACharacterBase* Character,
	const double ServerTime,
	FVector& OutCenter) const
{
	const FPdCharacterHitHistory* History = FindHistory(Character);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	FPdHitboxSnapshot Snapshot;
	if (!History || !Mesh || !FindSnapshotAtTime(*History, ServerTime, Snapshot))
	{
		return false;
	}

	const FTransform CurrentTransform(Mesh->GetComponentQuat(), Mesh->GetComponentLocation());
	const FTransform RewoundTransform(Snapshot.Rotation, Snapshot.Location);
	OutCenter = MapCurrentToRewound(CurrentTransform, RewoundTransform, Character->GetActorLocation());
	return true;
}

void ULagCompensationSubsystem::RecordServerShot(
	const AController* ShooterController,
	const FPdRewindRequest& RewindRequest,
	const ACharacterBase* CurrentHitCharacter,
	const ACharacterBase* RewoundHitCharacter)
{
	++Stats.ServerShots;
	Stats.RewoundShots += RewindRequest.IsRewinding() ? 1 : 0;
	Stats.ClampedShots += RewindRequest.bClamped ? 1 : 0;
	Stats.CurrentCharacterHits += CurrentHitCharacter ? 1 : 0;
	Stats.RewoundCharacterHits += RewoundHitCharacter ? 1 : 0;
	Stats.RewoundOnlyHits += RewoundHitCharacter && !CurrentHitCharacter ? 1 : 0;
	Stats.CurrentOnlyHits += CurrentHitCharacter && !RewoundHitCharacter ? 1 : 0;
	Stats.TotalRewindMs += RewindRequest.RewindMs;
	Stats.MaxRewindMs = FMath::Max(Stats.MaxRewindMs, RewindRequest.RewindMs);

	const APlayerState* PlayerState = ShooterController ? ShooterController->PlayerState : nullptr;
	UE_LOG(LogPdLagCompensation, Log,
		TEXT("Shot by %s: rewind=%.1fms%s ping=%.0fms current=%s rewound=%s"),
		PlayerState ? *PlayerState->GetPlayerName() : TEXT("Unknown"),
		RewindRequest.RewindMs,
		RewindRequest.bClamped ? TEXT(" (clamped)") : TEXT(""),
		PlayerState ? PlayerState->GetPingInMilliseconds() : 0.0f,
		CurrentHitCharacter ? *CurrentHitCharacter->GetName() : TEXT("miss"),
		RewoundHitCharacter ? *RewoundHitCharacter->GetName() : TEXT("miss"));
}

void ULagCompensationSubsystem::RecordClientShot(const bool bPerceivedCharacterHit)
{
	++Stats.ClientShots;
	Stats.ClientPerceivedHits += bPerceivedCharacterHit ? 1 : 0;
}

void ULagCompensationSubsystem::ResetStats()
{
	Stats = FPdLagCompensationStats();
}

FString ULagCompensationSubsystem::DescribeStats() const
{
	const auto Percent = [](const int32 Count, const int32 Total)
	{
		return Total > 0 ? 100.0 * static_cast<double>(Count) / static_cast<double>(Total) : 0.0;
	};

	return FString::Printf(
		TEXT("server shots=%d rewound=%d clamped=%d avgRewind=%.1fms maxRewind=%.1fms | ")
		TEXT("hit(current)=%d (%.1f%%) hit(rewound)=%d (%.1f%%) rewoundOnly=%d currentOnly=%d | ")
		TEXT("client shots=%d perceivedHits=%d (%.1f%%)"),
		Stats.ServerShots,
		Stats.RewoundShots,
		Stats.ClampedShots,
		Stats.RewoundShots > 0 ? Stats.TotalRewindMs / Stats.RewoundShots : 0.0,
		Stats.MaxRewindMs,
		Stats.CurrentCharacterHits,
		Percent(Stats.CurrentCharacterHits, Stats.ServerShots),
		Stats.RewoundCharacterHits,
		Percent(Stats.RewoundCharacterHits, Stats.ServerShots),
		Stats.RewoundOnlyHits,
		Stats.CurrentOnlyHits,
		Stats.ClientShots,
		Stats.ClientPerceivedHits,
		Percent(Stats.ClientPerceivedHits, Stats.ClientShots));
}

// Internal Helpers ----------------------------------------------------------------------------------------------------

bool ULagCompensationSubsystem::ShouldRecordHistory() const
{
	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
}

void ULagCompensationSubsystem::RecordSnapshots()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double ServerTime = World->GetTimeSeconds();
	const double HistorySeconds = FMath::Max(
		CVarLagCompensationHistoryMs.GetValueOnGameThread(),
		CVarLagCompensationMaxRewindMs.GetValueOnGameThread() + 100.0f) * 0.001;
	const double OldestKeptTime = ServerTime - HistorySeconds;

	for (int32 HistoryIndex = Histories.Num() - 1; HistoryIndex >= 0; --HistoryIndex)
	{
		FPdCharacterHitHistory& History = Histories[HistoryIndex];
		const ACharacterBase* Character = History.Character.Get();
		if (!IsValid(Character))
		{
			Histories.RemoveAtSwap(HistoryIndex);
			continue;
		}

		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!Mesh)
		{
			continue;
		}

		FPdHitboxSnapshot Snapshot;
		Snapshot.ServerTime = ServerTime;
		Snapshot.Location = Mesh->GetComponentLocation();
		Snapshot.Rotation = Mesh->GetComponentQuat();

		TArray<FPdHitboxSnapshot>& Snapshots = History.Snapshots;
		if (!Snapshots.IsEmpty() && Snapshots.Last().ServerTime >= ServerTime)
		{
			Snapshots.Last() = Snapshot;
		}
		else
		{
			Snapshots.Add(Snapshot);
		}

		// 보간을 위해 기록 구간 경계보다 오래된 표본은 하나만 남긴다.
		int32 RemoveCount = 0;
		while (RemoveCount + 1 < Snapshots.Num() && Snapshots[RemoveCount + 1].ServerTime <= OldestKeptTime)
		{
			++RemoveCount;
		}

		if (RemoveCount > 0)
		{
			Snapshots.RemoveAt(0, RemoveCount);
		}
	}
}

const FPdCharacterHitHistory* ULagCompensationSubsystem::FindHistory(const ACharacterBase* Character) const
{
	if (!Character)
	{
		return nullptr;
	}

	return Histories.FindByPredicate([Character](const FPdCharacterHitHistory& History)
	{
		return History.Character.Get() == Character;
	});
}

bool ULagCompensationSubsystem::FindSnapshotAtTime(
	const FPdCharacterHitHistory& History,
	const double ServerTime,
	FPdHitboxSnapshot& OutSnapshot) const
{
	const TArray<FPdHitboxSnapshot>& Snapshots = History.Snapshots;
	if (Snapshots.IsEmpty())
	{
		return false;
	}

	if (ServerTime <= Snapshots[0].ServerTime)
	{
		OutSnapshot = Snapshots[0];
		return true;
	}

	if (ServerTime >= Snapshots.Last().ServerTime)
	{
		OutSnapshot = Snapshots.Last();
		return true;
	}

	// 되감기 구간은 짧으므로 최신 표본부터 찾는다.
	for (int32 SnapshotIndex = Snapshots.Num() - 1; SnapshotIndex > 0; --SnapshotIndex)
	{
		const FPdHitboxSnapshot& Older = Snapshots[SnapshotIndex - 1];
		const FPdHitboxSnapshot& Newer = Snapshots[SnapshotIndex];
		if (ServerTime < Older.ServerTime)
		{
			continue;
		}

		const double Span = Newer.ServerTime - Older.ServerTime;
		const double Alpha = Span > UE_DOUBLE_SMALL_NUMBER
			? FMath::Clamp((ServerTime - Older.ServerTime) / Span, 0.0, 1.0)
			: 1.0;

		// 부활·포털처럼 순간 이동한 구간은 중간 위치가 존재하지 않으므로 가까운 표본을 그대로 사용한다.
		const double TeleportDistance = FMath::Max(0.0f, CVarLagCompensationTeleportDistance.GetValueOnGameThread());
		if (FVector::DistSquared(Older.Location, Newer.Location) > FMath::Square(TeleportDistance))
		{
			OutSnapshot = Alpha < 0.5 ? Older : Newer;
			return true;
		}

		OutSnapshot.ServerTime = ServerTime;
		OutSnapshot.Location = Older.Location + (Newer.Location - Older.Location) * Alpha;
		OutSnapshot.Rotation = FQuat::Slerp(Older.Rotation, Newer.Rotation, Alpha);
		return true;
	}

	OutSnapshot = Snapshots[0];
	return true;
}

void ULagCompensationSubsystem::AppendRewoundCharacters(TArray<AActor*>& InOutActorsToIgnore) const
{
	for (const FPdCharacterHitHistory& History : Histories)
	{
		ACharacterBase* Character = History.Character.Get();
		if (IsValid(Character) && !History.Snapshots.IsEmpty())
		{
			InOutActorsToIgnore.AddUnique(Character);
		}
	}
}

// 과거 위치의 메시에 대한 trace를 현재 메시 좌표계에서 수행하고 결과를 과거 위치 기준으로 되돌린다.
void ULagCompensationSubsystem::TraceRewoundCharacters(
	const double RewindServerTime,
	const FVector& Start,
	const FVector& End,
	const float Radius,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	const TArray<AActor*>& ActorsToIgnore,
	TArray<FHitResult>& OutHits) const
{
	for (const FPdCharacterHitHistory& History : Histories)
	{
		ACharacterBase* Character = History.Character.Get();
		if (!IsValid(Character) || ActorsToIgnore.Contains(Character))
		{
			continue;
		}

		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (!Mesh
			|| !Mesh->IsQueryCollisionEnabled()
			|| !ObjectTypes.Contains(UEngineTypes::ConvertToObjectType(Mesh->GetCollisionObjectType())))
		{
			continue;
		}

		FPdHitboxSnapshot Snapshot;
		if (!FindSnapshotAtTime(History, RewindServerTime, Snapshot))
		{
			continue;
		}

		const FTransform CurrentTransform(Mesh->GetComponentQuat(), Mesh->GetComponentLocation());
		const FTransform RewoundTransform(Snapshot.Rotation, Snapshot.Location);

		if (Mesh->Bounds.SphereRadius > 0.0f)
		{
			const FVector RewoundBoundsCenter = MapCurrentToRewound(CurrentTransform, RewoundTransform, Mesh->Bounds.Origin);
			const double CullRadius = Mesh->Bounds.SphereRadius + Radius;
			if (FMath::PointDistToSegmentSquared(RewoundBoundsCenter, Start, End) > FMath::Square(CullRadius))
			{
				continue;
			}
		}

		const FVector ShiftedStart = MapRewoundToCurrent(CurrentTransform, RewoundTransform, Start);
		const FVector ShiftedEnd = MapRewoundToCurrent(CurrentTransform, RewoundTransform, End);

		FHitResult MeshHit;
		bool bHit = false;
		if (Radius > UE_KINDA_SMALL_NUMBER)
		{
			bHit = Mesh->SweepComponent(
				MeshHit,
				ShiftedStart,
				ShiftedEnd,
				FQuat::Identity,
				FCollisionShape::MakeSphere(Radius));
		}
		else
		{
			const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PdLagCompensationTrace), false);
			bHit = Mesh->LineTraceComponent(MeshHit, ShiftedStart, ShiftedEnd, QueryParams);
		}

		if (!bHit)
		{
			continue;
		}

		// 선분 길이가 같은 강체 변환이므로 Time·Distance는 그대로 유효하다.
		MeshHit.bBlockingHit = true;
		MeshHit.TraceStart = Start;
		MeshHit.TraceEnd = End;
		MeshHit.Location = MapCurrentToRewound(CurrentTransform, RewoundTransform, MeshHit.Location);
		MeshHit.ImpactPoint = MapCurrentToRewound(CurrentTransform, RewoundTransform, MeshHit.ImpactPoint);
		MeshHit.Normal = MapCurrentToRewoundDirection(CurrentTransform, RewoundTransform, MeshHit.Normal);
		MeshHit.ImpactNormal = MapCurrentToRewoundDirection(CurrentTransform, RewoundTransform, MeshHit.ImpactNormal);
		MeshHit.Component = Mesh;
		MeshHit.HitObjectHandle = FActorInstanceHandle(Character);
		OutHits.Add(MeshHit);
	}
}
