#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_WorldNetworkBase)

UGameFeatureAction_WorldNetworkBase::UGameFeatureAction_WorldNetworkBase()
	: bClientAction(false)
	, bServerAction(false)
{
}

//----------------------------------------------------------------------------------------------------------------------
//--- Game Feature Events
void UGameFeatureAction_WorldNetworkBase::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
	const FGameFeatureStateChangeContext ChangeContext(Context);

	GameInstanceStartHandles.Add(ChangeContext, FWorldDelegates::OnStartGameInstance.AddUObject(
		this,
		&ThisClass::HandleGameInstanceStart,
		ChangeContext));

	GameInstanceWorldChangedHandles.Add(ChangeContext, FWorldDelegates::OnGameInstanceWorldChanged.AddUObject(
		this,
		&ThisClass::HandleGameInstanceWorldChanged,
		ChangeContext));

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (!Context.ShouldApplyToWorldContext(WorldContext))
		{
			continue;
		}

		UWorld* World = WorldContext.World();
		if (World && World->IsGameWorld() && ShouldApplyToNetMode(World->GetNetMode()))
		{
			AddToWorld(WorldContext, ChangeContext);
		}
	}
}

void UGameFeatureAction_WorldNetworkBase::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	const FGameFeatureStateChangeContext ChangeContext(Context);

	if (FDelegateHandle* StartHandle = GameInstanceStartHandles.Find(ChangeContext))
	{
		FWorldDelegates::OnStartGameInstance.Remove(*StartHandle);
		GameInstanceStartHandles.Remove(ChangeContext);
	}

	if (FDelegateHandle* WorldChangedHandle = GameInstanceWorldChangedHandles.Find(ChangeContext))
	{
		FWorldDelegates::OnGameInstanceWorldChanged.Remove(*WorldChangedHandle);
		GameInstanceWorldChangedHandles.Remove(ChangeContext);
	}
}

//----------------------------------------------------------------------------------------------------------------------
//--- World Events
void UGameFeatureAction_WorldNetworkBase::HandleGameInstanceStart(UGameInstance* GameInstance,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!GameInstance)
	{
		return;
	}

	const FWorldContext* WorldContext = GameInstance->GetWorldContext();
	UWorld* World = WorldContext ? WorldContext->World() : nullptr;
	if (WorldContext && World && World->IsGameWorld()
		&& ChangeContext.ShouldApplyToWorldContext(*WorldContext)
		&& ShouldApplyToNetMode(World->GetNetMode()))
	{
		AddToWorld(*WorldContext, ChangeContext);
	}
}

void UGameFeatureAction_WorldNetworkBase::HandleGameInstanceWorldChanged(UGameInstance* GameInstance, UWorld* OldWorld,
	UWorld* NewWorld, FGameFeatureStateChangeContext ChangeContext)
{
	static_cast<void>(OldWorld);

	if (!GameInstance || !NewWorld || !NewWorld->IsGameWorld())
	{
		return;
	}

	const FWorldContext* WorldContext = GameInstance->GetWorldContext();
	if (WorldContext
		&& ChangeContext.ShouldApplyToWorldContext(*WorldContext)
		&& ShouldApplyToNetMode(NewWorld->GetNetMode()))
	{
		AddToWorld(*WorldContext, ChangeContext);
	}
}

bool UGameFeatureAction_WorldNetworkBase::ShouldApplyToNetMode(ENetMode NetMode) const
{
	switch (NetMode)
	{
	case NM_Client:
		return bClientAction;
	case NM_DedicatedServer:
		return bServerAction;
	case NM_ListenServer:
	case NM_Standalone:
		return bClientAction || bServerAction;
	default:
		return false;
	}
}
