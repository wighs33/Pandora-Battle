#include "Component/Character/EnemyAttackSelection.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/Ability/PunchAbility.h"
#include "AbilitySystem/Ability/RangedAttackAbility.h"
#include "Algo/RandomShuffle.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

namespace PdEnemyAttackSelection
{
	namespace
	{
		FGameplayTag ResolveAttackAbilityTag(
			const bool bUsingRangedWeapon,
			const bool bHasEquippedWeapon)
		{
			if (bUsingRangedWeapon)
			{
				return LabGameplayTags::Action_RangedAttack;
			}

			return bHasEquippedWeapon
				? LabGameplayTags::Action_Attack
				: LabGameplayTags::Action_Punch;
		}

		bool IsAbilityClassCompatibleWithAttackMode(
			const UClass* AbilityClass,
			const bool bUsingRangedWeapon,
			const bool bHasEquippedWeapon)
		{
			if (!AbilityClass)
			{
				return false;
			}

			if (bUsingRangedWeapon)
			{
				return AbilityClass->IsChildOf(URangedAttackAbility::StaticClass());
			}

			const bool bIsPunchAbility =
				AbilityClass->IsChildOf(UPunchAbility::StaticClass());
			if (!bHasEquippedWeapon)
			{
				return bIsPunchAbility;
			}

			return AbilityClass->IsChildOf(UAttackAbility::StaticClass())
				&& !bIsPunchAbility;
		}
	}

	void GatherAttackAbilityHandles(
		const UPdAbilitySystemComponent& AbilitySystem,
		const bool bUsingRangedWeapon,
		const bool bHasEquippedWeapon,
		TArray<FGameplayAbilitySpecHandle>& OutAbilityHandles)
	{
		OutAbilityHandles.Reset();
		const FGameplayTag AttackAbilityTag =
			ResolveAttackAbilityTag(
				bUsingRangedWeapon,
				bHasEquippedWeapon);
		if (AttackAbilityTag.IsValid())
		{
			FGameplayTagContainer AttackAbilityTags;
			AttackAbilityTags.AddTag(AttackAbilityTag);
			AbilitySystem.FindAllAbilitiesWithTags(
				OutAbilityHandles,
				AttackAbilityTags,
				false);
		}

		if (!OutAbilityHandles.IsEmpty())
		{
			return;
		}

		// Compatibility fallback for legacy Blueprint abilities without the
		// expected native attack asset tag.
		for (const FGameplayAbilitySpec& AbilitySpec :
			AbilitySystem.GetActivatableAbilities())
		{
			const UGameplayAbility* AbilityCDO = AbilitySpec.Ability;
			const UClass* AbilityClass =
				AbilityCDO ? AbilityCDO->GetClass() : nullptr;
			if (IsAbilityClassCompatibleWithAttackMode(
					AbilityClass,
					bUsingRangedWeapon,
					bHasEquippedWeapon))
			{
				OutAbilityHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}

	bool TryContinueActiveAttack(
		UPdAbilitySystemComponent& AbilitySystem,
		const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
		const bool bRequestCombo)
	{
		for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
		{
			FGameplayAbilitySpec* AbilitySpec =
				AbilitySystem.FindAbilitySpecFromHandle(AbilityHandle);
			if (!AbilitySpec || !AbilitySpec->IsActive())
			{
				continue;
			}

			UAttackAbility* ActiveAttackAbility =
				Cast<UAttackAbility>(AbilitySpec->GetPrimaryInstance());
			if (ActiveAttackAbility)
			{
				if (bRequestCombo)
				{
					const FName RequestedSectionName =
						ActiveAttackAbility->GetNextAttackSectionName();
					if (!RequestedSectionName.IsNone())
					{
						ActiveAttackAbility->RequestJumpToSection(
							RequestedSectionName);
					}
				}
				return true;
			}

			const UGameplayAbility* AbilityCDO = AbilitySpec->Ability;
			const UClass* AbilityClass =
				AbilityCDO ? AbilityCDO->GetClass() : nullptr;
			if (AbilityClass
				&& (AbilityClass->IsChildOf(UAttackAbility::StaticClass())
					|| AbilityClass->IsChildOf(
						URangedAttackAbility::StaticClass())))
			{
				return true;
			}
		}

		return false;
	}

	bool TryActivateAnyAttack(
		UPdAbilitySystemComponent& AbilitySystem,
		TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
	{
		Algo::RandomShuffle(AbilityHandles);
		for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
		{
			if (AbilitySystem.TryActivateAbility(
					AbilityHandle,
					true))
			{
				return true;
			}
		}
		return false;
	}

	bool IsAnyAttackActive(const UPdAbilitySystemComponent& AbilitySystem)
	{
		return AbilitySystem.HasActiveAbilityOfAnyClass({
			UAttackAbility::StaticClass(),
			URangedAttackAbility::StaticClass(),
			UPunchAbility::StaticClass()
		});
	}
}
