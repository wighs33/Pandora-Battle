#include "Pandora/PandoraSkillRuntimeContext.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillRuntimeContext)

void UPandoraSkillRuntimeContext::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const USkillDefinition* InSkillDataAsset,
	const int32 InSkillIndex,
	const int32 InPandoraLevel,
	const EEnum_Direction InLoadoutDirection)
{
	PandoraDefinition = InPandoraDefinition;
	SkillDataAsset = InSkillDataAsset;
	SkillIndex = InSkillIndex;
	PandoraLevel = FMath::Max(InPandoraLevel, 1);
	LoadoutDirection = InLoadoutDirection;
}

const FSkill* UPandoraSkillRuntimeContext::GetPandoraSkill() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition && Definition->Skill.IsValidIndex(SkillIndex) ? &Definition->Skill[SkillIndex] : nullptr;
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPandoraSkillRuntimeContext::GetProjectileImpactEffectAreas() const
{
	const USkillDefinition* SkillData = SkillDataAsset.Get();
	if (!SkillData || SkillData->SkillDataType != ESkillDataType::Projectile)
	{
		return TArray<FProjectileImpactEffectAreaSpawnConfig>();
	}

	return TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

// 현재 선택과 무관하게, 서버가 부여한 원래 판도라와 스킬 정보를 클라이언트에 전달한다.
void UPandoraSkillRuntimeContext::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, PandoraDefinition);
	DOREPLIFETIME(ThisClass, SkillDataAsset);
	DOREPLIFETIME(ThisClass, SkillIndex);
	DOREPLIFETIME(ThisClass, PandoraLevel);
	DOREPLIFETIME(ThisClass, LoadoutDirection);
}

void UPandoraSkillRuntimeContext::OnRep_Source()
{
	if (UPdAbilitySystemComponent* ASC = GetTypedOuter<UPdAbilitySystemComponent>())
	{
		ASC->NotifyPandoraSourceReplicated(this);
	}
}

#if UE_WITH_IRIS
void UPandoraSkillRuntimeContext::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}
#endif

// 같은 슬롯 번호나 스킬 클래스를 써도 다른 판도라의 쿨다운과 섞이지 않도록 원본을 비교한다.
FGameplayEffectQuery UPandoraSkillRuntimeContext::MakeCooldownQuery() const
{
	FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(LabGameplayTags::Cooldown));
	const TWeakObjectPtr<const UPandoraDefinition> SourcePandora = PandoraDefinition.Get();
	const int32 SourceSkillIndex = SkillIndex;
	Query.CustomMatchDelegate.BindLambda([SourcePandora, SourceSkillIndex](const FActiveGameplayEffect& Effect)
	{
		const UPandoraSkillRuntimeContext* Source = Cast<UPandoraSkillRuntimeContext>(Effect.Spec.GetContext().GetSourceObject());
		return SourcePandora.IsValid() && SourceSkillIndex != INDEX_NONE && Source
			&& Source->GetPandoraDefinition() == SourcePandora.Get() && Source->GetSkillIndex() == SourceSkillIndex;
	});
	return Query;
}
