#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityGrantAndInputManager)

namespace
{
// 활성화 예측 키로 콜백 전후가 같은 시전인지 구분한다.
FPredictionKey ResolveAbilityInputPredictionKey(const FGameplayAbilitySpec& AbilitySpec)
{
	const UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
	return AbilityInstance ? AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}

// 부여 중인 능력 목록을 콜백에서 다시 변경하지 않도록 다음 틱에 실행하고, 그 전에 ASC가 사라지면 중단한다.
void TryActivateGrantedAbilityNextTick(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle AbilityHandle)
{
	if (!AbilitySystemComponent || !AbilityHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = AbilitySystemComponent->GetWorld())
	{
		const TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystemComponent = AbilitySystemComponent;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakAbilitySystemComponent, AbilityHandle]() {
			if (UAbilitySystemComponent* ResolvedAbilitySystemComponent = WeakAbilitySystemComponent.Get())
			{
				ResolvedAbilitySystemComponent->TryActivateAbility(AbilityHandle);
			}
		}));
		return;
	}

	AbilitySystemComponent->TryActivateAbility(AbilityHandle);
}
} // namespace

// 서버에서 아직 없는 클래스의 능력만 부여한다. 자동 실행 능력은 부여 콜백이 끝난 다음 틱에 활성화를 시도한다.
TArray<FGameplayAbilitySpecHandle> UAbilityGrantAndInputManager::GrantAbilities(UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, const int32 AbilityLevel, UObject* SourceObject)
{
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || AbilityClasses.IsEmpty())
	{
		return GrantedHandles;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		if (!AbilityClass || AbilitySystemComponent.FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(
			AbilityClass, FMath::Max(AbilityLevel, 1), INDEX_NONE, SourceObject ? SourceObject : AbilitySystemComponent.GetAvatarActor());

		const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
		const bool bAutoActivateWhenGranted = AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();

		const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent.GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			continue;
		}

		GrantedHandles.Add(GrantedHandle);
		if (bAutoActivateWhenGranted)
		{
			TryActivateGrantedAbilityNextTick(&AbilitySystemComponent, GrantedHandle);
		}
	}

	return GrantedHandles;
}

// 서버에서 전달받은 핸들의 능력을 회수한다. 입력 정리는 ASC의 OnRemoveAbility 콜백이 처리한다.
void UAbilityGrantAndInputManager::RemoveAbilities(
	UPdAbilitySystemComponent& AbilitySystemComponent, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || AbilityHandles.IsEmpty())
	{
		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		if (AbilityHandle.IsValid())
		{
			AbilitySystemComponent.ClearAbility(AbilityHandle);
		}
	}
}

// 살아 있는 서버 캐릭터의 비활성 자동 실행 능력을 모아 다시 활성화하도록 예약한다.
void UAbilityGrantAndInputManager::ReactivateAutoActivatedAbilities(UPdAbilitySystemComponent& AbilitySystemComponent) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || AbilitySystemComponent.HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	// 활성화 콜백이 능력 목록을 바꿀 수 있어, 잠금 안에서 핸들만 수집하고 실행은 잠금 밖에서 예약한다.
	TArray<FGameplayAbilitySpecHandle> AbilityHandlesToActivate;
	{
		FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
		for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
		{
			const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilitySpec.Ability);
			if (AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted() && !AbilitySpec.IsActive())
			{
				AbilityHandlesToActivate.Add(AbilitySpec.Handle);
			}
		}
	}

	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandlesToActivate)
	{
		TryActivateGrantedAbilityNextTick(&AbilitySystemComponent, AbilityHandle);
	}
}

// 지정 태그 중 하나를 가진 실행 중 능력의 Spec을 반환한다. 부여만 된 능력은 제외한다.
const FGameplayAbilitySpec* UAbilityGrantAndInputManager::FindActiveAbilitySpecByTags(
	const UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const
{
	if (AbilityTags.IsEmpty())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (AbilitySpec.IsActive() && AbilitySpec.Ability && AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return &AbilitySpec;
		}
	}

	return nullptr;
}

// 해당 클래스의 능력이 실행 중인지 검사한다. 필요하면 파생 클래스까지 포함한다.
bool UAbilityGrantAndInputManager::HasActiveAbilityOfClass(const UPdAbilitySystemComponent& AbilitySystemComponent,
	const TSubclassOf<UGameplayAbility> AbilityClass, const bool bIncludeChildClasses) const
{
	if (!AbilityClass)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability)
		{
			continue;
		}

		const UClass* SpecAbilityClass = AbilitySpec.Ability->GetClass();
		if (SpecAbilityClass && (SpecAbilityClass == AbilityClass || (bIncludeChildClasses && SpecAbilityClass->IsChildOf(AbilityClass))))
		{
			return true;
		}
	}

	return false;
}

// 여러 능력 클래스 중 하나라도 실행 중인지 같은 클래스 판정 규칙으로 검사한다.
bool UAbilityGrantAndInputManager::HasActiveAbilityOfAnyClass(const UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, const bool bIncludeChildClasses) const
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		if (HasActiveAbilityOfClass(AbilitySystemComponent, AbilityClass, bIncludeChildClasses))
		{
			return true;
		}
	}

	return false;
}

// 입력 태그에 연결된 능력의 누름을 기록한다. 실제 활성화는 프레임 입력 처리 단계에서 수행한다.
void UAbilityGrantAndInputManager::QueueAbilityInputPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		// 키를 누른 시점의 능력을 기억해, 슬롯 교체 후에도 같은 능력에 해제를 전달한다.
		AbilityHandlesByPressedInputTag.FindOrAdd(InputTag).AddUnique(Spec.Handle);
		FAbilityInputState& Input = AbilityInputStates.FindOrAdd(Spec.Handle);
		Input.bPressPending = true;
		Input.bReleasePending = false;
	}
}

// 현재 슬롯 대신 누를 때 기록한 능력에 해제를 예약하고, 해당 입력 태그의 누름 기록을 제거한다.
void UAbilityGrantAndInputManager::QueueAbilityInputReleased(const FGameplayTag& InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	AbilityHandlesByPressedInputTag.RemoveAndCopyValue(InputTag, Handles);
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		if (FAbilityInputState* Input = AbilityInputStates.Find(Handle))
		{
			Input->bReleasePending = true;
		}
	}
}

// 입력 상태의 스냅샷을 따라 누름·활성화 요청·해제를 처리한다. 아직 도착하지 않은 활성화 응답은 기다린다.
void UAbilityGrantAndInputManager::ProcessAbilityInput(UPdAbilitySystemComponent& AbilitySystemComponent)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	AbilityInputStates.GetKeys(Handles);
	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
		FAbilityInputState* Input = AbilityInputStates.Find(Handle);
		if (!Spec || !Spec->Ability)
		{
			ClearAbilityInput(Handle);
			continue;
		}
		if (!Input)
		{
			continue;
		}

		// 1. 누름을 전달하거나, 아직 보내지 않은 활성화 요청을 한 번 보낸다.
		if (Input->bPressPending)
		{
			Input->bPressPending = false;
			Spec->InputPressed = true;
			if (Spec->IsActive())
			{
				SendInputToActiveAbility(AbilitySystemComponent, Handle, true);
			}
			else if (!Input->bActivationRequestSent)
			{
				Input->bActivationRequestSent = true;
				if (!AbilitySystemComponent.TryActivateAbility(Handle))
				{
					Spec->InputPressed = false;
					ClearAbilityInput(Handle);
				}
			}
		}

		// 2. 콜백에서 종료·삭제될 수 있으므로 포인터를 다시 찾는다. TMap 참조를 콜백 너머로 보관하지 않는다.
		Input = AbilityInputStates.Find(Handle);
		Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
		if (!Input || !Spec || !Input->bReleasePending)
		{
			continue;
		}

		// 3. 활성화가 확인된 시전에만 해제를 전달한다. 서버 응답 대기 중이면 다음 프레임에 다시 확인한다.
		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			Input->bReleasePending = false;
			SendInputToActiveAbility(AbilitySystemComponent, Handle, false);
		}
	}
}

// 실행 중인 같은 시전에 타기팅 확정·입력 콜백·GAS 복제 이벤트를 순서대로 전달한다.
void UAbilityGrantAndInputManager::SendInputToActiveAbility(
	UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayAbilitySpecHandle Handle, const bool bPressed)
{
	FGameplayAbilitySpec* Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->IsActive())
	{
		return;
	}

	const FPredictionKey PredictionKey = ResolveAbilityInputPredictionKey(*Spec);
	if (!bPressed)
	{
		const UPdGameplayAbility* Ability = Cast<UPdGameplayAbility>(Spec->GetPrimaryInstance());
		if (Ability && Ability->ShouldConfirmTargetingOnInputRelease())
		{
			AbilitySystemComponent.LocalInputConfirm();
		}
	}

	// 타기팅 확정이나 InputReleased가 시전을 끝낼 수 있으므로 같은 시전에만 전달한다.
	Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->IsActive() || ResolveAbilityInputPredictionKey(*Spec) != PredictionKey)
	{
		return;
	}

	if (bPressed)
	{
		AbilitySystemComponent.AbilitySpecInputPressed(*Spec);
	}
	else
	{
		AbilitySystemComponent.AbilitySpecInputReleased(*Spec);
	}

	Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
	if (Spec && Spec->IsActive() && ResolveAbilityInputPredictionKey(*Spec) == PredictionKey)
	{
		AbilitySystemComponent.InvokeReplicatedEvent(
			bPressed ? EAbilityGenericReplicatedEvent::InputPressed : EAbilityGenericReplicatedEvent::InputReleased, Handle, PredictionKey);
	}
}

// 종료·실패·회수된 능력의 입력 상태와 모든 입력 태그에 남은 핸들 기록을 함께 제거한다.
void UAbilityGrantAndInputManager::ClearAbilityInput(const FGameplayAbilitySpecHandle Handle)
{
	AbilityInputStates.Remove(Handle);
	for (auto It = AbilityHandlesByPressedInputTag.CreateIterator(); It; ++It)
	{
		It.Value().Remove(Handle);
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}
