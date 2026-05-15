#include "GameFeature/Extension/ActorExtension.h"

#include "AssetRegistry/AssetBundleData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorExtension)

bool FActorExtension::CanActivate(AActor* Owner) const
{
	for (const TInstancedStruct<FExtensionCondition>& ConditionStruct : Conditions)
	{
		const FExtensionCondition* Condition = ConditionStruct.GetPtr();
		if (Condition && !Condition->IsSatisfied(Owner))
		{
			return false;
		}
	}

	return true;
}

void FActorExtension::OnActivate(AActor* Owner)
{
	if (bActivated)
	{
		return;
	}

	bActivated = true;
	for (const TInstancedStruct<FExtensionExecute>& ExecuteStruct : Executes)
	{
		const FExtensionExecute* Execute = ExecuteStruct.GetPtr();
		if (Execute)
		{
			Execute->OnActivate(Owner);
		}
	}
}

void FActorExtension::OnDeactivate(AActor* Owner)
{
	if (!bActivated)
	{
		return;
	}

	for (const TInstancedStruct<FExtensionExecute>& ExecuteStruct : Executes)
	{
		const FExtensionExecute* Execute = ExecuteStruct.GetPtr();
		if (Execute)
		{
			Execute->OnDeactivate(Owner);
		}
	}

	bActivated = false;
}

#if WITH_EDITORONLY_DATA
void FActorExtension::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const
{
	for (const TInstancedStruct<FExtensionExecute>& ExecuteStruct : Executes)
	{
		const FExtensionExecute* Execute = ExecuteStruct.GetPtr();
		if (Execute)
		{
			Execute->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif
