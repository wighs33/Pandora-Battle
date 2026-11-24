#include "Story/StoryPortal.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryPortal)

namespace
{
	/** 열릴 때는 목표를 넘었다 돌아오고, 닫힐 때는 곧게 오므라든다(스프링의 강성과 감쇠). */
	constexpr float OpenStiffness = 60.f;
	constexpr float OpenDamping = 8.f;
	constexpr float CloseSpeed = 1.6f;
	constexpr float MaxStepSeconds = 1.f / 120.f;
	constexpr float FlashFadeSpeed = 2.2f;
	constexpr float FlashLightScale = 5.f;
	constexpr float FlashSizeScale = 0.25f;

	UStaticMeshComponent* MakePart(AActor& Owner, USceneComponent& Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* Part = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(&Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		return Part;
	}
}

AStoryPortal::AStoryPortal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Ring = CreateDefaultSubobject<USceneComponent>(TEXT("Ring"));
	SetRootComponent(Ring);
	Swirl = MakePart(*this, *Ring, TEXT("Swirl"));
	Shards = MakePart(*this, *Ring, TEXT("Shards"));
	Rim = MakePart(*this, *Ring, TEXT("Rim"));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Ring);
	Light->SetRelativeLocation(FVector(0.f, 120.f, 0.f));
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetLightColor(FLinearColor(0.85f, 0.45f, 1.f));
	Light->SetAttenuationRadius(1400.f);
	Light->SetCastShadows(false);
}

void AStoryPortal::BeginPlay()
{
	Super::BeginPlay();
	Ring->SetRelativeScale3D(FVector(UE_KINDA_SMALL_NUMBER));
	Light->SetIntensity(0.f);
	SetActorHiddenInGame(true);
}

void AStoryPortal::Open()
{
	bOpen = true;
	OpenVelocity = 0.f;
	SetActorHiddenInGame(false);
}

void AStoryPortal::Close()
{
	bOpen = false;
}

void AStoryPortal::Flash()
{
	FlashAmount = 1.f;
}

void AStoryPortal::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsHidden())
	{
		return;
	}
	if (bOpen)
	{
		for (float Remaining = DeltaSeconds; Remaining > 0.f; Remaining -= MaxStepSeconds)
		{
			const float Step = FMath::Min(Remaining, MaxStepSeconds);
			OpenVelocity += (OpenStiffness * (1.f - OpenScale) - OpenDamping * OpenVelocity) * Step;
			OpenScale += OpenVelocity * Step;
		}
	}
	else
	{
		OpenScale -= CloseSpeed * DeltaSeconds;
		if (OpenScale <= 0.f)
		{
			OpenScale = 0.f;
			SetActorHiddenInGame(true);
		}
	}
	FlashAmount = FMath::FInterpTo(FlashAmount, 0.f, DeltaSeconds, FlashFadeSpeed);
	Ring->SetRelativeScale3D(FVector(FMath::Max(OpenScale * (1.f + FlashSizeScale * FlashAmount), UE_KINDA_SMALL_NUMBER)));
	Light->SetIntensity(LightIntensity * FMath::Max(OpenScale, 0.f) * (1.f + FlashLightScale * FlashAmount));
	// 메시는 +Y를 앞으로 본다. 고리는 그 축을 따라 돈다.
	Shards->AddLocalRotation(FRotator(ShardSpin * DeltaSeconds, 0.f, 0.f));
	Rim->AddLocalRotation(FRotator(-ShardSpin * 0.5f * DeltaSeconds, 0.f, 0.f));
}
