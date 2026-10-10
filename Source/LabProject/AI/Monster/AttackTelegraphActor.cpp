#include "AI/Monster/AttackTelegraphActor.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_AttackDamageWindow.h"
#include "Animation/AnimNotifyState_LeapToTarget.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttackTelegraphActor)

namespace
{
/** 데칼이 위아래로 닿는 깊이. 계단과 경사를 덮을 만큼만 둔다. */
constexpr float DecalDepth = 250.0f;

const FAnimNotifyEvent* FindNotifyState(const UAnimMontage& Montage, const UClass* NotifyStateClass)
{
	return Montage.Notifies.FindByPredicate([NotifyStateClass](const FAnimNotifyEvent& Notify)
	{
		return Notify.NotifyStateClass && Notify.NotifyStateClass->IsA(NotifyStateClass);
	});
}

FVector GetFeetLocation(const AActor& Actor)
{
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Actor.GetSimpleCollisionCylinder(Radius, HalfHeight);
	return Actor.GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
}
}

bool FAttackTelegraphSettings::IsInsideFrontCone(const FTransform& Origin, const FVector& FeetLocation, const float TargetRadius) const
{
	const FVector Local = Origin.InverseTransformPositionNoScale(FeetLocation);
	const float Distance = FVector2D(Local.X, Local.Y).Size();
	if (Shape != EAttackTelegraphShape::FrontCone || FMath::Abs(Local.Z) > DecalDepth || Distance > Radius + TargetRadius)
	{
		return false;
	}
	// 부채꼴 밖이어도 옆 가장자리까지의 거리가 캡슐 반지름 안이면 걸친 것으로 본다.
	const float DegreesOutside = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(Local.Y), Local.X)) - HalfAngle;
	return DegreesOutside <= 0.0f || Distance * FMath::Sin(FMath::DegreesToRadians(FMath::Min(DegreesOutside, 90.0f))) <= TargetRadius;
}

AAttackTelegraphActor::AAttackTelegraphActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	// 데칼은 자기 X축 방향으로 투영하므로 아래를 보게 눕힌다. 이때 데칼 UV의 +U가 액터 앞쪽이 된다.
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(GetRootComponent());
	Decal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Decal->FadeScreenSize = 0.0f;
}

// 채움 시간은 몽타주 시작부터 피해 구간 시작까지, 사라지는 시간은 피해 구간 끝이다.
// 원은 도약 노티파이가 시작될 때 대상 발밑에 멈춘다. 서버도 그 순간의 대상 위치로 도착 지점을 정한다.
AAttackTelegraphActor* AAttackTelegraphActor::SpawnForAttack(ACharacter& Monster, const FAttackTelegraphSettings& Settings,
	UMaterialInterface* Material, const UAnimMontage& AttackMontage, const float PlayRate, AActor* Target)
{
	const FAnimNotifyEvent* DamageWindow = FindNotifyState(AttackMontage, UAnimNotifyState_AttackDamageWindow::StaticClass());
	const bool bOnTarget = Settings.Shape == EAttackTelegraphShape::TargetCircle;
	const float Rate = PlayRate * AttackMontage.RateScale;
	UWorld* World = Monster.GetWorld();
	if (Settings.Shape == EAttackTelegraphShape::None || !Material || !DamageWindow || Rate <= 0.0f || !World
		|| (bOnTarget && !IsValid(Target)))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = &Monster;
	const FTransform Origin = GetMonsterOrigin(Monster);
	AAttackTelegraphActor* Telegraph = World->SpawnActor<AAttackTelegraphActor>(
		bOnTarget ? GetFeetLocation(*Target) : Origin.GetLocation(), Origin.Rotator(), SpawnParameters);
	if (!Telegraph)
	{
		return nullptr;
	}

	const double StartTime = World->GetTimeSeconds();
	const float FillSeconds = DamageWindow->GetTriggerTime() / Rate;
	const float EndSeconds = DamageWindow->GetEndTriggerTime() / Rate;
	UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(Material, Telegraph);
	MaterialInstance->SetScalarParameterValue(TEXT("StartTime"), StartTime);
	MaterialInstance->SetScalarParameterValue(TEXT("FillDuration"), FillSeconds);
	MaterialInstance->SetScalarParameterValue(TEXT("EndTime"), StartTime + EndSeconds);
	MaterialInstance->SetScalarParameterValue(TEXT("HalfAngle"), Settings.Shape == EAttackTelegraphShape::FrontCone ? Settings.HalfAngle : 180.0f);
	Telegraph->Decal->SetDecalMaterial(MaterialInstance);
	Telegraph->Decal->DecalSize = FVector(DecalDepth, Settings.Radius, Settings.Radius);
	Telegraph->SetLifeSpan(EndSeconds + 0.2f);

	if (bOnTarget)
	{
		const FAnimNotifyEvent* Leap = FindNotifyState(AttackMontage, UAnimNotifyState_LeapToTarget::StaticClass());
		Telegraph->FollowTarget = Target;
		Telegraph->FollowUntilTime = StartTime + (Leap ? Leap->GetTriggerTime() : DamageWindow->GetTriggerTime()) / Rate;
		Telegraph->SetActorTickEnabled(true);
	}
	else
	{
		Telegraph->AttachToActor(&Monster, FAttachmentTransformRules::KeepWorldTransform);
	}
	return Telegraph;
}

FTransform AAttackTelegraphActor::GetMonsterOrigin(const AActor& Monster)
{
	return FTransform(FRotator(0.0f, Monster.GetActorRotation().Yaw, 0.0f), GetFeetLocation(Monster));
}

// 멈출 때까지 대상 발밑을 따라간다.
void AAttackTelegraphActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AActor* Target = FollowTarget.Get();
	if (Target && GetWorld()->GetTimeSeconds() < FollowUntilTime)
	{
		SetActorLocation(GetFeetLocation(*Target));
		return;
	}
	SetActorTickEnabled(false);
}
