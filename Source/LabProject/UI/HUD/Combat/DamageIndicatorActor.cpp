#include "UI/HUD/Combat/DamageIndicatorActor.h"

#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/HUD/Combat/CombatUiPool.h"
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

	/** 내 캐릭터가 맞은 숫자의 글자 테두리 색. */
	const FLinearColor LocalPlayerDamageOutlineColor(0.1f, 0.0f, 0.05f, 1.0f);

	bool IsDamageForLocalPlayer(const AActor* DamagedActor)
	{
		const APawn* DamagedPawn = Cast<APawn>(DamagedActor);
		const APlayerController* PlayerController =
			DamagedPawn ? Cast<APlayerController>(DamagedPawn->GetController()) : nullptr;
		return PlayerController && PlayerController->IsLocalController();
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

	// 풀에서 다시 꺼낸 액터는 이미 BeginPlay를 지났으므로 여기서 다시 띄운다.
	if (HasActorBegunPlay())
	{
		SetPresented(true);
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

	// 다시 쓰는 위젯은 지난 숫자의 등장 애니메이션 상태가 남아 있을 수 있어, BP가 애니메이션을 처음부터 다시 틀도록 멈춰 둔다.
	UserWidget->StopAllAnimations();
	SetNumericProperty(UserWidget, TEXT("Damage"), Payload.DamageAmount);
	SetBoolProperty(UserWidget, TEXT("bCriticalHit"), Payload.bCriticalHit);
	ApplyDamageOutline(*UserWidget);
}

// 내 캐릭터가 맞은 숫자만 글자 테두리 색을 바꾸고, 다른 숫자는 원래 색으로 둔다. 위젯을 다시 쓰므로 매번 둘 중 하나로 정한다.
void ADamageIndicatorActor::ApplyDamageOutline(UUserWidget& UserWidget)
{
	UTextBlock* DamageText = Cast<UTextBlock>(UserWidget.GetWidgetFromName(TEXT("DamageText")));
	if (!DamageText)
	{
		return;
	}

	FSlateFontInfo FontInfo = DamageText->GetFont();
	if (!DefaultDamageOutlineColor.IsSet())
	{
		DefaultDamageOutlineColor = FontInfo.OutlineSettings.OutlineColor;
	}
	FontInfo.OutlineSettings.OutlineColor = IsDamageForLocalPlayer(Payload.DamagedActor)
		? LocalPlayerDamageOutlineColor
		: DefaultDamageOutlineColor.GetValue();
	DamageText->SetFont(FontInfo);
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
	SetPresented(false);
	Payload = FDamageIndicatorPayload();

	if (UCombatUiPool* Pool = UWorld::GetSubsystem<UCombatUiPool>(GetWorld()))
	{
		Pool->ReleaseDamageIndicator(*this);
		return;
	}
	Destroy();
}

// 쉬는 동안은 숨기고 위젯 컴포넌트도 멈춰, 월드 공간 위젯을 다시 그리지 않게 한다.
void ADamageIndicatorActor::SetPresented(const bool bPresented)
{
	SetActorHiddenInGame(!bPresented);
	if (DamageWidget)
	{
		DamageWidget->SetHiddenInGame(!bPresented);
		DamageWidget->SetComponentTickEnabled(bPresented);
	}
}

void ADamageIndicatorActor::UpdateFacing()
{
	if (!bFaceLocalCamera || !DamageWidget || DamageWidget->GetWidgetSpace() != EWidgetSpace::World)
	{
		return;
	}

	const APlayerController* PlayerController = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!PlayerController)
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FRotator FacingRotation = (CameraLocation - DamageWidget->GetComponentLocation()).Rotation();
	DamageWidget->SetWorldRotation(FacingRotation);
}
