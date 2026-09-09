#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityGrantAndInputManager)

namespace
{
FPredictionKey ResolveAbilityInputPredictionKey(const FGameplayAbilitySpec& AbilitySpec)
{
	const UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
	return AbilityInstance
		? AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey()
		: FPredictionKey();
}

void TryActivateGrantedAbilityNextTick(
	UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayAbilitySpecHandle AbilityHandle)
{
	if (!AbilitySystemComponent || !AbilityHandle.IsValid())
	{
		return;
	}

	if (UWorld* World = AbilitySystemComponent->GetWorld())
	{
		const TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystemComponent =
			AbilitySystemComponent;
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateLambda([WeakAbilitySystemComponent, AbilityHandle]()
			{
				if (UAbilitySystemComponent* ResolvedAbilitySystemComponent =
					WeakAbilitySystemComponent.Get())
				{
					ResolvedAbilitySystemComponent->TryActivateAbility(AbilityHandle);
				}
			}));
		return;
	}

	AbilitySystemComponent->TryActivateAbility(AbilityHandle);
}
}

void UAbilityGrantAndInputManager::QueueAbilityInputPressed(
	UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// 키를 누른 시점의 능력을 기억해, 슬롯 교체 후에도 같은 능력에 해제를 전달한다.
			PressedAbilityHandles.FindOrAdd(InputTag).AddUnique(Spec.Handle);
			FAbilityInputState& Input = AbilityInputStates.FindOrAdd(Spec.Handle);
			Input.bPressPending = true;
			Input.bReleasePending = false;
		}
	}
}

void UAbilityGrantAndInputManager::QueueAbilityInputReleased(const FGameplayTag& InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	PressedAbilityHandles.RemoveAndCopyValue(InputTag, Handles);
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		if (FAbilityInputState* Input = AbilityInputStates.Find(Handle))
		{
			Input->bReleasePending = true;
		}
	}
}

// 모든 입력 유형은 누름 → 활성화 요청 → 해제 순서로 처리한다.
// 서버 응답을 기다리는 해제도 이 경로에 남으므로, 별도 타이머나 Press 전용 재전달이 필요 없다.
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

		if (Input->bPressPending)
		{
			Input->bPressPending = false;
			Spec->InputPressed = true;
			if (Spec->IsActive())
			{
				SendInputToActiveAbility(AbilitySystemComponent, Handle, true);
			}
			else if (!Input->bActivationRequested)
			{
				Input->bActivationRequested = true;
				if (!AbilitySystemComponent.TryActivateAbility(Handle))
				{
					Spec->InputPressed = false;
					ClearAbilityInput(Handle);
				}
			}
		}

		// 활성화·입력 콜백이 능력을 종료하거나 입력 상태를 지울 수 있다.
		Input = AbilityInputStates.Find(Handle);
		Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
		if (!Input || !Spec || !Input->bReleasePending)
		{
			continue;
		}

		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			Input->bReleasePending = false;
			SendInputToActiveAbility(AbilitySystemComponent, Handle, false);
		}
	}
}

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
			bPressed ? EAbilityGenericReplicatedEvent::InputPressed : EAbilityGenericReplicatedEvent::InputReleased,
			Handle, PredictionKey);
	}
}

void UAbilityGrantAndInputManager::ClearAbilityInput(const FGameplayAbilitySpecHandle Handle)
{
	AbilityInputStates.Remove(Handle);
	for (auto It = PressedAbilityHandles.CreateIterator(); It; ++It)
	{
		It.Value().Remove(Handle);
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

const FGameplayAbilitySpec* UAbilityGrantAndInputManager::FindActiveAbilitySpecByTags(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	if (AbilityTags.IsEmpty())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (AbilitySpec.IsActive()
			&& AbilitySpec.Ability
			&& AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return &AbilitySpec;
		}
	}

	return nullptr;
}

bool UAbilityGrantAndInputManager::HasActiveAbilityOfClass(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	const TSubclassOf<UGameplayAbility> AbilityClass,
	const bool bIncludeChildClasses) const
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
		if (SpecAbilityClass
			&& (SpecAbilityClass == AbilityClass
				|| (bIncludeChildClasses && SpecAbilityClass->IsChildOf(AbilityClass))))
		{
			return true;
		}
	}

	return false;
}

bool UAbilityGrantAndInputManager::HasActiveAbilityOfAnyClass(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const bool bIncludeChildClasses) const
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		if (HasActiveAbilityOfClass(
			AbilitySystemComponent,
			AbilityClass,
			bIncludeChildClasses))
		{
			return true;
		}
	}

	return false;
}

TArray<FGameplayAbilitySpecHandle> UAbilityGrantAndInputManager::GrantAbilities(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const int32 AbilityLevel,
	UObject* SourceObject)
{
	TArray<FGameplayAbilitySpecHandle> GrantedHandles;
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || AbilityClasses.IsEmpty())
	{
		return GrantedHandles;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		if (!AbilityClass
			|| AbilitySystemComponent.FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(
			AbilityClass,
			FMath::Max(AbilityLevel, 1),
			INDEX_NONE,
			SourceObject ? SourceObject : AbilitySystemComponent.GetAvatarActor());

		const UPdGameplayAbility* AbilityCDO =
			Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
		const bool bAutoActivateWhenGranted =
			AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();

		const FGameplayAbilitySpecHandle GrantedHandle =
			AbilitySystemComponent.GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			continue;
		}

		GrantedHandles.Add(GrantedHandle);
		if (bAutoActivateWhenGranted)
		{
			TryActivateGrantedAbilityNextTick(
				&AbilitySystemComponent,
				GrantedHandle);
		}
	}

	return GrantedHandles;
}

void UAbilityGrantAndInputManager::RemoveAbilities(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles) const
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

void UAbilityGrantAndInputManager::ReactivateAutoActivatedAbilities(
	UPdAbilitySystemComponent& AbilitySystemComponent) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative()
		|| AbilitySystemComponent.HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandlesToActivate;
	{
		FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
		for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
		{
			const UPdGameplayAbility* AbilityCDO =
				Cast<UPdGameplayAbility>(AbilitySpec.Ability);
			if (AbilityCDO
				&& AbilityCDO->ShouldAutoActivateWhenGranted()
				&& !AbilitySpec.IsActive())
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
