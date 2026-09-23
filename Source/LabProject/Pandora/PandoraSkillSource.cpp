#include "Pandora/PandoraSkillSource.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "AbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Mode/PdPlayerState.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillSource)

void UPandoraSkillSource::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const int32 InSkillIndex,
	const EEnum_Direction InLoadoutDirection)
{
	if (PandoraDefinition != InPandoraDefinition)
	{
		PandoraDefinition = InPandoraDefinition;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, PandoraDefinition, this);
	}
	if (SkillIndex != InSkillIndex)
	{
		SkillIndex = InSkillIndex;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, SkillIndex, this);
	}
	if (LoadoutDirection != InLoadoutDirection)
	{
		LoadoutDirection = InLoadoutDirection;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraSkillSource, LoadoutDirection, this);
	}
}

const USkillDefinition* UPandoraSkillSource::GetSkillDataAsset() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition ? Definition->GetSkillDefinition(SkillIndex) : nullptr;
}

void UPandoraSkillSource::GetCooldownTimeRemainingAndDuration(float& OutRemaining, float& OutDuration) const
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;

	const APdPlayerState* PlayerState = GetTypedOuter<APdPlayerState>();
	const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (!ASC || !ASC->GetWorld())
	{
		return;
	}

	// 선택 변경 후에도 같은 출처를 유지하므로, 현재 슬롯 대신 이 객체로 효과를 구분한다.
	FGameplayEffectQuery Query;
	Query.EffectSource = this;
	// 기존 추가 알림과 동일하게, Cooldown을 부여하는 효과만 조회한다.
	Query.CustomMatchDelegate.BindLambda([](const FActiveGameplayEffect& Effect)
	{
		FGameplayTagContainer GrantedTags;
		Effect.Spec.GetAllGrantedTags(GrantedTags);
		return GrantedTags.HasTag(LabGameplayTags::Cooldown);
	});
	for (const TPair<float, float>& Time : ASC->GetActiveEffectsTimeRemainingAndDuration(Query))
	{
		if (Time.Key > OutRemaining)
		{
			OutRemaining = Time.Key;
			OutDuration = Time.Value;
		}
	}
}

// 현재 선택과 무관하게, 서버가 부여한 원래 판도라와 스킬 정보를 클라이언트에 전달한다.
void UPandoraSkillSource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PandoraDefinition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SkillIndex, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LoadoutDirection, Params);
}

void UPandoraSkillSource::OnRep_Source()
{
	if (UPandoraComponent* Pandora = GetTypedOuter<UPandoraComponent>())
	{
		Pandora->HandleSkillSourceReplicated(this);
	}
}

void UPandoraSkillSource::PreDestroyFromReplication()
{
	if (UPandoraComponent* Pandora = GetTypedOuter<UPandoraComponent>())
	{
		Pandora->HandleSkillSourceDestroyed(this);
	}
	Super::PreDestroyFromReplication();
}
