#include "Pandora/PandoraSkillSource.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillSource)

void UPandoraSkillSource::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const USkillDefinition* InSkillDataAsset,
	const int32 InSkillIndex,
	const int32 InPandoraLevel,
	const EEnum_Direction InLoadoutDirection)
{
	if (PandoraDefinition != InPandoraDefinition)
	{
		PandoraDefinition = InPandoraDefinition;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, PandoraDefinition, this);
	}
	if (SkillDataAsset != InSkillDataAsset)
	{
		SkillDataAsset = InSkillDataAsset;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, SkillDataAsset, this);
	}
	if (SkillIndex != InSkillIndex)
	{
		SkillIndex = InSkillIndex;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, SkillIndex, this);
	}
	const int32 NewPandoraLevel = FMath::Max(InPandoraLevel, 1);
	if (PandoraLevel != NewPandoraLevel)
	{
		PandoraLevel = NewPandoraLevel;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, PandoraLevel, this);
	}
	if (LoadoutDirection != InLoadoutDirection)
	{
		LoadoutDirection = InLoadoutDirection;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, LoadoutDirection, this);
	}
}

const FSkill* UPandoraSkillSource::GetPandoraSkill() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition && Definition->Skill.IsValidIndex(SkillIndex) ? &Definition->Skill[SkillIndex] : nullptr;
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPandoraSkillSource::GetProjectileImpactEffectAreas() const
{
	const USkillDefinition* SkillData = SkillDataAsset.Get();
	if (!SkillData || SkillData->SkillDataType != ESkillDataType::Projectile)
	{
		return TArray<FProjectileImpactEffectAreaSpawnConfig>();
	}

	return TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

// 현재 선택과 무관하게, 서버가 부여한 원래 판도라와 스킬 정보를 클라이언트에 전달한다.
void UPandoraSkillSource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PandoraDefinition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SkillDataAsset, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SkillIndex, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LoadoutDirection, Params);
}

void UPandoraSkillSource::OnRep_Source()
{
	if (UPdAbilitySystemComponent* ASC = GetTypedOuter<UPdAbilitySystemComponent>())
	{
		ASC->NotifyPandoraSourceReplicated(this);
	}
}

#if UE_WITH_IRIS
void UPandoraSkillSource::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}
#endif

// 같은 슬롯 번호나 스킬 클래스를 써도 다른 판도라의 쿨다운과 섞이지 않도록 원본을 비교한다.
FGameplayEffectQuery UPandoraSkillSource::MakeCooldownQuery() const
{
	FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(LabGameplayTags::Cooldown));
	const TWeakObjectPtr<const UPandoraDefinition> SourcePandora = PandoraDefinition.Get();
	const int32 SourceSkillIndex = SkillIndex;
	Query.CustomMatchDelegate.BindLambda([SourcePandora, SourceSkillIndex](const FActiveGameplayEffect& Effect)
	{
		const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Effect.Spec.GetContext().GetSourceObject());
		return SourcePandora.IsValid() && SourceSkillIndex != INDEX_NONE && Source
			&& Source->GetPandoraDefinition() == SourcePandora.Get() && Source->GetSkillIndex() == SourceSkillIndex;
	});
	return Query;
}
