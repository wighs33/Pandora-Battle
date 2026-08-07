#include "Component/AbilitySystem/AbilityResetRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/AbilityCollectionRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityResetRuntime)

namespace
{
enum class EAbilityEffectRemovalEvent : uint8
{
	Death,
	Respawn,
	PandoraReset
};

struct FAbilityEffectRemovalRule
{
	FGameplayTagQuery EffectTagQuery;
	FGameplayTagQuery OwningTagQuery;
	FGameplayTagQuery LooseTagQuery;
	FGameplayTagContainer GameplayCuesToRemove;
};

FAbilityEffectRemovalRule MakeEffectRemovalRule(
	const EAbilityEffectRemovalEvent RemovalEvent,
	const UPdAbilitySystemComponent& AbilitySystemComponent)
{
	FAbilityEffectRemovalRule Rule;
	switch (RemovalEvent)
	{
	case EAbilityEffectRemovalEvent::Death:
		{
			FGameplayTagContainer EffectTags;
			EffectTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnDeath);
			EffectTags.AddTag(LabGameplayTags::Cooldown);
			Rule.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(EffectTags);
			Rule.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(LabGameplayTags::Cooldown);
			break;
		}

	case EAbilityEffectRemovalEvent::Respawn:
		{
			FGameplayTagContainer EffectTags;
			EffectTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnRespawn);
			EffectTags.AddTag(LabGameplayTags::Debuff);
			EffectTags.AddTag(LabGameplayTags::Status_Burning);
			EffectTags.AddTag(LabGameplayTags::Status_Frostbite);
			EffectTags.AddTag(LabGameplayTags::Status_ElectricShock);
			Rule.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(EffectTags);

			FGameplayTagContainer OwnedTags;
			OwnedTags.AddTag(LabGameplayTags::Debuff);
			OwnedTags.AddTag(LabGameplayTags::Status_Burning);
			OwnedTags.AddTag(LabGameplayTags::Status_Frostbite);
			OwnedTags.AddTag(LabGameplayTags::Status_ElectricShock);
			Rule.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(OwnedTags);
			Rule.LooseTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(OwnedTags);
			break;
		}

	case EAbilityEffectRemovalEvent::PandoraReset:
		Rule.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(
			LabGameplayTags::Effect_Policy_RemoveOnPandoraReset);
		break;
	}

	if (const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(&AbilitySystemComponent))
	{
		switch (RemovalEvent)
		{
		case EAbilityEffectRemovalEvent::Death:
			Rule.GameplayCuesToRemove = SettingDefinition->DeathGameplayCuesToRemove;
			break;
		case EAbilityEffectRemovalEvent::Respawn:
			Rule.GameplayCuesToRemove = SettingDefinition->RespawnGameplayCuesToRemove;
			break;
		case EAbilityEffectRemovalEvent::PandoraReset:
			Rule.GameplayCuesToRemove = SettingDefinition->PandoraResetGameplayCuesToRemove;
			break;
		}
	}

	return Rule;
}

int32 RemoveActiveEffectsMatchingRule(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FAbilityEffectRemovalRule& RemovalRule)
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative())
	{
		return 0;
	}

	const int32 InitialActiveEffectCount =
		AbilitySystemComponent.GetNumActiveGameplayEffects();

	if (!RemovalRule.EffectTagQuery.IsEmpty())
	{
		FGameplayEffectQuery EffectQuery;
		EffectQuery.EffectTagQuery = RemovalRule.EffectTagQuery;
		AbilitySystemComponent.RemoveActiveEffects(EffectQuery);
	}

	if (!RemovalRule.OwningTagQuery.IsEmpty())
	{
		FGameplayEffectQuery OwningQuery;
		OwningQuery.OwningTagQuery = RemovalRule.OwningTagQuery;
		AbilitySystemComponent.RemoveActiveEffects(OwningQuery);
	}

	if (!RemovalRule.LooseTagQuery.IsEmpty())
	{
		FGameplayTagContainer OwnedTags;
		AbilitySystemComponent.GetOwnedGameplayTags(OwnedTags);
		for (const FGameplayTag& OwnedTag : OwnedTags)
		{
			FGameplayTagContainer SingleTag;
			SingleTag.AddTag(OwnedTag);
			if (RemovalRule.LooseTagQuery.Matches(SingleTag))
			{
				AbilitySystemComponent.SetLooseGameplayTagCount(
					OwnedTag,
					0,
					EGameplayTagReplicationState::CountToOwner);
			}
		}
	}

	for (const FGameplayTag& GameplayCueTag : RemovalRule.GameplayCuesToRemove)
	{
		AbilitySystemComponent.RemoveGameplayCue(GameplayCueTag);
	}

	AbilitySystemComponent.ForceReplication();
	return FMath::Max(
		InitialActiveEffectCount - AbilitySystemComponent.GetNumActiveGameplayEffects(),
		0);
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

void UAbilityResetRuntime::ResetAbilityRuntimeState(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const UAbilityCollectionRuntime& CollectionRuntime,
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

	RemoveActiveEffectsMatchingRule(
		AbilitySystemComponent,
		MakeEffectRemovalRule(
			bPandoraAbilitiesOnly
				? EAbilityEffectRemovalEvent::PandoraReset
				: EAbilityEffectRemovalEvent::Death,
			AbilitySystemComponent));

	AbilitySystemComponent.NotifyAbilitiesChanged();
}

int32 UAbilityResetRuntime::ClearStatusEffectsForRespawn(
	UPdAbilitySystemComponent& AbilitySystemComponent) const
{
	return RemoveActiveEffectsMatchingRule(
		AbilitySystemComponent,
		MakeEffectRemovalRule(
			EAbilityEffectRemovalEvent::Respawn,
			AbilitySystemComponent));
}
