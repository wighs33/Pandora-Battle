#include "Component/Pandora/PandoraComponent.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillSource.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"

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

void UPandoraComponent::GrantPandoraSkills(
	UPdAbilitySystemComponent* ASC, const UPandoraDefinition* Definition, const int32 PandoraLevel,
	const EEnum_Direction LoadoutDirection)
{
	if (!ASC || GetOwner() != ASC->GetOwner() || !ASC->IsOwnerActorAuthoritative() || !Definition)
	{
		return;
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
				CastChecked<UPandoraSkillSource>(ExistingSpec->SourceObject.Get())->SetLoadoutDirection(LoadoutDirection);
			}
			continue;
		}

		UPandoraSkillSource* Source = CreateSkillSource(Definition, SkillIndex, LoadoutDirection);
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
			GrantedPandoraAbilityHandles.Add(Handle);
			const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
			if (AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted())
			{
				UAbilityGrantAndInputManager::TryActivateGrantedAbilityNextTick(ASC, Handle);
			}
		}
		else
		{
			ReleaseSkillSourceIfUnused(Source);
		}
	}
}

void UPandoraComponent::RemoveGrantedPandoraSkills(UPdAbilitySystemComponent* ASC, const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
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

void UPandoraComponent::ClearGrantedPandoraContent()
{
	const TArray<FGameplayAbilitySpecHandle> Handles = MoveTemp(GrantedPandoraAbilityHandles);
	GrantedPandoraAbilityHandles.Reset();
	if (UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		RemoveGrantedPandoraSkills(ASC, Handles);
	}

	// 부여 실패나 GAS의 대기 중 추가 취소로 회수 콜백이 없었던 출처도 정리한다.
	// 목록 잠금으로 회수가 보류된 능력의 출처는 실제 회수 이벤트까지 유지한다.
	const TArray<TObjectPtr<UPandoraSkillSource>> Sources = OwnedSkillSources;
	for (UPandoraSkillSource* Source : Sources)
	{
		ReleaseSkillSourceIfUnused(Source);
	}
}

void UPandoraComponent::RefreshPandoraSkillInputBindings(UPdAbilitySystemComponent* ASC) const
{
	if (!HasPandoraAuthority() || !ASC)
	{
		return;
	}
	const UPandoraDefinition* Definition = CurrentPandoraDefinition;
	const int32 PandoraLevel = ResolveSelectedPandoraRuntimeLevel(Definition);
	const bool bShouldBindSelectedPandora = Definition && IsPandoraCompatibleWithCurrentWeapon(Definition);
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

void UPandoraComponent::BindAbilityRemoval()
{
	if (AbilityRemovedHandle.IsValid())
	{
		return;
	}
	UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (ASC && HasPandoraAuthority())
	{
		SourceAbilitySystemComponent = ASC;
		AbilityRemovedHandle = ASC->OnAbilityRemovedNative.AddUObject(this, &ThisClass::HandleGrantedAbilityRemoved);
	}
}

UPandoraSkillSource* UPandoraComponent::CreateSkillSource(const UPandoraDefinition* Definition,
	const int32 SkillIndex, const EEnum_Direction LoadoutDirection)
{
	if (!HasPandoraAuthority() || !Definition || !Definition->GetSkillDefinition(SkillIndex))
	{
		return nullptr;
	}
	BindAbilityRemoval();
	if (!SourceAbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	UPandoraSkillSource* Source = NewObject<UPandoraSkillSource>(this);
	Source->Initialize(Definition, SkillIndex, LoadoutDirection);
	OwnedSkillSources.Add(Source);
	if (IsReadyForReplication())
	{
		AddReplicatedSubObject(Source);
	}
	return Source;
}

void UPandoraComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (!HasPandoraAuthority())
	{
		return;
	}
	BindAbilityRemoval();
	// 능력 부여 여부와 무관하게, 복제 준비 전에 생성된 출처도 등록한다.
	for (UPandoraSkillSource* Source : OwnedSkillSources)
	{
		if (IsValid(Source))
		{
			AddReplicatedSubObject(Source);
		}
	}
}

void UPandoraComponent::HandleGrantedAbilityRemoved(const FGameplayAbilitySpec& Spec)
{
	GrantedPandoraAbilityHandles.Remove(Spec.Handle);
	ReleaseSkillSourceIfUnused(Cast<UPandoraSkillSource>(Spec.SourceObject.Get()), Spec.Handle);
}

void UPandoraComponent::ReleaseSkillSourceIfUnused(UPandoraSkillSource* Source, const FGameplayAbilitySpecHandle RemovedHandle)
{
	if (!HasPandoraAuthority() || !IsValid(Source) || Source->GetOuter() != this || !OwnedSkillSources.Contains(Source))
	{
		return;
	}
	if (const UPdAbilitySystemComponent* ASC = SourceAbilitySystemComponent.Get())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			// 회수 콜백은 Spec이 목록에서 삭제되기 전에 실행된다.
			if (Spec.Handle != RemovedHandle && Spec.SourceObject.Get() == Source)
			{
				return;
			}
		}
	}
	DestroyReplicatedSubObjectOnRemotePeers(Source);
	OwnedSkillSources.Remove(Source);
}

void UPandoraComponent::HandleSkillSourceReplicated(UPandoraSkillSource* Source)
{
	if (!IsValid(Source) || Source->GetOuter() != this)
	{
		return;
	}
	// 클라이언트에서는 SourceObject의 약한 참조와 별개로 출처를 보관한다.
	OwnedSkillSources.AddUnique(Source);
	if (UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		// LocalPredicted 시전의 성공 응답은 GAS가 확인만 한다. 출처 수신은 UI 준비 상태만 갱신한다.
		ASC->OnAbilitiesChangedNative.Broadcast();
	}
}

void UPandoraComponent::HandleSkillSourceDestroyed(UPandoraSkillSource* Source)
{
	OwnedSkillSources.Remove(Source);
}

void UPandoraComponent::ReleaseSkillSourcesForEndPlay()
{
	if (UPdAbilitySystemComponent* ASC = SourceAbilitySystemComponent.Get())
	{
		ASC->OnAbilityRemovedNative.Remove(AbilityRemovedHandle);
	}
	AbilityRemovedHandle.Reset();
	SourceAbilitySystemComponent.Reset();
	if (HasPandoraAuthority())
	{
		for (UPandoraSkillSource* Source : OwnedSkillSources)
		{
			if (IsValid(Source))
			{
				DestroyReplicatedSubObjectOnRemotePeers(Source);
			}
		}
	}
	OwnedSkillSources.Reset();
}
