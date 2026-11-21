#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatUiPool.generated.h"

class ADamageIndicatorActor;
class UUserWidget;
struct FDamageIndicatorPayload;

/**
 * 클라이언트 전투 UI(데미지 숫자, 캐릭터 머리 위 위젯)를 다 쓴 뒤 지우지 않고 모아 두었다가 다시 쓴다.
 *
 * 데미지 숫자는 맞을 때마다 모든 클라이언트에 액터·위젯 컴포넌트·위젯 한 벌씩 뜨고, 머리 위 위젯은 캐릭터마다 한 벌씩 붙는다.
 * 광역 공격이나 졸개 소환처럼 한꺼번에 몰리면 매번 새로 만들고 지우는 대신 쉬는 것을 꺼내 쓴다.
 * 쉬는 것은 종류마다 상한까지만 두고, 그보다 많이 몰린 순간에 만든 것은 다 쓰면 지운다. 전용 서버는 그리지 않으므로 만들지 않는다.
 */
UCLASS()
class LABPROJECT_API UCombatUiPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 같은 클래스의 쉬는 데미지 숫자를 꺼내 Payload로 띄우고, 없으면 새로 만든다. */
	void ShowDamageIndicator(TSubclassOf<ADamageIndicatorActor> IndicatorClass, const FDamageIndicatorPayload& Payload);

	/** 움직임을 마친 데미지 숫자를 다음 숫자에 쓰도록 받는다. */
	void ReleaseDamageIndicator(ADamageIndicatorActor& Indicator);

	/** 같은 클래스의 쉬는 머리 위 위젯을 꺼내고, 없으면 새로 만든다. */
	UUserWidget* AcquireOverheadWidget(TSubclassOf<UUserWidget> WidgetClass);

	/** 캐릭터에서 뗀 머리 위 위젯을 다음 캐릭터에 쓰도록 받는다. */
	void ReleaseOverheadWidget(UUserWidget& Widget);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADamageIndicatorActor>> IdleDamageIndicators;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> IdleOverheadWidgets;
};
