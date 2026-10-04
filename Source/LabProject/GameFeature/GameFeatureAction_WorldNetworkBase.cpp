#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeaturesSubsystemSettings.h"

#if WITH_EDITORONLY_DATA
#include "AssetRegistry/AssetBundleData.h"
#endif

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

	// 맵 로딩은 월드를 InitWorld보다 먼저 현재 월드로 바꾼다. 월드 서브시스템이 생긴 뒤인 초기화 완료 시점에 적용한다.
	PostWorldInitializationHandles.Add(ChangeContext, FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this,
		&ThisClass::HandlePostWorldInitialization,
		ChangeContext));

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		AddToWorldIfReady(WorldContext, ChangeContext);
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

	if (FDelegateHandle* PostInitializationHandle = PostWorldInitializationHandles.Find(ChangeContext))
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(*PostInitializationHandle);
		PostWorldInitializationHandles.Remove(ChangeContext);
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

	if (const FWorldContext* WorldContext = GameInstance->GetWorldContext())
	{
		AddToWorldIfReady(*WorldContext, ChangeContext);
	}
}

void UGameFeatureAction_WorldNetworkBase::HandleGameInstanceWorldChanged(UGameInstance* GameInstance, UWorld* OldWorld,
	UWorld* NewWorld, FGameFeatureStateChangeContext ChangeContext)
{
	static_cast<void>(OldWorld);
	static_cast<void>(NewWorld);

	// 아직 초기화 전인 월드는 HandlePostWorldInitialization이 이어서 처리한다.
	if (const FWorldContext* WorldContext = GameInstance ? GameInstance->GetWorldContext() : nullptr)
	{
		AddToWorldIfReady(*WorldContext, ChangeContext);
	}
}

void UGameFeatureAction_WorldNetworkBase::HandlePostWorldInitialization(UWorld* World,
	const UWorld::InitializationValues IVS, FGameFeatureStateChangeContext ChangeContext)
{
	static_cast<void>(IVS);

	// 월드 컨텍스트의 현재 월드가 된 뒤에 초기화된 경우만 처리한다. 먼저 초기화된 월드는 월드 변경 알림이 처리한다.
	const FWorldContext* WorldContext = GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (WorldContext && WorldContext->World() == World)
	{
		AddToWorldIfReady(*WorldContext, ChangeContext);
	}
}

void UGameFeatureAction_WorldNetworkBase::AddToWorldIfReady(const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (World && World->IsGameWorld() && World->bIsWorldInitialized
		&& ChangeContext.ShouldApplyToWorldContext(WorldContext)
		&& ShouldApplyToNetMode(World->GetNetMode()))
	{
		AddToWorld(WorldContext, ChangeContext);
	}
}

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_WorldNetworkBase::AddToActionBundles(FAssetBundleData& AssetBundleData, const FTopLevelAssetPath& AssetPath) const
{
	if (bClientAction)
	{
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, AssetPath);
	}

	if (bServerAction)
	{
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, AssetPath);
	}
}
#endif

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
