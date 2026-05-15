#include "UI/DamageIndicatorActor.h"

#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageIndicatorActor)

namespace
{
	void SetNumericProperty(UObject* Object, const FName PropertyName, const double Value)
	{
		if (!Object || PropertyName.IsNone())
		{
			return;
		}

		UClass* ObjectClass = Object->GetClass();
		if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(ObjectClass, PropertyName))
		{
			FloatProperty->SetPropertyValue_InContainer(Object, static_cast<float>(Value));
			return;
		}

		if (FDoubleProperty* DoubleProperty = FindFProperty<FDoubleProperty>(ObjectClass, PropertyName))
		{
			DoubleProperty->SetPropertyValue_InContainer(Object, Value);
		}
	}

	void SetBoolProperty(UObject* Object, const FName PropertyName, const bool bValue)
	{
		if (!Object || PropertyName.IsNone())
		{
			return;
		}

		if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName))
		{
			BoolProperty->SetPropertyValue_InContainer(Object, bValue);
		}
	}
}

ADamageIndicatorActor::ADamageIndicatorActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DamageWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageWidget"));
	DamageWidget->SetupAttachment(SceneRoot);
	DamageWidget->SetWidgetSpace(EWidgetSpace::World);
	DamageWidget->SetDrawAtDesiredSize(true);
	DamageWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageWidget->SetGenerateOverlapEvents(false);
}

void ADamageIndicatorActor::BeginPlay()
{
	Super::BeginPlay();

	if (DamageWidget)
	{
		DamageWidget->InitWidget();
	}

	if (bPayloadInitialized)
	{
		ApplyPayloadToWidget();
		ReceiveDamageIndicatorInitialized(Payload);
		StartMovement();
	}
}

void ADamageIndicatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMovementActive)
	{
		UpdateFacing();
		return;
	}

	ElapsedTime += DeltaSeconds;

	const float RawAlpha = Duration > 0.0f ? FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f) : 1.0f;
	const float MovementAlpha = MovementAlphaCurve ? MovementAlphaCurve->GetFloatValue(RawAlpha) : RawAlpha;

	SetActorLocation(FMath::Lerp(Payload.StartLocation, Payload.EndLocation, MovementAlpha));
	UpdateFacing();
	ReceiveDamageIndicatorUpdated(RawAlpha);

	if (RawAlpha >= 1.0f)
	{
		FinishMovement();
	}
}

void ADamageIndicatorActor::InitializeDamageIndicator(const FDamageIndicatorPayload& InPayload)
{
	Payload = InPayload;
	bPayloadInitialized = true;
	ElapsedTime = 0.0f;

	SetActorLocation(Payload.StartLocation);

	if (HasActorBegunPlay())
	{
		ApplyPayloadToWidget();
		ReceiveDamageIndicatorInitialized(Payload);
		StartMovement();
	}
}

void ADamageIndicatorActor::ApplyPayloadToWidget()
{
	if (!DamageWidget)
	{
		return;
	}

	DamageWidget->InitWidget();
	UUserWidget* UserWidget = DamageWidget->GetUserWidgetObject();
	if (!UserWidget)
	{
		return;
	}

	SetNumericProperty(UserWidget, TEXT("DamageAmount"), Payload.DamageAmount);
	SetNumericProperty(UserWidget, TEXT("Damage"), Payload.DamageAmount);
	SetBoolProperty(UserWidget, TEXT("bCriticalHit"), Payload.bCriticalHit);
}

void ADamageIndicatorActor::StartMovement()
{
	UpdateFacing();

	if (Duration <= 0.0f)
	{
		FinishMovement();
		return;
	}

	bMovementActive = true;
	SetActorTickEnabled(true);
}

void ADamageIndicatorActor::FinishMovement()
{
	if (!bMovementActive && Duration > 0.0f)
	{
		return;
	}

	bMovementActive = false;
	SetActorTickEnabled(false);
	ReceiveDamageIndicatorFinished();

	if (bDestroyOnFinish)
	{
		Destroy();
	}
}

void ADamageIndicatorActor::UpdateFacing()
{
	if (!bFaceLocalCamera || !DamageWidget || DamageWidget->GetWidgetSpace() != EWidgetSpace::World)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FRotator FacingRotation = (CameraLocation - DamageWidget->GetComponentLocation()).Rotation();
	DamageWidget->SetWorldRotation(FacingRotation);
}
