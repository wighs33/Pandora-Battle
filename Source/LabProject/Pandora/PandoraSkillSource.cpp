#include "Pandora/PandoraSkillSource.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillSource)

void UPandoraSkillSource::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const int32 InSkillIndex,
	const int32 InPandoraLevel,
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

const USkillDefinition* UPandoraSkillSource::GetSkillDataAsset() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition ? Definition->GetSkillDefinition(SkillIndex) : nullptr;
}

void UPandoraSkillSource::GetCooldownTimeRemainingAndDuration(float& OutRemaining, float& OutDuration) const
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;

	const UAbilitySystemComponent* ASC = GetTypedOuter<UAbilitySystemComponent>();
	const UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(CooldownEffectHandle);
	if (!Effect || Effect->IsPendingRemove)
	{
		return;
	}
	const float Remaining = Effect->GetTimeRemaining(World->GetTimeSeconds());
	if (Remaining > 0.0f)
	{
		OutRemaining = Remaining;
		OutDuration = Effect->GetDuration();
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

void UPandoraSkillSource::SetCooldownEffectHandle(const FActiveGameplayEffectHandle EffectHandle)
{
	if (EffectHandle.IsValid())
	{
		CooldownEffectHandle = EffectHandle;
	}
}

void UPandoraSkillSource::ClearCooldownEffectHandle(const FActiveGameplayEffectHandle EffectHandle)
{
	// 이전 효과의 제거 알림이 나중에 도착해도 새 쿨다운 핸들은 유지한다.
	if (CooldownEffectHandle == EffectHandle)
	{
		CooldownEffectHandle.Invalidate();
	}
}
