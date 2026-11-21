#include "AI/Monster/MonsterAreaStrike.h"

#include "AI/Monster/AttackTelegraphActor.h"
#include "AI/Monster/MonsterCharacter.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterAreaStrike)

namespace
{
	/** 떨어진 뒤 폭발 효과가 다른 화면에도 닿을 때까지 액터를 남겨 두는 시간. */
	constexpr float LingerAfterImpactSeconds = 1.0f;

	/** 바닥 원이 가득 찬 뒤 사라지기까지의 시간. */
	constexpr float TelegraphHoldSeconds = 0.15f;
}

AMonsterAreaStrike::AMonsterAreaStrike()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

// 서버와 각 화면이 생긴 순간부터 각자 시간을 잰다. 화면은 복제가 늦게 닿은 만큼 늦게 그리지만, 판정은 서버 시간으로 한 번만 한다.
void AMonsterAreaStrike::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	if (GetNetMode() != NM_DedicatedServer)
	{
		AAttackTelegraphActor::SpawnTimedCircle(*World, GetActorLocation(), Radius, TelegraphMaterial, ImpactDelay,
			ImpactDelay + TelegraphHoldSeconds);
		World->GetTimerManager().SetTimer(FallEffectTimerHandle, this, &ThisClass::PlayFallEffect,
			FMath::Max(ImpactDelay - FallLeadTime, UE_KINDA_SMALL_NUMBER), false);
	}
	World->GetTimerManager().SetTimer(ImpactTimerHandle, this, &ThisClass::Impact, ImpactDelay, false);
}

void AMonsterAreaStrike::PlayFallEffect()
{
	if (FallEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, FallEffect, GetActorLocation(), GetActorRotation(), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease, true);
	}
}

void AMonsterAreaStrike::Impact()
{
	if (GetNetMode() != NM_DedicatedServer && ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, GetActorLocation(), GetActorRotation(), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease, true);
	}
	if (HasAuthority())
	{
		if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetOwner()))
		{
			Monster->ApplyAreaStrikeDamage(GetActorLocation(), Radius);
		}
		SetLifeSpan(LingerAfterImpactSeconds);
	}
}
