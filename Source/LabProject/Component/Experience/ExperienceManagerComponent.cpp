#include "Component/Experience/ExperienceManagerComponent.h"

#include "Definition/Experience/ExperienceDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFeaturesSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceManagerComponent)

DEFINE_LOG_CATEGORY(PdExperienceManagerLog);

namespace
{
	// GameFeature는 프로세스 공용이므로 한 PIE 월드의 종료가 다른 월드의 기능을 끄지 않도록 추적한다.
	TMap<FString, TSet<TWeakObjectPtr<UExperienceManagerComponent>>> ExperienceGameFeatureReferences;

	void RemoveStaleExperienceGameFeatureReferences(TSet<TWeakObjectPtr<UExperienceManagerComponent>>& References)
	{
		for (auto Iterator = References.CreateIterator(); Iterator; ++Iterator)
		{
			if (!Iterator->IsValid())
			{
				Iterator.RemoveCurrent();
			}
		}
	}

	void AcquireExperienceGameFeatureReference(UExperienceManagerComponent* ExperienceManager, const FString& PluginURL)
	{
		TSet<TWeakObjectPtr<UExperienceManagerComponent>>& References = ExperienceGameFeatureReferences.FindOrAdd(PluginURL);
		RemoveStaleExperienceGameFeatureReferences(References);
		References.Add(ExperienceManager);
	}

	bool ReleaseExperienceGameFeatureReference(UExperienceManagerComponent* ExperienceManager, const FString& PluginURL)
	{
		TSet<TWeakObjectPtr<UExperienceManagerComponent>>* References = ExperienceGameFeatureReferences.Find(PluginURL);
		if (!References || References->Remove(ExperienceManager) == 0)
		{
			return false;
		}

		RemoveStaleExperienceGameFeatureReferences(*References);
		if (!References->IsEmpty())
		{
			return false;
		}

		ExperienceGameFeatureReferences.Remove(PluginURL);
		return true;
	}
}

// 서버의 Experience 선택을 클라이언트에도 전달할 수 있도록 복제를 활성화한다.
UExperienceManagerComponent::UExperienceManagerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

// 로딩 결과가 아니라 선택한 ID를 복제한다. 각 월드는 자신의 로딩 완료를 따로 판단한다.
void UExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UExperienceManagerComponent, CurrentExperienceId, Params);
}

// 맵을 떠날 때 이 월드의 에셋 보유와 GameFeature 사용 권한을 반납한다.
void UExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateExperience();
	Super::EndPlay(EndPlayReason);
}

// 서버가 이 월드에서 사용할 Experience를 한 번 선택하고 로딩을 시작한다.
void UExperienceManagerComponent::SetCurrentExperienceAuth(FPrimaryAssetId ExperienceId)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(PdExperienceManagerLog, Warning, TEXT("SetCurrentExperienceAuth ignored because the owner has no authority."));
		return;
	}
	if (!ExperienceId.IsValid())
	{
		UE_LOG(PdExperienceManagerLog, Error, TEXT("SetCurrentExperienceAuth ignored because ExperienceId is invalid."));
		return;
	}
	if (LoadState != EExperienceLoadState::Unloaded)
	{
		UE_LOG(PdExperienceManagerLog, Warning,
			TEXT("SetCurrentExperienceAuth ignored because an experience is already active or loading. Current=%s Requested=%s State=%d"),
			*CurrentExperienceId.ToString(), *ExperienceId.ToString(), static_cast<int32>(LoadState));
		return;
	}

	CurrentExperienceId = ExperienceId;
	MARK_PROPERTY_DIRTY_FROM_NAME(UExperienceManagerComponent, CurrentExperienceId, this);
	StartExperienceLoad();
}

// 준비가 끝난 Experience 설정을 스폰과 초기화 로직에 제공한다.
const UExperienceDefinition* UExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	check(CurrentExperience);
	return CurrentExperience;
}

// 이미 준비됐다면 즉시 알리고, 아직 로딩 중이라면 완료 시 실행할 작업을 등록한다.
FDelegateHandle UExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.ExecuteIfBound(CurrentExperience);
		return FDelegateHandle();
	}
	return OnExperienceLoaded.Add(MoveTemp(Delegate));
}

// 뒤늦게 구독한 호출자도 실패 원인을 받을 수 있도록 마지막 실패 정보를 보관한다.
FDelegateHandle UExperienceManagerComponent::CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate Delegate)
{
	if (HasExperienceLoadFailed())
	{
		Delegate.ExecuteIfBound(LastFailedExperienceId, LastLoadFailureMessage);
		return FDelegateHandle();
	}
	return OnExperienceLoadFailed.Add(MoveTemp(Delegate));
}

// 대기하던 AI 등이 먼저 종료되면 더 이상 필요 없는 준비 완료 구독을 해제한다.
void UExperienceManagerComponent::RemoveOnExperienceLoaded(const FDelegateHandle DelegateHandle)
{
	if (DelegateHandle.IsValid())
	{
		OnExperienceLoaded.Remove(DelegateHandle);
	}
}

// 서버가 선택한 ID를 받은 클라이언트도 같은 Experience의 로딩을 시작한다.
void UExperienceManagerComponent::HandleCurrentExperienceIdReplicated()
{
	StartExperienceLoad();
}

// 다른 월드의 로딩 상태와 독립적인 핸들로 Experience와 그 하드 참조를 비동기 로드한다.
void UExperienceManagerComponent::StartExperienceLoad()
{
	if (!CurrentExperienceId.IsValid() || LoadState != EExperienceLoadState::Unloaded)
	{
		return;
	}

	LoadState = EExperienceLoadState::Loading;
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();
	UAssetManager& AssetManager = UAssetManager::Get();
	if (!AssetManager.GetPrimaryAssetPath(CurrentExperienceId).IsValid())
	{
		FailExperienceLoad(CurrentExperienceId,
			FString::Printf(TEXT("Experience asset is not registered: %s"), *CurrentExperienceId.ToString()));
		return;
	}

	FAssetManagerLoadParams LoadParams;
	LoadParams.OnComplete =
		FStreamableDelegateWithHandle::CreateUObject(this, &ThisClass::HandleExperienceAssetLoaded, CurrentExperienceId);
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		AssetManager.PreloadPrimaryAssets({CurrentExperienceId}, {}, false, MoveTemp(LoadParams));
	if (!NewLoadHandle.IsValid())
	{
		FailExperienceLoad(CurrentExperienceId, TEXT("Experience asset preload could not be started."));
		return;
	}

	// 즉시 완료 콜백에서 실패하거나 맵이 종료됐다면 이미 정리한 핸들을 다시 보관하지 않는다.
	if (LoadState == EExperienceLoadState::Failed || LoadState == EExperienceLoadState::Deactivating)
	{
		NewLoadHandle->CancelHandle();
		return;
	}
	ExperienceLoadHandle = MoveTemp(NewLoadHandle);
}

// 로드한 에셋이 실제 Experience인지 확인하고 필요한 GameFeature 활성화를 이어간다.
void UExperienceManagerComponent::HandleExperienceAssetLoaded(
	TSharedPtr<FStreamableHandle> LoadHandle, FPrimaryAssetId LoadedExperienceId)
{
	if (LoadedExperienceId != CurrentExperienceId || LoadState != EExperienceLoadState::Loading)
	{
		return;
	}

	if (!LoadHandle.IsValid() || LoadHandle->HasError())
	{
		FailExperienceLoad(LoadedExperienceId,
			FString::Printf(TEXT("Experience asset preload failed: %s"), *LoadedExperienceId.ToString()));
		return;
	}
	ExperienceLoadHandle = MoveTemp(LoadHandle);
	CurrentExperience = Cast<UExperienceDefinition>(UAssetManager::Get().GetPrimaryAssetObject(LoadedExperienceId));
	if (!CurrentExperience)
	{
		FailExperienceLoad(LoadedExperienceId,
			FString::Printf(TEXT("Experience asset could not be loaded: %s"), *LoadedExperienceId.ToString()));
		return;
	}
	StartGameFeatureLoads();
}

// 전체 설정 검증과 공유 사용 등록을 마친 뒤 엔진의 일괄 활성화를 요청한다.
void UExperienceManagerComponent::StartGameFeatureLoads()
{
	TArray<FString> PluginURLs;
	FText Error;
	if (!CurrentExperience->ResolveGameFeaturePluginURLs(PluginURLs, Error))
	{
		FailExperienceLoad(CurrentExperienceId, Error.ToString());
		return;
	}

	LoadState = EExperienceLoadState::LoadingGameFeatures;
	GameFeaturePluginURLs = PluginURLs;
	for (const FString& PluginURL : PluginURLs)
	{
		AcquireExperienceGameFeatureReference(this, PluginURL);
	}

	UGameFeaturesSubsystem::Get().LoadAndActivateGameFeaturePlugin(
		PluginURLs, FGameFeatureProtocolOptions(),
		FMultipleGameFeaturePluginsLoaded::CreateUObject(this, &ThisClass::HandleGameFeaturesLoaded));
}

// 개별 플러그인이 즉시 완료되더라도 전체 요청이 끝난 결과만 준비 판정에 사용한다.
void UExperienceManagerComponent::HandleGameFeaturesLoaded(const TMap<FString, UE::GameFeatures::FResult>& Results)
{
	if (LoadState != EExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	for (const TPair<FString, UE::GameFeatures::FResult>& Entry : Results)
	{
		if (Entry.Value.HasError())
		{
			FailExperienceLoad(CurrentExperienceId,
				FString::Printf(TEXT("GameFeature load failed: %s (%s)"), *Entry.Key, *Entry.Value.GetError()));
			return;
		}
	}
	FinishExperienceLoad();
}

// 이 월드의 Experience 준비를 기다리던 스폰·AI·확장 초기화 작업을 재개한다.
void UExperienceManagerComponent::FinishExperienceLoad()
{
	if (LoadState != EExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	LoadState = EExperienceLoadState::Loaded;
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();
	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Clear();
}

// 실패 상태를 먼저 확정해 해제 중 들어오는 콜백을 막고, 정리 후 호출자에게 실패를 알린다.
void UExperienceManagerComponent::FailExperienceLoad(FPrimaryAssetId FailedExperienceId, FString FailureMessage)
{
	if (LoadState != EExperienceLoadState::Loading && LoadState != EExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	LoadState = EExperienceLoadState::Failed;
	LastFailedExperienceId = FailedExperienceId;
	LastLoadFailureMessage = MoveTemp(FailureMessage);
	UE_LOG(PdExperienceManagerLog, Error, TEXT("%s"), *LastLoadFailureMessage);

	ReleaseRuntimeResources();
	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Broadcast(LastFailedExperienceId, LastLoadFailureMessage);
	OnExperienceLoadFailed.Clear();
}

// 종료된 월드에서는 늦게 도착한 로딩 결과나 새 요청으로 Experience를 다시 시작하지 않는다.
void UExperienceManagerComponent::DeactivateExperience()
{
	if (LoadState == EExperienceLoadState::Deactivating)
	{
		return;
	}

	LoadState = EExperienceLoadState::Deactivating;
	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Clear();
	ReleaseRuntimeResources();
	CurrentExperienceId = FPrimaryAssetId();
	if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UExperienceManagerComponent, CurrentExperienceId, this);
	}
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();
}

// 실패와 맵 종료가 공유하는 정리 경로다. 공용 에셋을 Unload하지 않고 이 Manager의 보유만 해제한다.
void UExperienceManagerComponent::ReleaseRuntimeResources()
{
	TArray<FString> PluginURLsToRelease = MoveTemp(GameFeaturePluginURLs);
	GameFeaturePluginURLs.Reset();
	for (const FString& PluginURL : PluginURLsToRelease)
	{
		if (ReleaseExperienceGameFeatureReference(this, PluginURL))
		{
			UGameFeaturesSubsystem::Get().DeactivateGameFeaturePlugin(PluginURL);
		}
	}

	if (ExperienceLoadHandle.IsValid())
	{
		ExperienceLoadHandle->CancelHandle();
		ExperienceLoadHandle.Reset();
	}
	CurrentExperience = nullptr;
}
