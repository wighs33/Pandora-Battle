#include "Pandora/PandoraSkillBinder.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPandoraSkillBinder, Log, All);

namespace
{
	constexpr int32 MaxPandoraSlotsValue = 4;

	void TryActivateAbilityNextTick(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle AbilityHandle)
	{
		if (!AbilitySystemComponent || !AbilityHandle.IsValid())
		{
			return;
		}

		if (UWorld* World = AbilitySystemComponent->GetWorld())
		{
			TWeakObjectPtr<UAbilitySystemComponent> WeakASC = AbilitySystemComponent;
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakASC, AbilityHandle]()
			{
				if (UAbilitySystemComponent* ASC = WeakASC.Get())
				{
					ASC->TryActivateAbility(AbilityHandle);
				}
			}));
			return;
		}

		AbilitySystemComponent->TryActivateAbility(AbilityHandle);
	}

	bool IsPandoraInputTag(const FGameplayTag& InputTag)
	{
		return InputTag.MatchesTagExact(LabGameplayTags::Input_Ability_Skill1)
			|| InputTag.MatchesTagExact(LabGameplayTags::Input_Ability_Skill2)
			|| InputTag.MatchesTagExact(LabGameplayTags::Input_Ability_Skill3)
			|| InputTag.MatchesTagExact(LabGameplayTags::Input_Ability_Skill4);
	}

	void RemovePandoraInputTags(FGameplayAbilitySpec& AbilitySpec)
	{
		FGameplayTagContainer TagsToRemove;
		for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
		{
			if (IsPandoraInputTag(Tag))
			{
				TagsToRemove.AddTag(Tag);
			}
		}

		AbilitySpec.GetDynamicSpecSourceTags().RemoveTags(TagsToRemove);
	}

	FGameplayTag GetPandoraInputTag(const int32 SlotIndex)
	{
		switch (SlotIndex)
		{
		case 0:
			return LabGameplayTags::Input_Ability_Skill1;
		case 1:
			return LabGameplayTags::Input_Ability_Skill2;
		case 2:
			return LabGameplayTags::Input_Ability_Skill3;
		case 3:
			return LabGameplayTags::Input_Ability_Skill4;
		default:
			return FGameplayTag();
		}
	}

	int32 ResolveEntryLevel(const UPandoraDefinition* PandoraDefinition, const FSkill& Entry, const int32 PandoraLevel)
	{
		const int32 EffectivePandoraLevel = PandoraLevel > 0 ? PandoraLevel : 1;
		const int32 MaxUnlockLevel = PandoraDefinition ? PandoraDefinition->GetMaxLevel() : Entry.GetMaxLevel();
		return FMath::Clamp(EffectivePandoraLevel, 1, FMath::Max(MaxUnlockLevel, 1));
	}

	UPandoraSkillRuntimeContext* CreateRuntimeContext(
		UObject* ContextOuter,
		const UPandoraDefinition* PandoraDefinition,
		const FSkill& Entry,
		const int32 SlotIndex,
		const int32 EntryLevel)
	{
		UObject* EffectiveOuter = ContextOuter ? ContextOuter : GetTransientPackage();
		UPandoraSkillRuntimeContext* RuntimeContext = NewObject<UPandoraSkillRuntimeContext>(EffectiveOuter);
		RuntimeContext->Initialize(PandoraDefinition, Entry.SkillDefinition.Get(), SlotIndex, EntryLevel);
		return RuntimeContext;
	}

	FGameplayAbilitySpec* FindAbilitySpecByRuntimeContext(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UPandoraDefinition* PandoraDefinition,
		const int32 SlotIndex)
	{
		if (!AbilitySystemComponent || !PandoraDefinition || SlotIndex == INDEX_NONE)
		{
			return nullptr;
		}

		TArray<FGameplayAbilitySpecHandle> AbilityHandles;
		AbilitySystemComponent->GetAllAbilities(AbilityHandles);
		for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
		{
			FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
			const UPandoraSkillRuntimeContext* RuntimeContext = AbilitySpec
				? Cast<UPandoraSkillRuntimeContext>(AbilitySpec->SourceObject.Get())
				: nullptr;
			if (RuntimeContext
				&& RuntimeContext->GetPandoraDefinition() == PandoraDefinition
				&& RuntimeContext->GetSkillIndex() == SlotIndex)
			{
				return AbilitySpec;
			}
		}

		return nullptr;
	}
}

FPandoraSkillBindingResult FPandoraSkillBinder::GrantPandoraContent(
	UObject* ContextOuter,
	AActor* AuthorityOwner,
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const UPandoraDefinition* PandoraDefinition,
	const int32 PandoraLevel)
{
	FPandoraSkillBindingResult Result;
	if (!AuthorityOwner || !AuthorityOwner->HasAuthority() || !PandoraDefinition || !AbilitySystemComponent)
	{
		return Result;
	}

	const int32 NumEntriesToGrant = FMath::Min(PandoraDefinition->Skill.Num(), MaxPandoraSlotsValue);
	for (int32 SlotIndex = 0; SlotIndex < NumEntriesToGrant; ++SlotIndex)
	{
		const FSkill& Entry = PandoraDefinition->Skill[SlotIndex];
		const int32 EntryLevel = ResolveEntryLevel(PandoraDefinition, Entry, PandoraLevel);

		const TArray<TSubclassOf<UGameplayAbility>> AbilityClasses = Entry.GetAbilitiesToGrantForLevel(EntryLevel);
		const FGameplayTag InputTag = GetPandoraInputTag(SlotIndex);
		bool bShouldBindInputTag = true;
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
		{
			if (!AbilityClass)
			{
				continue;
			}

			UPandoraSkillRuntimeContext* RuntimeContext = CreateRuntimeContext(
				ContextOuter,
				PandoraDefinition,
				Entry,
				SlotIndex,
				EntryLevel);
			Result.RuntimeContexts.Add(RuntimeContext);

			FGameplayAbilitySpec AbilitySpec(AbilityClass, EntryLevel, INDEX_NONE, RuntimeContext);
			if (bShouldBindInputTag && InputTag.IsValid())
			{
				AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);
			}

			const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
			const bool bAutoActivateWhenGranted = AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();
			const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(AbilitySpec);
			if (GrantedHandle.IsValid())
			{
				Result.AbilityHandles.Add(GrantedHandle);
				if (bAutoActivateWhenGranted)
				{
					TryActivateAbilityNextTick(AbilitySystemComponent, GrantedHandle);
				}

				UE_LOG(LogPandoraSkillBinder, Log, TEXT("Granted pandora ability: owner=%s pandora=%s slot=%d source=%s level=%d ability=%s input=%s context=%s"),
					*GetNameSafe(AuthorityOwner),
					*GetNameSafe(PandoraDefinition),
					SlotIndex,
					*GetNameSafe(RuntimeContext ? RuntimeContext->GetSkillDataAsset() : nullptr),
					EntryLevel,
					*GetNameSafe(AbilityClass.Get()),
					bShouldBindInputTag ? *InputTag.ToString() : TEXT("None"),
					*GetNameSafe(RuntimeContext));
			}

			bShouldBindInputTag = false;
		}

		const TArray<TSubclassOf<UGameplayEffect>> EffectClasses = Entry.GetEffectsToApplyForLevel(EntryLevel);
		for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectClasses)
		{
			if (!EffectClass)
			{
				continue;
			}

			UPandoraSkillRuntimeContext* RuntimeContext = CreateRuntimeContext(
				ContextOuter,
				PandoraDefinition,
				Entry,
				SlotIndex,
				EntryLevel);
			Result.RuntimeContexts.Add(RuntimeContext);

			FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
			EffectContext.AddSourceObject(RuntimeContext);

			const UGameplayEffect* EffectCDO = EffectClass->GetDefaultObject<UGameplayEffect>();
			const FActiveGameplayEffectHandle EffectHandle =
				AbilitySystemComponent->ApplyGameplayEffectToSelf(EffectCDO, EntryLevel, EffectContext);
			if (EffectHandle.WasSuccessfullyApplied())
			{
				Result.EffectHandles.Add(EffectHandle);
			}
		}
	}

	UE_LOG(LogPandoraSkillBinder, Log, TEXT("Applied pandora content: owner=%s pandora=%s abilities=%d effects=%d contexts=%d"),
		*GetNameSafe(AuthorityOwner),
		*GetNameSafe(PandoraDefinition),
		Result.AbilityHandles.Num(),
		Result.EffectHandles.Num(),
		Result.RuntimeContexts.Num());
	return Result;
}

void FPandoraSkillBinder::RemoveGrantedContent(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles,
	const TArray<FActiveGameplayEffectHandle>& EffectHandles)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!AbilityHandles.IsEmpty())
	{
		AbilitySystemComponent->RemoveAbilities(AbilityHandles);
	}

	for (const FActiveGameplayEffectHandle& EffectHandle : EffectHandles)
	{
		if (EffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(EffectHandle);
		}
	}
}

void FPandoraSkillBinder::RefreshInputBindings(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	const UPandoraDefinition* PandoraDefinition,
	const int32 PandoraLevel,
	const bool bShouldBindSelectedPandora)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	AbilitySystemComponent->GetAllAbilities(AbilityHandles);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
		if (!AbilitySpec || !AbilitySpec->Ability)
		{
			continue;
		}

		const int32 PreviousTagCount = AbilitySpec->GetDynamicSpecSourceTags().Num();
		RemovePandoraInputTags(*AbilitySpec);
		if (AbilitySpec->GetDynamicSpecSourceTags().Num() != PreviousTagCount)
		{
			AbilitySystemComponent->MarkAbilitySpecDirty(*AbilitySpec);
		}
	}

	if (bShouldBindSelectedPandora && PandoraDefinition)
	{
		const int32 NumEntriesToBind = FMath::Min(PandoraDefinition->Skill.Num(), MaxPandoraSlotsValue);
		for (int32 SlotIndex = 0; SlotIndex < NumEntriesToBind; ++SlotIndex)
		{
			const FSkill& Entry = PandoraDefinition->Skill[SlotIndex];
			const int32 EntryLevel = ResolveEntryLevel(PandoraDefinition, Entry, PandoraLevel);
			const TArray<TSubclassOf<UGameplayAbility>> AbilityClasses = Entry.GetAbilitiesToGrantForLevel(EntryLevel);
			if (AbilityClasses.IsEmpty() || !AbilityClasses[0])
			{
				continue;
			}

			FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByRuntimeContext(
				AbilitySystemComponent,
				PandoraDefinition,
				SlotIndex);
			if (!AbilitySpec)
			{
				continue;
			}

			const FGameplayTag InputTag = GetPandoraInputTag(SlotIndex);
			if (!InputTag.IsValid())
			{
				continue;
			}

			AbilitySpec->GetDynamicSpecSourceTags().AddTag(InputTag);
			AbilitySystemComponent->MarkAbilitySpecDirty(*AbilitySpec);
		}
	}

	AbilitySystemComponent->NotifyAbilitiesChanged();
}

int32 FPandoraSkillBinder::MaxPandoraSlots()
{
	return MaxPandoraSlotsValue;
}
