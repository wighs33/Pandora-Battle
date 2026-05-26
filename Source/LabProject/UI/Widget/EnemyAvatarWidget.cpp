#include "UI/Widget/EnemyAvatarWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widget/EnemyHealthBarWidget.h"
#include "UI/Widget/EnemyShieldBarWidget.h"
#include "UI/Widget/StatusEffectsBarWidget.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyAvatarWidget)

DEFINE_LOG_CATEGORY_STATIC(LogEnemyAvatarWidget, Log, All);

namespace
{
	void SetObjectPropertyValue(UObject* Object, const FName PropertyName, UObject* Value)
	{
		if (!IsValid(Object))
		{
			return;
		}

		if (FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName))
		{
			ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}
}

void UEnemyAvatarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PropagateOwnerActorToChildren();
}

void UEnemyAvatarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateWidgetSize();
}

void UEnemyAvatarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor.Get() == InOwnerActor)
	{
		PropagateOwnerActorToChildren();
		return;
	}

	OwnerActor = InOwnerActor;
	PropagateOwnerActorToChildren();
}

void UEnemyAvatarWidget::UpdateWidgetSize()
{
	const APawn* LocalPlayerPawn = ResolveLocalPlayerPawn();
	if (!OwnerActor.Get() || !LocalPlayerPawn)
	{
		return;
	}

	if (FMath::IsNearlyEqual(MinDistance, MaxDistance))
	{
		SetRenderScale(FVector2D(MaxRenderScale));
		return;
	}

	const float DistanceToPlayer = OwnerActor.Get()->GetDistanceTo(LocalPlayerPawn);
	const float Alpha = FMath::Clamp((DistanceToPlayer - MinDistance) / (MaxDistance - MinDistance), 0.0f, 1.0f);
	const float RenderScale = FMath::Lerp(MaxRenderScale, MinRenderScale, Alpha);

	SetRenderScale(FVector2D(RenderScale));
}

void UEnemyAvatarWidget::PropagateOwnerActorToChildren()
{
	UUserWidget* HealthBarWidget = FindFirstChildUserWidget({
		TEXT("EnemyHealthBar"),
		TEXT("W_EnemyHealthBar"),
		TEXT("WBP_EnemyHealthBar")
	});
	UE_LOG(LogEnemyAvatarWidget, Log, TEXT("PropagateOwnerActorToChildren: avatar=%s owner=%s healthWidget=%s class=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		*GetNameSafe(HealthBarWidget),
		*GetNameSafe(HealthBarWidget ? HealthBarWidget->GetClass() : nullptr));
	if (UEnemyHealthBarWidget* EnemyHealthBar = Cast<UEnemyHealthBarWidget>(HealthBarWidget))
	{
		EnemyHealthBar->SetOwnerActor(OwnerActor.Get());
	}
	else if (HealthBarWidget)
	{
		SetObjectPropertyValue(HealthBarWidget, TEXT("OwnerActor"), OwnerActor.Get());
	}

	UUserWidget* ShieldBarWidget = FindFirstChildUserWidget({
		TEXT("EnemyShieldBar"),
		TEXT("W_EnemyShieldBar"),
		TEXT("WBP_EnemyShieldBar"),
		TEXT("W_EnemyArmorBar"),
		TEXT("WBP_EnemyArmorBar")
	});
	UE_LOG(LogEnemyAvatarWidget, Log, TEXT("PropagateOwnerActorToChildren: avatar=%s owner=%s shieldWidget=%s class=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		*GetNameSafe(ShieldBarWidget),
		*GetNameSafe(ShieldBarWidget ? ShieldBarWidget->GetClass() : nullptr));
	if (UEnemyShieldBarWidget* EnemyShieldBar = Cast<UEnemyShieldBarWidget>(ShieldBarWidget))
	{
		EnemyShieldBar->SetOwnerActor(OwnerActor.Get());
	}
	else if (ShieldBarWidget)
	{
		SetObjectPropertyValue(ShieldBarWidget, TEXT("OwnerActor"), OwnerActor.Get());
	}

	if (UUserWidget* StatusEffectsBar = FindFirstChildUserWidget({
		TEXT("StatusEffectsBar"),
		TEXT("W_StatusEffectsBar"),
		TEXT("WBP_StatusEffectsBar")
	}))
	{
		if (UStatusEffectsBarWidget* NativeStatusEffectsBar = Cast<UStatusEffectsBarWidget>(StatusEffectsBar))
		{
			NativeStatusEffectsBar->SetOwnerActor(OwnerActor.Get());
		}
		else
		{
			SetObjectPropertyValue(StatusEffectsBar, TEXT("OwnerActor"), OwnerActor.Get());
		}
	}
}

APawn* UEnemyAvatarWidget::ResolveLocalPlayerPawn() const
{
	if (APawn* OwningPlayerPawn = GetOwningPlayerPawn())
	{
		return OwningPlayerPawn;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		return PlayerController->GetPawn();
	}

	return nullptr;
}

UUserWidget* UEnemyAvatarWidget::FindChildUserWidget(const FName WidgetName) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	return Cast<UUserWidget>(WidgetTree->FindWidget(WidgetName));
}

UUserWidget* UEnemyAvatarWidget::FindFirstChildUserWidget(std::initializer_list<FName> WidgetNames) const
{
	for (const FName WidgetName : WidgetNames)
	{
		if (UUserWidget* FoundWidget = FindChildUserWidget(WidgetName))
		{
			return FoundWidget;
		}
	}

	return nullptr;
}
