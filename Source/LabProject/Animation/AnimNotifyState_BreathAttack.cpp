#include "Animation/AnimNotifyState_BreathAttack.h"

#include "AI/Monster/MonsterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_BreathAttack)

namespace
{
/** 이보다 위를 향한 면(바닥)에 닿은 불길에만 바닥 불을 놓는다. 걸을 수 있는 경사의 기본값과 같다. */
constexpr float MinFloorNormalZ = 0.71f;
}

// 불꽃은 화면을 그리는 인스턴스에서만 띄운다. 불꽃들은 매 프레임 입에서 불길 끝까지 월드 기준으로 다시 놓으므로, 자리를 잡은 뒤에 켠다.
void UAnimNotifyState_BreathAttack::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !Effect || MeshComp->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (int32 Segment = 0; Segment < FlameSegments; ++Segment)
	{
		if (UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(Effect, MeshComp, NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, false))
		{
			Spawned->ComponentTags.AddUnique(GetSpawnedComponentTag(Segment));
			Spawned->SetUsingAbsoluteLocation(true);
			Spawned->SetUsingAbsoluteRotation(true);
		}
	}
	const FVector Start = GetFlameStart(*MeshComp);
	const FRotator Aim = GetAimRotation(*MeshComp, Start);
	FHitResult FlameHit;
	PlaceSpawnedEffects(*MeshComp, Start, TraceFlame(*MeshComp, Start, Aim, FlameHit), Aim);
	for (int32 Segment = 0; Segment < FlameSegments; ++Segment)
	{
		if (UNiagaraComponent* Spawned = FindSpawnedEffect(*MeshComp, Segment))
		{
			Spawned->Activate(true);
		}
	}
}

// 몸이 돌고 입이 움직이는 대로 불꽃을 다시 놓고 불길이 닿은 바닥에 불을 놓는다. 서버는 같은 불길 선분으로 피해를 준다.
void UAnimNotifyState_BreathAttack::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!MeshComp)
	{
		return;
	}
	const FVector Start = GetFlameStart(*MeshComp);
	const FRotator Aim = GetAimRotation(*MeshComp, Start);
	FHitResult FlameHit;
	const FVector End = TraceFlame(*MeshComp, Start, Aim, FlameHit);
	if (MeshComp->GetNetMode() != NM_DedicatedServer)
	{
		PlaceSpawnedEffects(*MeshComp, Start, End, Aim);
		if (FlameHit.bBlockingHit)
		{
			SpawnImpactEffect(*MeshComp, FlameHit, FrameDeltaTime);
		}
	}
	AMonsterCharacter* Monster = Cast<AMonsterCharacter>(MeshComp->GetOwner());
	if (Monster && Monster->HasAuthority())
	{
		Monster->ApplyBreathDamage(Start, End, Radius, HitInterval);
	}
}

// 이펙트는 바로 지우지 않고 꺼서, 뿜어 둔 불꽃이 사그라든 뒤 스스로 사라지게 한다.
void UAnimNotifyState_BreathAttack::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	for (int32 Segment = 0; MeshComp && Segment < FlameSegments; ++Segment)
	{
		if (UNiagaraComponent* Spawned = FindSpawnedEffect(*MeshComp, Segment))
		{
			Spawned->ComponentTags.Remove(GetSpawnedComponentTag(Segment));
			Spawned->Deactivate();
		}
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_BreathAttack::GetNotifyName_Implementation() const
{
	return TEXT("Breath Attack");
}

FVector UAnimNotifyState_BreathAttack::GetFlameStart(const USkeletalMeshComponent& MeshComp) const
{
	return MeshComp.GetSocketTransform(SocketName).TransformPosition(LocationOffset);
}

// 바닥 높이는 몸 충돌 원통의 바닥(캐릭터의 발)으로 본다.
FRotator UAnimNotifyState_BreathAttack::GetAimRotation(const USkeletalMeshComponent& MeshComp, const FVector& Start) const
{
	const AActor* Owner = MeshComp.GetOwner();
	if (!Owner)
	{
		return MeshComp.GetComponentRotation();
	}
	float CollisionRadius = 0.0f;
	float CollisionHalfHeight = 0.0f;
	Owner->GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
	const FVector AimPoint = Owner->GetActorLocation() + Owner->GetActorForwardVector().GetSafeNormal2D() * AimDistance
		- FVector(0.0f, 0.0f, CollisionHalfHeight);
	return (AimPoint - Start).Rotation();
}

FVector UAnimNotifyState_BreathAttack::TraceFlame(const USkeletalMeshComponent& MeshComp, const FVector& Start, const FRotator& Aim,
	FHitResult& OutHit) const
{
	const FVector End = Start + Aim.Vector() * Reach;
	const UWorld* World = MeshComp.GetWorld();
	if (World && World->LineTraceSingleByObjectType(OutHit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionQueryParams(SCENE_QUERY_STAT(BreathReach), false, MeshComp.GetOwner())))
	{
		return OutHit.ImpactPoint;
	}
	return End;
}

// 노티파이 객체는 같은 몽타주를 재생하는 모든 몬스터가 함께 쓰므로, 마지막으로 띄운 시각을 들고 있지 않고 월드 시간이 간격 경계를 넘었는지로 정한다.
// 벽에 닿은 불길에는 띄우지 않는다. 바닥 불은 반복 이펙트일 수 있어 켜 둘 시간이 지나면 끄고, 남은 불꽃이 사그라들면 스스로 사라진다.
void UAnimNotifyState_BreathAttack::SpawnImpactEffect(const USkeletalMeshComponent& MeshComp, const FHitResult& FlameHit, const float FrameDeltaTime) const
{
	UWorld* World = MeshComp.GetWorld();
	if (!ImpactEffect || !World || FlameHit.ImpactNormal.Z < MinFloorNormalZ)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (FMath::FloorToInt64(Now / ImpactInterval) == FMath::FloorToInt64((Now - FrameDeltaTime) / ImpactInterval))
	{
		return;
	}
	UNiagaraComponent* Fire = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, FlameHit.ImpactPoint, FRotator::ZeroRotator,
		FVector(ImpactEffectScale));
	if (!Fire)
	{
		return;
	}
	FTimerHandle LifetimeHandle;
	World->GetTimerManager().SetTimer(LifetimeHandle, FTimerDelegate::CreateWeakLambda(Fire, [Fire]() { Fire->Deactivate(); }), ImpactLifetime, false);
}

UNiagaraComponent* UAnimNotifyState_BreathAttack::FindSpawnedEffect(const USkeletalMeshComponent& MeshComp, const int32 Segment) const
{
	const FName Tag = GetSpawnedComponentTag(Segment);
	for (USceneComponent* Child : MeshComp.GetAttachChildren())
	{
		UNiagaraComponent* Spawned = Cast<UNiagaraComponent>(Child);
		if (Spawned && Spawned->ComponentHasTag(Tag))
		{
			return Spawned;
		}
	}
	return nullptr;
}

void UAnimNotifyState_BreathAttack::PlaceSpawnedEffects(const USkeletalMeshComponent& MeshComp, const FVector& Start, const FVector& End,
	const FRotator& Aim) const
{
	for (int32 Segment = 0; Segment < FlameSegments; ++Segment)
	{
		if (UNiagaraComponent* Spawned = FindSpawnedEffect(MeshComp, Segment))
		{
			Spawned->SetWorldLocationAndRotation(FMath::Lerp(Start, End, static_cast<float>(Segment) / FlameSegments), Aim);
		}
	}
}

FName UAnimNotifyState_BreathAttack::GetSpawnedComponentTag(const int32 Segment) const
{
	return FName(*FString::Printf(TEXT("%s_Flame%d"), *GetName(), Segment));
}
