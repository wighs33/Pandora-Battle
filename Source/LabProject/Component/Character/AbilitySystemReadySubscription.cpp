#include "Component/Character/AbilitySystemReadySubscription.h"

#include "Character/CharacterBase.h"
#include "Component/Character/AbilityStateComponent.h"
#include "Mode/PdPlayerController.h"

// 등록을 먼저 끝낸 뒤 현재 준비 상태를 전달해, 콜백 안에서 구독을 다시 바꿔도 핸들이 남지 않게 한다.
void FAbilitySystemReadySubscription::SubscribeToCharacter(
	ACharacterBase* Character, const FPdAbilitySystemReadyDelegate::FDelegate& Delegate,
	const FPdAbilitySystemReadyDelegate::FDelegate& OnReleased)
{
	Reset();

	UAbilityStateComponent* AbilityStateComponent = Character ? Character->GetAbilityStateComponent() : nullptr;
	if (!AbilityStateComponent)
	{
		return;
	}

	CharacterSource = AbilityStateComponent;
	Handle = AbilityStateComponent->RegisterOnAbilitySystemReady(Delegate);
	if (OnReleased.IsBound())
	{
		ReleasedHandle = AbilityStateComponent->RegisterOnAbilitySystemReleased(OnReleased);
	}
	if (UPdAbilitySystemComponent* ReadyAbilitySystem = AbilityStateComponent->GetReadyAbilitySystemComponent())
	{
		Delegate.ExecuteIfBound(Character, ReadyAbilitySystem);
	}
}

// 조종 캐릭터 교체는 컨트롤러가 추적하므로, 구독자는 현재 준비된 캐릭터만 받는다.
void FAbilitySystemReadySubscription::SubscribeToPossessedCharacter(
	APlayerController* PlayerController, const FPdAbilitySystemReadyDelegate::FDelegate& Delegate)
{
	Reset();

	APdPlayerController* PdPlayerController = Cast<APdPlayerController>(PlayerController);
	if (!PdPlayerController)
	{
		return;
	}

	ControllerSource = PdPlayerController;
	Handle = PdPlayerController->RegisterOnPossessedCharacterAbilitySystemReady(Delegate);
	if (ACharacterBase* ReadyCharacter = PdPlayerController->GetReadyPossessedCharacter())
	{
		Delegate.ExecuteIfBound(ReadyCharacter, ReadyCharacter->GetAbilityStateComponent()->GetReadyAbilitySystemComponent());
	}
}

void FAbilitySystemReadySubscription::Reset()
{
	if (UAbilityStateComponent* AbilityStateComponent = CharacterSource.Get())
	{
		AbilityStateComponent->UnregisterOnAbilitySystemReady(Handle);
		AbilityStateComponent->UnregisterOnAbilitySystemReleased(ReleasedHandle);
	}
	if (APdPlayerController* PdPlayerController = ControllerSource.Get())
	{
		PdPlayerController->UnregisterOnPossessedCharacterAbilitySystemReady(Handle);
	}

	CharacterSource.Reset();
	ControllerSource.Reset();
	Handle.Reset();
	ReleasedHandle.Reset();
}
