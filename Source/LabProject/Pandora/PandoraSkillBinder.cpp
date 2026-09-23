#include "Pandora/PandoraSkillBinder.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillSource.h"

namespace
{
	FGameplayTag GetPandoraInputTag(const int32 SkillIndex)
	{
		switch (SkillIndex)
		{
		case 0: return LabGameplayTags::Input_Ability_Skill1;
		case 1: return LabGameplayTags::Input_Ability_Skill2;
		case 2: return LabGameplayTags::Input_Ability_Skill3;
		default: return FGameplayTag();
		}
	}

	FGameplayAbilitySpec* FindGrantedSkill(UPdAbilitySystemComponent& ASC, const UPandoraDefinition* Definition,
		const int32 SkillIndex)
	{
		for (FGameplayAbilitySpec& Spec : ASC.GetActivatableAbilities())
		{
			const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Spec.SourceObject.Get());
			if (Spec.Ability && Spec.Ability->IsA<USkillAbility>() && Source
				&& Source->GetPandoraDefinition() == Definition && Source->GetSkillIndex() == SkillIndex)
			{
				return &Spec;
			}
		}
		return nullptr;
	}
}

TArray<FGameplayAbilitySpecHandle> FPandoraSkillBinder::GrantPandoraContent(
	UPandoraComponent* SourceOwner, UPdAbilitySystemComponent* ASC, const UPandoraDefinition* Definition, const int32 PandoraLevel,
	const EEnum_Direction LoadoutDirection)
{
	TArray<FGameplayAbilitySpecHandle> NewHandles;
	if (!SourceOwner || !ASC || SourceOwner->GetOwner() != ASC->GetOwner()
		|| !ASC->IsOwnerActorAuthoritative() || !Definition)
	{
		return NewHandles;
	}

	const int32 SkillCount = FMath::Min(Definition->GetSkillCount(), UPandoraDefinition::GetFixedMaxLevel());
	for (int32 SkillIndex = 0; SkillIndex < SkillCount; ++SkillIndex)
	{
		if (!Definition->IsSkillSlotUnlocked(SkillIndex, PandoraLevel))
		{
			continue;
		}
		const USkillDefinition* Skill = Definition->GetSkillDefinition(SkillIndex);
		const int32 SkillLevel = UPandoraDefinition::GetRequiredLevelForSkillSlot(SkillIndex);
		const TSubclassOf<UGameplayAbility> AbilityClass = USkillAbility::StaticClass();
		if (!Skill || !Skill->Action)
		{
			continue;
		}
		if (FGameplayAbilitySpec* ExistingSpec = FindGrantedSkill(*ASC, Definition, SkillIndex))
		{
			// 실행 중인 시전의 원본은 건드리지 않는다. 다음 시전은 PreActivate에서 선택 방향을 반영한다.
			if (!ExistingSpec->IsActive())
			{
				CastChecked<UPandoraSkillSource>(ExistingSpec->SourceObject.Get())->Initialize(
					Definition, SkillIndex, LoadoutDirection);
			}
			continue;
		}

		UPandoraSkillSource* Source = SourceOwner->CreateSkillSource(Definition, SkillIndex, LoadoutDirection);
		if (!Source)
		{
			continue;
		}
		FGameplayAbilitySpec Spec(AbilityClass, SkillLevel, INDEX_NONE, Source);
		Spec.GetDynamicSpecSourceTags().AppendTags(Skill->Activation.Tags);
		Spec.GetDynamicSpecSourceTags().AddTag(LabGameplayTags::Ability_Source_Pandora);
		Spec.GetDynamicSpecSourceTags().AddTag(LabGameplayTags::Ability_Pandora_Selected);
		if (Skill->SkillType == ESkillType::Press)
		{
			Spec.GetDynamicSpecSourceTags().AddTag(LabGameplayTags::Skill_Type_Press);
		}
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (Handle.IsValid())
		{
			NewHandles.Add(Handle);
			const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
			if (AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted())
			{
				UAbilityGrantAndInputManager::TryActivateGrantedAbilityNextTick(ASC, Handle);
			}
		}
		else
		{
			SourceOwner->ReleaseSkillSourceIfUnused(Source);
		}
	}
	return NewHandles;
}

// 인벤토리 전체 초기화나 기능 종료에서만 회수한다. 단순 선택 변경은 이 경로를 사용하지 않는다.
void FPandoraSkillBinder::RemoveGrantedContent(UPdAbilitySystemComponent* ASC, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	for (const FGameplayAbilitySpecHandle Handle : AbilityHandles)
	{
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			if (Spec->SourceObject.IsValid())
			{
				FGameplayEffectQuery Query;
				Query.EffectSource = Spec->SourceObject.Get();
				ASC->RemoveActiveEffects(Query);
			}
		}
	}
	ASC->RemoveAbilities(AbilityHandles);
}

// 선택한 판도라만 새 입력을 받는다. 비선택 스킬의 실행 인스턴스와 활성 효과는 그대로 둔다.
void FPandoraSkillBinder::RefreshInputBindings(
	UPdAbilitySystemComponent* ASC, const UPandoraDefinition* Definition, const int32 PandoraLevel,
	const bool bShouldBindSelectedPandora)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Spec.SourceObject.Get());
		if (!Source || !Spec.Ability)
		{
			continue;
		}
		FGameplayTagContainer& Tags = Spec.GetDynamicSpecSourceTags();
		const FGameplayTagContainer PreviousTags = Tags;
		Tags.RemoveTag(LabGameplayTags::Ability_Pandora_Selected);
		for (int32 Index = 0; Index < UPandoraDefinition::GetFixedMaxLevel(); ++Index)
		{
			Tags.RemoveTag(GetPandoraInputTag(Index));
		}
		Tags.RemoveTag(LabGameplayTags::Input_Ability_Skill4);

		const int32 SkillIndex = Source->GetSkillIndex();
		if (bShouldBindSelectedPandora && Definition && Source->GetPandoraDefinition() == Definition
			&& Definition->IsSkillSlotUnlocked(SkillIndex, PandoraLevel))
		{
			Tags.AddTag(LabGameplayTags::Ability_Pandora_Selected);
			if (Spec.Ability->IsA<USkillAbility>())
			{
				Tags.AddTag(GetPandoraInputTag(SkillIndex));
			}
		}
		if (Tags != PreviousTags)
		{
			ASC->MarkAbilitySpecDirty(Spec);
		}
	}
}
