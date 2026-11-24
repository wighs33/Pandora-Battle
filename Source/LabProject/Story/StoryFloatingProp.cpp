#include "Story/StoryFloatingProp.h"

#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryFloatingProp)

AStoryFloatingProp::AStoryFloatingProp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
}

void AStoryFloatingProp::BeginPlay()
{
	Super::BeginPlay();
	PlacedTransform = GetActorTransform();
	// 소품마다 박자가 어긋나게 놓인 자리로 시작 시각을 정한다. 함께 움직이는 겹친 소품(유리와 속)은 자리가 같아 박자도 같다.
	const FVector Location = PlacedTransform.GetLocation();
	Elapsed = FMath::Frac(FMath::Abs(Location.X * 0.0137 + Location.Y * 0.0071)) * PeriodSeconds;
}

void AStoryFloatingProp::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	const float Wave = FMath::Sin(UE_TWO_PI * Elapsed / PeriodSeconds);
	FTransform Current = PlacedTransform;
	Current.AddToTranslation(FVector(0.0, 0.0, BobHeight * Wave));
	// 놓인 기울기를 지킨 채 소품의 위쪽 축으로 돈다.
	Current.SetRotation(PlacedTransform.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(FMath::Fmod(SpinDegreesPerSecond * Elapsed, 360.f))));
	Current.SetScale3D(PlacedTransform.GetScale3D() * (1.f + PulseScale * Wave));
	SetActorTransform(Current);
}
