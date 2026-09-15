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
} // namespace

// 부여 중인 능력 목록을 콜백에서 다시 변경하지 않도록 다음 틱에 실행하고, 그 전에 ASC가 사라지면 중단한다.
void UAbilityGrantAndInputManager::TryActivateGrantedAbilityNextTick(
	UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle AbilityHandle)
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

// 서버에서 아직 없는 클래스의 능력만 부여한다. 자동 실행 능력은 부여 콜백이 끝난 다음 틱에 활성화를 시도한다.
TArray<FGameplayAbilitySpecHandle> UAbilityGrantAndInputManager::GrantAbilities(UPdAbilitySystemComponent& AbilitySystemComponent,
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, const int32 AbilityLevel)
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
			AbilityClass,
			FMath::Max(AbilityLevel, 1));

		const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
		const bool bAutoActivateWhenGranted = AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();

		const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent.GiveAbility(AbilitySpec);
		if (!GrantedHandle.IsValid())
		{
			continue;
		}

		GrantedHandles.Add(GrantedHandle);

		// GiveAbility는 능력을 등록만 하므로, 회복처럼 입력 없이 동작할 자동 실행 능력은 별도로 활성화를 시도한다.
		// 부여 콜백에서 능력 목록이 다시 변경되지 않도록 다음 틱에 실행한다.
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
		if (AbilitySpec.IsActive() && AbilitySpec.Ability
			&& (AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags)
				|| AbilitySpec.GetDynamicSpecSourceTags().HasAny(AbilityTags)))
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

// 누름은 즉시 전달한다. 키 해제를 사용하는 능력만 입력 대상을 기록한다.
void UAbilityGrantAndInputManager::HandleAbilityInputPressed(UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;

	// 활성화 콜백이 능력 목록을 바꿀 수 있으므로 입력 시점의 핸들만 먼저 수집한다.
	TArray<FGameplayAbilitySpecHandle> Handles;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
			Handles.Add(Spec.Handle);
	}

	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability || Spec->PendingRemove) continue;

		const UPdGameplayAbility* Ability = Cast<UPdGameplayAbility>(Spec->Ability);
		const bool bUsesInputRelease = Ability && Ability->UsesInputRelease(*Spec);
		if (bUsesInputRelease)
		{
			HoldAbilityHandlesByInputTag.FindOrAdd(InputTag).AddUnique(Handle);
			PendingHoldReleases.Remove(Handle);
		}

		Spec->InputPressed = true;
		if (Spec->IsActive())
		{
			// 기본 공격의 콤보 입력과 스킬의 재입력도 즉시 전달한다.
			SendInputToActiveAbility(AbilitySystemComponent, Handle, true);
		}
		else if (!PendingRemoteActivations.Contains(Handle))
		{
			const EGameplayAbilityNetExecutionPolicy::Type Policy = Spec->Ability->GetNetExecutionPolicy();
			if (!AbilitySystemComponent.IsOwnerActorAuthoritative()
				&& Policy == EGameplayAbilityNetExecutionPolicy::ServerInitiated)
			{
				PendingRemoteActivations.Add(Handle);
			}
			if (!AbilitySystemComponent.TryActivateAbility(Handle))
			{
				ClearAbilityInput(Handle);
				if (FGameplayAbilitySpec* FailedSpec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle))
					FailedSpec->InputPressed = false;
			}
		}

		// 해제를 사용하지 않는 능력에는 누름 이벤트만 전달하고 유지 상태는 남기지 않는다.
		if (!bUsesInputRelease)
		{
			if (FGameplayAbilitySpec* CurrentSpec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle))
				CurrentSpec->InputPressed = false;
		}
	}
}

// 현재 슬롯 대신 누를 때 기록한 능력에 해제를 예약하고, 해당 입력 태그의 누름 기록을 제거한다.
void UAbilityGrantAndInputManager::HandleAbilityInputReleased(const FGameplayTag& InputTag)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	HoldAbilityHandlesByInputTag.RemoveAndCopyValue(InputTag, Handles);
	for (const FGameplayAbilitySpecHandle Handle : Handles) PendingHoldReleases.Add(Handle);
}

// 보관한 해제만 전달한다. 활성화 응답이 아직 도착하지 않았다면 다음 프레임에 다시 확인한다.
void UAbilityGrantAndInputManager::ProcessPendingInputReleases(UPdAbilitySystemComponent& AbilitySystemComponent)
{
	const TArray<FGameplayAbilitySpecHandle> Handles = PendingHoldReleases.Array();
	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	for (const FGameplayAbilitySpecHandle Handle : Handles)
	{
		FGameplayAbilitySpec* Spec = AbilitySystemComponent.FindAbilitySpecFromHandle(Handle);
		if (!Spec || !Spec->Ability || Spec->PendingRemove)
		{
			ClearAbilityInput(Handle);
			continue;
		}
		// 앞선 해제 콜백이 다른 능력을 종료했을 수 있다.
		if (!PendingHoldReleases.Contains(Handle)) continue;

		Spec->InputPressed = false;
		if (Spec->IsActive())
		{
			PendingHoldReleases.Remove(Handle);
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
		// Handle과 PredictionKey로 구분한 시전의 입력 이벤트를 발생시켜 WaitInputPress/WaitInputRelease 태스크에 전달한다.
		// 이 호출 자체가 RPC를 보내는 것은 아니며, 필요한 서버 전송은 입력 대기 태스크가 처리한다.
		AbilitySystemComponent.InvokeReplicatedEvent(
			bPressed ? EAbilityGenericReplicatedEvent::InputPressed : EAbilityGenericReplicatedEvent::InputReleased, Handle, PredictionKey);
	}
}

// 종료·실패·회수된 능력의 입력 상태와 모든 입력 태그에 남은 핸들 기록을 함께 제거한다.
void UAbilityGrantAndInputManager::ClearAbilityInput(const FGameplayAbilitySpecHandle Handle)
{
	PendingHoldReleases.Remove(Handle);
	PendingRemoteActivations.Remove(Handle);
	for (auto It = HoldAbilityHandlesByInputTag.CreateIterator(); It; ++It)
	{
		It.Value().Remove(Handle);
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}
