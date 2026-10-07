#include "AI/Monster/MonsterSignificanceSubsystem.h"

#include "AI/Monster/MonsterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterSignificanceSubsystem)

namespace
{
	TAutoConsoleVariable<float> CVarOffscreenMovementTickInterval(TEXT("pd.Monster.OffscreenMovementTickInterval"), 0.25f,
		TEXT("클라이언트에서 화면 밖 몬스터의 이동 시뮬레이션 주기(초). 0이면 매 프레임 돈다."));

	/** 화면 밖 판정을 다시 하는 간격과, 마지막으로 그려진 뒤 이 시간이 지나야 화면 밖으로 보는 여유. */
	constexpr float UpdateIntervalSeconds = 0.2f;
	constexpr double OffscreenGraceSeconds = 0.3;
}

bool UMonsterSignificanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UMonsterSignificanceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(UpdateTimerHandle, this, &ThisClass::UpdateMovementTickIntervals, UpdateIntervalSeconds, true);
}

void UMonsterSignificanceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	Super::Deinitialize();
}

void UMonsterSignificanceSubsystem::RegisterMonster(AMonsterCharacter* Monster)
{
	Monsters.AddUnique(Monster);
}

void UMonsterSignificanceSubsystem::UnregisterMonster(AMonsterCharacter* Monster)
{
	Monsters.RemoveSwap(Monster);
}

// 서버 권한으로 움직이는 몬스터(리슨 서버)는 게임 판정에 쓰이므로 건드리지 않는다.
// 화면 밖 몬스터마다 주기를 조금씩 달리해, 같은 프레임에 몰려 한꺼번에 돌지 않게 한다.
void UMonsterSignificanceSubsystem::UpdateMovementTickIntervals()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float OffscreenInterval = FMath::Max(CVarOffscreenMovementTickInterval.GetValueOnGameThread(), 0.0f);
	Monsters.RemoveAllSwap([](const TWeakObjectPtr<AMonsterCharacter>& Monster) { return !Monster.IsValid(); });
	for (const TWeakObjectPtr<AMonsterCharacter>& WeakMonster : Monsters)
	{
		AMonsterCharacter* Monster = WeakMonster.Get();
		UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
		const USkeletalMeshComponent* Mesh = Monster->GetMesh();
		if (Monster->GetLocalRole() != ROLE_SimulatedProxy || !Movement || !Mesh)
		{
			continue;
		}
		const bool bOnScreen = Now - Mesh->GetLastRenderTimeOnScreen() <= OffscreenGraceSeconds;
		if (bOnScreen || OffscreenInterval <= 0.0f)
		{
			if (Movement->GetComponentTickInterval() > 0.0f)
			{
				Movement->SetComponentTickInterval(0.0f);
			}
		}
		else if (Movement->GetComponentTickInterval() <= 0.0f)
		{
			Movement->SetComponentTickInterval(OffscreenInterval * FMath::FRandRange(1.0f, 1.25f));
		}
	}
}
