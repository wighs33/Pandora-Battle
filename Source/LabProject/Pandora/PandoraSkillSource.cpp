#include "Pandora/PandoraSkillSource.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraSkillSourceOwner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillSource)

namespace
{
	IPandoraSkillSourceOwner* FindSourceOwner(const UObject* Source)
	{
		for (UObject* Outer = Source->GetOuter(); Outer; Outer = Outer->GetOuter())
		{
			if (IPandoraSkillSourceOwner* Owner = Cast<IPandoraSkillSourceOwner>(Outer))
			{
				return Owner;
			}
		}
		return nullptr;
	}
}

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
	SetLoadoutDirection(InLoadoutDirection);
}

void UPandoraSkillSource::SetLoadoutDirection(const EEnum_Direction InLoadoutDirection)
{
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
	if (IPandoraSkillSourceOwner* Owner = FindSourceOwner(this))
	{
		Owner->HandleSkillSourceReplicated(this);
	}
}

void UPandoraSkillSource::PreDestroyFromReplication()
{
	if (IPandoraSkillSourceOwner* Owner = FindSourceOwner(this))
	{
		Owner->HandleSkillSourceDestroyed(this);
	}
	Super::PreDestroyFromReplication();
}
