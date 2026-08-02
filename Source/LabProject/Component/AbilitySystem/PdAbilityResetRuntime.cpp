#include "Component/AbilitySystem/PdAbilityResetRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilityCollectionRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilityResetRuntime)

namespace
{
const UGameSettingDefinition* ResolveAbilitySystemSettingDefinition(
	const UPdAbilitySystemComponent& AbilitySystemComponent)
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(&AbilitySystemComponent);
	return SettingDefinition
		? SettingDefinition
		: GetDefault<UGameSettingDefinition>();
}

bool CanPrepareAbilityInstanceForRuntimeReset(
	const UGameplayAbility* AbilityInstance,
	const UPdAbilitySystemComponent& AbilitySystemComponent)
{
	if (!IsValid(AbilityInstance) || !AbilityInstance->IsActive())
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo =
		AbilityInstance->GetCurrentActorInfo();
	return ActorInfo
		&& ActorInfo->AbilitySystemComponent.Get() == &AbilitySystemComponent;
}
}

void UPdAbilityResetRuntime::ResetAbilityRuntimeState(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const UPdAbilityCollectionRuntime& CollectionRuntime,
	const bool bPandoraAbilitiesOnly)
{
	TArray<FGameplayAbilitySpecHandle> ActiveAbilityHandles;
	{
		FScopedAbilityListLock AbilityListLock(AbilitySystemComponent);
		for (FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent.GetActivatableAbilities())
		{
			if (!AbilitySpec.Ability
				|| (bPandoraAbilitiesOnly
					&& !CollectionRuntime.IsPandoraAbilitySpec(AbilitySpec)))
			{
				continue;
			}

			AbilitySpec.InputPressed = false;
			if (!AbilitySpec.IsActive()
				|| AbilitySpec.Ability->GetAssetTags().HasTagExact(
					LabGameplayTags::GameplayAbility_Death))
			{
				continue;
			}

			for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
			{
				if (!IsValid(AbilityInstance))
				{
					continue;
				}

				if (UPdGameplayAbility* PdAbilityInstance =
					Cast<UPdGameplayAbility>(AbilityInstance))
				{
					PdAbilityInstance->SuppressPendingCooldownForRuntimeReset();
					PdAbilityInstance->CleanupConfiguredPresentation();
				}

				// A spec can remain active while its replicated/per-execution list still
				// contains an inactive instance that was never initialized locally.
				// UE 5.7's SetCanBeCanceled dereferences CurrentActorInfo without a
				// null check, so only prepare the instance that is actually bound to
				// this ASC. CancelAbilityHandle below remains the single cancel path.
				if (CanPrepareAbilityInstanceForRuntimeReset(
					AbilityInstance,
					AbilitySystemComponent))
				{
					AbilityInstance->SetCanBeCanceled(true);
				}
			}

			ActiveAbilityHandles.Add(AbilitySpec.Handle);
		}
	}

	{
		TGuardValue<bool> ResetGuard(bResettingAbilityRuntimeState, true);
		for (const FGameplayAbilitySpecHandle& AbilityHandle : ActiveAbilityHandles)
		{
			AbilitySystemComponent.CancelAbilityHandle(AbilityHandle);
		}
	}

	if (const UGameSettingDefinition* SettingDefinition =
		ResolveAbilitySystemSettingDefinition(AbilitySystemComponent))
	{
		RemoveActiveEffectsMatchingPolicy(
			AbilitySystemComponent,
			bPandoraAbilitiesOnly
				? SettingDefinition->RemoveOnPandoraResetPolicy
				: SettingDefinition->RemoveOnDeathPolicy);
	}

	AbilitySystemComponent.NotifyAbilitiesChanged();
}

int32 UPdAbilityResetRuntime::ClearStatusEffectsForRespawn(
	UPdAbilitySystemComponent& AbilitySystemComponent) const
{
	const UGameSettingDefinition* SettingDefinition =
		ResolveAbilitySystemSettingDefinition(AbilitySystemComponent);
	return SettingDefinition
		? RemoveActiveEffectsMatchingPolicy(
			AbilitySystemComponent,
			SettingDefinition->RemoveOnRespawnPolicy)
		: 0;
}

int32 UPdAbilityResetRuntime::RemoveActiveEffectsMatchingPolicy(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FPdGameplayEffectRemovalPolicy& RemovalPolicy) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative())
	{
		return 0;
	}

	const int32 InitialActiveEffectCount =
		AbilitySystemComponent.GetNumActiveGameplayEffects();

	if (!RemovalPolicy.EffectTagQuery.IsEmpty())
	{
		FGameplayEffectQuery EffectQuery;
		EffectQuery.EffectTagQuery = RemovalPolicy.EffectTagQuery;
		AbilitySystemComponent.RemoveActiveEffects(EffectQuery);
	}

	if (!RemovalPolicy.OwningTagQuery.IsEmpty())
	{
		FGameplayEffectQuery OwningQuery;
		OwningQuery.OwningTagQuery = RemovalPolicy.OwningTagQuery;
		AbilitySystemComponent.RemoveActiveEffects(OwningQuery);
	}

	if (!RemovalPolicy.LooseTagQuery.IsEmpty())
	{
		FGameplayTagContainer OwnedTags;
		AbilitySystemComponent.GetOwnedGameplayTags(OwnedTags);
		for (const FGameplayTag& OwnedTag : OwnedTags)
		{
			FGameplayTagContainer SingleTag;
			SingleTag.AddTag(OwnedTag);
			if (RemovalPolicy.LooseTagQuery.Matches(SingleTag))
			{
				AbilitySystemComponent.SetLooseGameplayTagCount(
					OwnedTag,
					0,
					EGameplayTagReplicationState::CountToOwner);
			}
		}
	}

	for (const FGameplayTag& GameplayCueTag : RemovalPolicy.GameplayCuesToRemove)
	{
		AbilitySystemComponent.RemoveGameplayCue(GameplayCueTag);
	}

	AbilitySystemComponent.ForceReplication();
	return FMath::Max(
		InitialActiveEffectCount - AbilitySystemComponent.GetNumActiveGameplayEffects(),
		0);
}
