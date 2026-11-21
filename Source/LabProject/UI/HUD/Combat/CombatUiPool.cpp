#include "UI/HUD/Combat/CombatUiPool.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "UI/HUD/Combat/DamageIndicatorActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombatUiPool)

namespace
{
	/** 쉬게 둘 데미지 숫자 수. 숫자 하나가 0.8초 떠 있으므로, 광역 공격 몇 번이 겹친 순간에 뜨는 수보다 넉넉하게 둔다. */
	constexpr int32 MaxIdleDamageIndicators = 128;

	/** 쉬게 둘 머리 위 위젯 수. 졸개 한 무리가 죽고 다음 무리가 나올 때 쓸 만큼 둔다. */
	constexpr int32 MaxIdleOverheadWidgets = 64;

	/** 같은 클래스의 쉬는 객체를 하나 꺼낸다. 없으면 nullptr. */
	template <typename ObjectType>
	ObjectType* TakeIdleOfClass(TArray<TObjectPtr<ObjectType>>& Idle, const UClass* Class)
	{
		const int32 Index = Idle.FindLastByPredicate([Class](const ObjectType* Object)
		{
			return IsValid(Object) && Object->GetClass() == Class;
		});
		if (Index == INDEX_NONE)
		{
			return nullptr;
		}
		ObjectType* Object = Idle[Index];
		Idle.RemoveAtSwap(Index);
		return Object;
	}
}

bool UCombatUiPool::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UCombatUiPool::Deinitialize()
{
	IdleDamageIndicators.Reset();
	IdleOverheadWidgets.Reset();
	Super::Deinitialize();
}

void UCombatUiPool::ShowDamageIndicator(const TSubclassOf<ADamageIndicatorActor> IndicatorClass, const FDamageIndicatorPayload& Payload)
{
	if (ADamageIndicatorActor* Indicator = TakeIdleOfClass(IdleDamageIndicators, IndicatorClass.Get()))
	{
		Indicator->InitializeDamageIndicator(Payload);
		return;
	}

	UWorld* World = GetWorld();
	const FTransform SpawnTransform(Payload.StartLocation);
	ADamageIndicatorActor* Indicator = World ? World->SpawnActorDeferred<ADamageIndicatorActor>(IndicatorClass, SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
	if (!Indicator)
	{
		return;
	}
	Indicator->InitializeDamageIndicator(Payload);
	Indicator->FinishSpawning(SpawnTransform);
}

void UCombatUiPool::ReleaseDamageIndicator(ADamageIndicatorActor& Indicator)
{
	if (IdleDamageIndicators.Num() >= MaxIdleDamageIndicators)
	{
		Indicator.Destroy();
		return;
	}
	IdleDamageIndicators.Add(&Indicator);
}

UUserWidget* UCombatUiPool::AcquireOverheadWidget(const TSubclassOf<UUserWidget> WidgetClass)
{
	if (UUserWidget* Widget = TakeIdleOfClass(IdleOverheadWidgets, WidgetClass.Get()))
	{
		return Widget;
	}
	return WidgetClass ? CreateWidget(GetWorld(), WidgetClass) : nullptr;
}

void UCombatUiPool::ReleaseOverheadWidget(UUserWidget& Widget)
{
	if (IdleOverheadWidgets.Num() < MaxIdleOverheadWidgets)
	{
		IdleOverheadWidgets.Add(&Widget);
	}
}
