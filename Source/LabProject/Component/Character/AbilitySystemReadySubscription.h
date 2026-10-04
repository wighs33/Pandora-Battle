#pragma once

#include "CoreMinimal.h"

class ACharacterBase;
class APdPlayerController;
class APlayerController;
class UAbilityStateComponent;
class UPdAbilitySystemComponent;

/**
 * 캐릭터의 ASC가 이 캐릭터를 Avatar로 연결했을 때 보내는 준비 알림.
 * 기본 속성은 ASC 소유 액터(PlayerState·적)의 기본 서브오브젝트라 같은 시점에 함께 준비된다.
 * 빙의·PlayerState 도착·리스폰으로 다시 연결될 때마다 다시 보낸다.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FPdAbilitySystemReadyDelegate,
	ACharacterBase* /*Character*/, UPdAbilitySystemComponent* /*AbilitySystemComponent*/);

/**
 * ASC 준비 알림의 구독 하나를 보관한다.
 *
 * 대상이 이미 준비되어 있으면 구독 즉시 한 번 호출하고, 이후 다시 준비될 때마다 호출한다.
 * 위젯·컴포넌트는 준비 여부를 타이머로 다시 확인하지 않고 이 알림에서만 ASC에 연결한다.
 * 캐릭터 구독은 ASC에서 빠질 때의 알림도 함께 받을 수 있다. 소유자는 종료 시 Reset으로 구독을 해제한다.
 */
class LABPROJECT_API FAbilitySystemReadySubscription final
{
public:
	FAbilitySystemReadySubscription() = default;
	FAbilitySystemReadySubscription(const FAbilitySystemReadySubscription&) = delete;
	FAbilitySystemReadySubscription& operator=(const FAbilitySystemReadySubscription&) = delete;

	/** 지정한 캐릭터의 ASC 준비를 구독한다. OnReleased를 주면 캐릭터가 그 ASC에서 빠질 때도 알린다. */
	void SubscribeToCharacter(ACharacterBase* Character, const FPdAbilitySystemReadyDelegate::FDelegate& Delegate,
		const FPdAbilitySystemReadyDelegate::FDelegate& OnReleased = FPdAbilitySystemReadyDelegate::FDelegate());

	/** 플레이어가 조종하는 캐릭터의 ASC 준비를 구독한다. Pawn이 바뀌면 새 캐릭터가 준비될 때 다시 알린다. */
	void SubscribeToPossessedCharacter(APlayerController* PlayerController, const FPdAbilitySystemReadyDelegate::FDelegate& Delegate);

	void Reset();

	/** 캐릭터 구독에서 그 캐릭터의 ASC가 지금 준비돼 있으면 돌려준다. 준비 전·해제 뒤에는 nullptr이다. */
	UPdAbilitySystemComponent* GetReadyAbilitySystem() const;

private:
	TWeakObjectPtr<UAbilityStateComponent> CharacterSource;
	TWeakObjectPtr<APdPlayerController> ControllerSource;
	FDelegateHandle Handle;
	FDelegateHandle ReleasedHandle;
};
