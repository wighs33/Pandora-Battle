#include "Component/AbilitySystem/AbilityCollectionRuntime.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityCollectionRuntime)

namespace
{
FPredictionKey ResolveAbilityInputPredictionKey(const FGameplayAbilitySpec& AbilitySpec)
{
	const UGameplayAbility* AbilityInstance = AbilitySpec.GetPrimaryInstance();
	return AbilityInstance
		? AbilityInstance->GetCurrentActivationInfo().GetActivationPredictionKey()
		: FPredictionKey();
}

bool ShouldAutoConfirmOnInputRelease(const FGameplayAbilitySpec& AbilitySpec)
{
	const UGameplayAbility* Ability = AbilitySpec.GetPrimaryInstance();
	if (!Ability)
	{
		Ability = AbilitySpec.Ability;
	}

	const UPdGameplayAbility* PdAbility = Cast<UPdGameplayAbility>(Ability);
	return !PdAbility || PdAbility->ShouldAutoConfirmOnInputRelease();
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

void UAbilityCollectionRuntime::AbilityInputTagPressed(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return;
	}

	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability
			|| !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.InputPressed = true;
		if (AbilitySpec.IsActive())
		{
			AbilitySystemComponent.AbilitySpecInputPressed(AbilitySpec);
			AbilitySystemComponent.InvokeReplicatedEvent(
				EAbilityGenericReplicatedEvent::InputPressed,
				AbilitySpec.Handle,
				ResolveAbilityInputPredictionKey(AbilitySpec));
			continue;
		}

		if (!AbilitySpec.Ability->CanActivateAbility(
			AbilitySpec.Handle,
			AbilitySystemComponent.AbilityActorInfo.Get(),
			nullptr,
			nullptr,
			nullptr))
		{
			AbilitySpec.InputPressed = false;
			continue;
		}

		AbilitySystemComponent.TryActivateAbility(AbilitySpec.Handle);
	}
}

void UAbilityCollectionRuntime::AbilityInputTagReleased(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid())
	{
		return;
	}

	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability
			|| !AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			continue;
		}

		AbilitySpec.InputPressed = false;
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		// Eligible press skills use release as their cast confirmation. Staged
		// abilities such as projectiles opt out and wait for the separate primary
		// attack/target-confirm input instead.
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(
			LabGameplayTags::Skill_Type_Press)
			&& ShouldAutoConfirmOnInputRelease(AbilitySpec))
		{
			AbilitySystemComponent.LocalInputConfirm();
			if (!AbilitySpec.IsActive())
			{
				continue;
			}
		}

		AbilitySystemComponent.AbilitySpecInputReleased(AbilitySpec);
		AbilitySystemComponent.InvokeReplicatedEvent(
			EAbilityGenericReplicatedEvent::InputReleased,
			AbilitySpec.Handle,
			ResolveAbilityInputPredictionKey(AbilitySpec));
	}
}

void UAbilityCollectionRuntime::ReplayReleasedPressInputAfterActivation(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayAbilitySpecHandle AbilityHandle) const
{
	FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
	FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent.FindAbilitySpecFromHandle(AbilityHandle);
	if (!AbilitySpec
		|| AbilitySpec->InputPressed
		|| !AbilitySpec->IsActive()
		|| !AbilitySpec->Ability
		|| !AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(
			LabGameplayTags::Skill_Type_Press)
		|| !ShouldAutoConfirmOnInputRelease(*AbilitySpec))
	{
		return;
	}

	// A ServerInitiated ability may become active on the owning client after a
	// very short key press has already ended. Replay that release after the
	// ability tasks have bound their confirm callbacks so network latency cannot
	// strand the cast in its targeting phase.
	AbilitySystemComponent.LocalInputConfirm();
	if (!AbilitySpec->IsActive())
	{
		return;
	}

	AbilitySystemComponent.AbilitySpecInputReleased(*AbilitySpec);
	AbilitySystemComponent.InvokeReplicatedEvent(
		EAbilityGenericReplicatedEvent::InputReleased,
		AbilitySpec->Handle,
		ResolveAbilityInputPredictionKey(*AbilitySpec));
}

const FGameplayAbilitySpec* UAbilityCollectionRuntime::FindActiveAbilitySpecByTags(
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

bool UAbilityCollectionRuntime::HasActiveAbilityOfClass(
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

bool UAbilityCollectionRuntime::HasActiveAbilityOfAnyClass(
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

TArray<FGameplayAbilitySpecHandle> UAbilityCollectionRuntime::GrantAbilities(
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
			|| HasGrantedAbilityClass(AbilitySystemComponent, AbilityClass))
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

void UAbilityCollectionRuntime::RemoveAbilities(
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

void UAbilityCollectionRuntime::ReactivateAutoActivatedAbilities(
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

void UAbilityCollectionRuntime::CachePandoraSkillRuntimeContext(UObject* SourceObject)
{
	if (UPandoraSkillRuntimeContext* RuntimeContext =
		Cast<UPandoraSkillRuntimeContext>(SourceObject))
	{
		GrantedPandoraSkillRuntimeContexts.AddUnique(RuntimeContext);
	}
}

void UAbilityCollectionRuntime::ReleasePandoraSkillRuntimeContextIfUnused(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	UPandoraSkillRuntimeContext* RuntimeContext,
	const FGameplayAbilitySpecHandle RemovedHandle)
{
	if (!RuntimeContext)
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (AbilitySpec.Handle != RemovedHandle
			&& AbilitySpec.SourceObject.Get() == RuntimeContext)
		{
			return;
		}
	}

	GrantedPandoraSkillRuntimeContexts.Remove(RuntimeContext);
}

bool UAbilityCollectionRuntime::IsPandoraAbilitySpec(
	const FGameplayAbilitySpec& AbilitySpec) const
{
	if (Cast<UPandoraSkillRuntimeContext>(AbilitySpec.SourceObject.Get()))
	{
		return true;
	}

	const FGameplayTagContainer& SourceTags = AbilitySpec.GetDynamicSpecSourceTags();
	return SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1)
		|| SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2)
		|| SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3)
		|| SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4);
}

bool UAbilityCollectionRuntime::HasReplicatedAbilityListChanged(
	const UPdAbilitySystemComponent& AbilitySystemComponent) const
{
	const TArray<FGameplayAbilitySpec>& CurrentAbilities =
		AbilitySystemComponent.GetActivatableAbilities();
	if (LastReplicatedAbilityHandles.Num() != CurrentAbilities.Num()
		|| LastReplicatedAbilityClasses.Num() != CurrentAbilities.Num())
	{
		return true;
	}

	for (int32 Index = 0; Index < CurrentAbilities.Num(); ++Index)
	{
		const FGameplayAbilitySpec& CurrentSpec = CurrentAbilities[Index];
		const TSubclassOf<UGameplayAbility> CurrentAbilityClass =
			CurrentSpec.Ability ? CurrentSpec.Ability->GetClass() : nullptr;

		if (LastReplicatedAbilityHandles[Index] != CurrentSpec.Handle
			|| LastReplicatedAbilityClasses[Index] != CurrentAbilityClass)
		{
			return true;
		}
	}

	return false;
}

void UAbilityCollectionRuntime::CacheReplicatedAbilityList(
	const UPdAbilitySystemComponent& AbilitySystemComponent)
{
	LastReplicatedAbilityHandles.Reset();
	LastReplicatedAbilityClasses.Reset();

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		LastReplicatedAbilityHandles.Add(AbilitySpec.Handle);
		LastReplicatedAbilityClasses.Add(
			AbilitySpec.Ability ? AbilitySpec.Ability->GetClass() : nullptr);
	}
}

bool UAbilityCollectionRuntime::HasGrantedAbilityClass(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	const TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UClass* AbilityClassType = AbilityClass.Get();
	if (!AbilityClassType)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClassType)
		{
			return true;
		}
	}

	return false;
}
