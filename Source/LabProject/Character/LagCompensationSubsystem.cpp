#include "Character/LagCompensationSubsystem.h"

#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LagCompensationSubsystem)

namespace
{
	TAutoConsoleVariable<float> CVarMaxRewindMs(TEXT("pd.LagComp.MaxRewindMs"), 200.0f,
		TEXT("Longest rewind for a shot, in milliseconds. 0 turns lag compensation off."), ECVF_Default);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<bool> CVarDebugDraw(TEXT("pd.LagComp.DebugDraw"), false,
		TEXT("Draws the current (red) and rewound (green) capsule of a character hit by a rewound shot."), ECVF_Cheat);
#endif

	constexpr double MillisecondsToSeconds = 0.001;

	/** 연속한 두 기록이 이보다 멀면 순간 이동(부활, 포털)으로 보고 중간 위치를 만들지 않는다. */
	constexpr double TeleportDistance = 300.0;

	double GetMaxRewindSeconds()
	{
		return FMath::Max(0.0f, CVarMaxRewindMs.GetValueOnGameThread()) * MillisecondsToSeconds;
	}

	/** 크기는 빼고 위치와 회전만 쓴다. 두 시각의 크기가 같아 판정에는 차이가 없다. */
	FTransform GetMeshTransform(const USkeletalMeshComponent& Mesh)
	{
		return FTransform(Mesh.GetComponentQuat(), Mesh.GetComponentLocation());
	}
}

// Engine Overrides ----------------------------------------------------------------------------------------------------

// 틱 가능한 서브시스템은 액터의 틱이 모두 끝난 뒤에 돌므로, 이번 프레임의 최종 위치를 기록한다.
void ULagCompensationSubsystem::Tick(float /*DeltaTime*/)
{
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
	return IsServerWorld() && !Histories.IsEmpty();
}

// Public API ----------------------------------------------------------------------------------------------------------

ULagCompensationSubsystem* ULagCompensationSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<ULagCompensationSubsystem>() : nullptr;
}

bool ULagCompensationSubsystem::IsDebugDrawEnabled()
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return CVarDebugDraw.GetValueOnGameThread();
#endif
}

void ULagCompensationSubsystem::RegisterCharacter(ACharacterBase* Character)
{
	if (IsValid(Character) && Character->HasAuthority() && !FindHistory(Character))
	{
		Histories.Add({Character, {}});
	}
}

void ULagCompensationSubsystem::UnregisterCharacter(const ACharacterBase* Character)
{
	Histories.RemoveAllSwap([Character](const FHistory& History) { return History.Character.Get() == Character; });
}

// 사수 화면의 상대는 편도 지연만큼 늦고 사격 요청도 편도 지연만큼 늦게 오므로, 왕복 지연만큼 되감는다.
TOptional<double> ULagCompensationSubsystem::GetRewindTime(const AController* Shooter) const
{
	const APlayerController* Player = Cast<APlayerController>(Shooter);
	// 서버와 리슨 서버 호스트는 지금 월드를 그대로 보고 쏜다.
	if (!IsServerWorld() || !Player || Player->IsLocalController() || !Player->PlayerState)
	{
		return {};
	}

	const double RoundTripSeconds = Player->PlayerState->GetPingInMilliseconds() * MillisecondsToSeconds;
	const double RewindSeconds = FMath::Min(RoundTripSeconds, GetMaxRewindSeconds());
	if (RewindSeconds <= 0.0)
	{
		return {};
	}
	return GetWorld()->GetTimeSeconds() - RewindSeconds;
}

void ULagCompensationSubsystem::TraceAtTime(const double RewindTime, const FVector& Start, const FVector& End,
	const float Radius, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, const TArray<AActor*>& ActorsToIgnore,
	const EDrawDebugTrace::Type DebugDrawType, TArray<FHitResult>& OutHits) const
{
	// 기록이 있는 캐릭터는 과거 위치로 따로 판정하므로 월드 판정에서 뺀다.
	TArray<AActor*> WorldActorsToIgnore = ActorsToIgnore;
	for (const FHistory& History : Histories)
	{
		if (History.Character.IsValid() && !History.Snapshots.IsEmpty())
		{
			WorldActorsToIgnore.AddUnique(History.Character.Get());
		}
	}

	if (Radius > 0.0f)
	{
		UKismetSystemLibrary::SphereTraceMultiForObjects(this, Start, End, Radius, ObjectTypes, false, WorldActorsToIgnore,
			DebugDrawType, OutHits, true);
	}
	else
	{
		UKismetSystemLibrary::LineTraceMultiForObjects(this, Start, End, ObjectTypes, false, WorldActorsToIgnore,
			DebugDrawType, OutHits, true);
	}
	TraceRewoundCharacters(RewindTime, Start, End, Radius, ObjectTypes, ActorsToIgnore, OutHits);

	// 호출한 쪽은 앞에서부터 유효한 충돌을 고르므로, 월드 충돌과 과거 위치의 캐릭터 충돌을 거리순으로 합친다.
	OutHits.StableSort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });
}

// 캡슐은 메시와 함께 움직이므로, 지금 캡슐 중심을 메시 기준으로 과거 위치에 옮긴다.
TOptional<FVector> ULagCompensationSubsystem::GetCapsuleCenterAtTime(const ACharacterBase& Character, const double Time) const
{
	const FHistory* History = FindHistory(&Character);
	const USkeletalMeshComponent* Mesh = Character.GetMesh();
	const TOptional<FTransform> RewoundTransform = History && Mesh ? FindMeshTransformAtTime(*History, Time) : TOptional<FTransform>();
	if (!RewoundTransform)
	{
		return {};
	}
	return RewoundTransform->TransformPosition(GetMeshTransform(*Mesh).InverseTransformPosition(Character.GetActorLocation()));
}

// Internal Helpers ----------------------------------------------------------------------------------------------------

bool ULagCompensationSubsystem::IsServerWorld() const
{
	const ENetMode NetMode = GetWorld()->GetNetMode();
	return NetMode == NM_DedicatedServer || NetMode == NM_ListenServer;
}

// 되감을 수 있는 가장 이른 시각보다 오래된 기록은 지운다. 그 시각도 앞뒤 기록을 섞을 수 있게 경계 밖 기록 하나는 남긴다.
void ULagCompensationSubsystem::RecordSnapshots()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const double EarliestRewindTime = Now - GetMaxRewindSeconds();
	Histories.RemoveAllSwap([](const FHistory& History) { return !History.Character.IsValid(); });
	for (FHistory& History : Histories)
	{
		const USkeletalMeshComponent* Mesh = History.Character->GetMesh();
		if (!Mesh)
		{
			continue;
		}

		TArray<FSnapshot>& Snapshots = History.Snapshots;
		if (!Snapshots.IsEmpty() && Snapshots.Last().Time >= Now)
		{
			Snapshots.Pop();
		}
		Snapshots.Add({Now, GetMeshTransform(*Mesh)});

		int32 ExpiredCount = 0;
		while (ExpiredCount + 1 < Snapshots.Num() && Snapshots[ExpiredCount + 1].Time <= EarliestRewindTime)
		{
			++ExpiredCount;
		}
		Snapshots.RemoveAt(0, ExpiredCount);
	}
}

const ULagCompensationSubsystem::FHistory* ULagCompensationSubsystem::FindHistory(const ACharacterBase* Character) const
{
	return Character
		? Histories.FindByPredicate([Character](const FHistory& History) { return History.Character.Get() == Character; })
		: nullptr;
}

// 두 기록 사이의 시각은 앞뒤 기록을 섞는다. 기록 범위 밖이면 가까운 끝의 기록을 쓴다.
TOptional<FTransform> ULagCompensationSubsystem::FindMeshTransformAtTime(const FHistory& History, const double Time) const
{
	const TArray<FSnapshot>& Snapshots = History.Snapshots;
	if (Snapshots.IsEmpty())
	{
		return {};
	}
	if (Time >= Snapshots.Last().Time)
	{
		return Snapshots.Last().MeshTransform;
	}

	// 되감는 폭이 짧으므로 최신 기록부터 찾는다. 기록 시각은 서로 다르므로 두 기록 사이의 간격은 0이 아니다.
	for (int32 NewerIndex = Snapshots.Num() - 1; NewerIndex > 0; --NewerIndex)
	{
		const FSnapshot& Older = Snapshots[NewerIndex - 1];
		const FSnapshot& Newer = Snapshots[NewerIndex];
		if (Time < Older.Time)
		{
			continue;
		}

		const double Alpha = (Time - Older.Time) / (Newer.Time - Older.Time);
		// 순간 이동한 구간에는 중간 위치가 없으므로 가까운 기록을 그대로 쓴다.
		if (FVector::Dist(Older.MeshTransform.GetLocation(), Newer.MeshTransform.GetLocation()) > TeleportDistance)
		{
			return Alpha < 0.5 ? Older.MeshTransform : Newer.MeshTransform;
		}
		FTransform Blended;
		Blended.Blend(Older.MeshTransform, Newer.MeshTransform, Alpha);
		return Blended;
	}
	return Snapshots[0].MeshTransform;
}

// 사선을 과거 위치 기준에서 지금 메시 기준으로 옮겨 그 메시에만 판정하고, 맞은 결과를 다시 과거 위치 기준으로 되돌린다.
void ULagCompensationSubsystem::TraceRewoundCharacters(const double RewindTime, const FVector& Start, const FVector& End,
	const float Radius, const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, const TArray<AActor*>& ActorsToIgnore,
	TArray<FHitResult>& OutHits) const
{
	for (const FHistory& History : Histories)
	{
		ACharacterBase* Character = History.Character.Get();
		USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		if (!Mesh || ActorsToIgnore.Contains(Character) || !Mesh->IsQueryCollisionEnabled()
			|| !ObjectTypes.Contains(UEngineTypes::ConvertToObjectType(Mesh->GetCollisionObjectType())))
		{
			continue;
		}
		const TOptional<FTransform> RewoundTransform = FindMeshTransformAtTime(History, RewindTime);
		if (!RewoundTransform)
		{
			continue;
		}

		// A * B는 A를 먼저 적용한다. 과거 메시 기준 좌표로 바꾼 뒤 지금 메시 위치에 놓는다.
		const FTransform RewoundToCurrent = RewoundTransform->Inverse() * GetMeshTransform(*Mesh);
		const FTransform CurrentToRewound = RewoundToCurrent.Inverse();
		const FVector CurrentStart = RewoundToCurrent.TransformPosition(Start);
		const FVector CurrentEnd = RewoundToCurrent.TransformPosition(End);

		FHitResult Hit;
		const bool bHit = Radius > 0.0f
			? Mesh->SweepComponent(Hit, CurrentStart, CurrentEnd, FQuat::Identity, FCollisionShape::MakeSphere(Radius))
			: Mesh->LineTraceComponent(Hit, CurrentStart, CurrentEnd, FCollisionQueryParams(SCENE_QUERY_STAT(LagCompensationTrace)));
		if (!bHit)
		{
			continue;
		}

		// 강체 변환이라 선분 길이가 같으므로 Time과 Distance는 그대로 쓸 수 있다.
		Hit.bBlockingHit = true;
		Hit.TraceStart = Start;
		Hit.TraceEnd = End;
		Hit.Location = CurrentToRewound.TransformPosition(Hit.Location);
		Hit.ImpactPoint = CurrentToRewound.TransformPosition(Hit.ImpactPoint);
		Hit.Normal = CurrentToRewound.TransformVectorNoScale(Hit.Normal);
		Hit.ImpactNormal = CurrentToRewound.TransformVectorNoScale(Hit.ImpactNormal);
		Hit.Component = Mesh;
		Hit.HitObjectHandle = FActorInstanceHandle(Character);
		OutHits.Add(Hit);
	}
}
