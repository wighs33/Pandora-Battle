#include "GameFeature/GameFeatureAction_SpawnActors.h"

#include "AssetRegistry/AssetBundleData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_SpawnActors)

DEFINE_LOG_CATEGORY_STATIC(LogGameFeatureSpawnActors, Log, All);

// 피처가 꺼지면 아직 끝나지 않은 로딩을 멈추고, 놓은 액터를 치운다.
void UGameFeatureAction_SpawnActors::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FSpawnedActors* Spawned = SpawnedActorsByContext.Find(ChangeContext);
	if (!Spawned)
	{
		return;
	}
	if (Spawned->LoadHandle.IsValid())
	{
		Spawned->LoadHandle->CancelHandle();
	}
	for (const TWeakObjectPtr<AActor>& Actor : Spawned->Actors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	SpawnedActorsByContext.Remove(ChangeContext);
}

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_SpawnActors::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	for (const FGameFeatureSpawnWorldEntry& WorldEntry : SpawnEntries)
	{
		for (const FGameFeatureSpawnActorEntry& ActorEntry : WorldEntry.Actors)
		{
			if (!ActorEntry.ActorClass.IsNull())
			{
				AddToActionBundles(AssetBundleData, ActorEntry.ActorClass.ToSoftObjectPath().GetAssetPath());
			}
		}
	}
}
#endif

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_SpawnActors::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	if (!bClientAction && !bServerAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_SpawnActors", "MissingNetwork", "At least one Network option is required."));
	}
	for (const FGameFeatureSpawnWorldEntry& WorldEntry : SpawnEntries)
	{
		if (WorldEntry.TargetWorld.IsNull())
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(NSLOCTEXT("PdGameFeatureAction_SpawnActors", "MissingWorld", "Every spawn entry needs a TargetWorld."));
		}
		for (const FGameFeatureSpawnActorEntry& ActorEntry : WorldEntry.Actors)
		{
			if (ActorEntry.ActorClass.IsNull())
			{
				Result = EDataValidationResult::Invalid;
				Context.AddError(NSLOCTEXT("PdGameFeatureAction_SpawnActors", "MissingClass", "Every spawned actor needs an ActorClass."));
			}
		}
	}
	return Result;
}
#endif

// 이 맵에 놓을 액터 클래스를 비동기로 불러온 뒤 놓는다. 이미 불러온 클래스면 바로 놓는다.
void UGameFeatureAction_SpawnActors::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	const TArray<const FGameFeatureSpawnActorEntry*> Entries = World ? FindEntriesForWorld(*World) : TArray<const FGameFeatureSpawnActorEntry*>();
	if (Entries.IsEmpty())
	{
		return;
	}

	TArray<FSoftObjectPath> ClassPaths;
	for (const FGameFeatureSpawnActorEntry* Entry : Entries)
	{
		ClassPaths.AddUnique(Entry->ActorClass.ToSoftObjectPath());
	}
	FSpawnedActors& Spawned = SpawnedActorsByContext.FindOrAdd(ChangeContext);
	Spawned.LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(ClassPaths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::SpawnLoadedActors, TWeakObjectPtr<UWorld>(World), ChangeContext));
}

TArray<const FGameFeatureSpawnActorEntry*> UGameFeatureAction_SpawnActors::FindEntriesForWorld(const UWorld& World) const
{
	// PIE 월드는 패키지 이름 앞에 "UEDPIE_n_"가 붙으므로 떼고 비교한다.
	const FString WorldPackageName = UWorld::RemovePIEPrefix(World.GetOutermost()->GetName());
	TArray<const FGameFeatureSpawnActorEntry*> Entries;
	for (const FGameFeatureSpawnWorldEntry& WorldEntry : SpawnEntries)
	{
		if (WorldEntry.TargetWorld.ToSoftObjectPath().GetLongPackageName() != WorldPackageName)
		{
			continue;
		}
		for (const FGameFeatureSpawnActorEntry& ActorEntry : WorldEntry.Actors)
		{
			if (!ActorEntry.ActorClass.IsNull())
			{
				Entries.Add(&ActorEntry);
			}
		}
	}
	return Entries;
}

// 로딩이 끝났을 때 피처가 아직 켜져 있고 월드가 남아 있으면 놓는다. 클라이언트는 불러 둔 클래스만 들고 있는다.
void UGameFeatureAction_SpawnActors::SpawnLoadedActors(const TWeakObjectPtr<UWorld> WeakWorld, const FGameFeatureStateChangeContext ChangeContext)
{
	UWorld* World = WeakWorld.Get();
	FSpawnedActors* Spawned = SpawnedActorsByContext.Find(ChangeContext);
	if (!World || !Spawned || World->GetNetMode() == NM_Client)
	{
		return;
	}
	for (const FGameFeatureSpawnActorEntry* Entry : FindEntriesForWorld(*World))
	{
		UClass* ActorClass = Entry->ActorClass.Get();
		if (!ActorClass)
		{
			UE_LOG(LogGameFeatureSpawnActors, Error, TEXT("Could not load %s for %s."), *Entry->ActorClass.ToString(), *GetPathName());
			continue;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Actor = World->SpawnActor<AActor>(ActorClass, Entry->Transform, SpawnParameters))
		{
			Spawned->Actors.Add(Actor);
		}
	}
}
