#pragma once

#include "CoreMinimal.h"
#include "GameFeature/GameFeatureAction_WorldNetworkBase.h"
#include "GameFeatureAction_SpawnActors.generated.h"

class AActor;
class UWorld;
struct FAssetBundleData;
struct FGameFeatureDeactivatingContext;
struct FGameFeatureStateChangeContext;
struct FStreamableHandle;
struct FWorldContext;

/** 피처가 켜진 맵에 놓을 액터 하나. */
USTRUCT()
struct FGameFeatureSpawnActorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "!Spawn")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "!Spawn")
	FTransform Transform;
};

/** 한 맵에 놓을 액터 목록. */
USTRUCT()
struct FGameFeatureSpawnWorldEntry
{
	GENERATED_BODY()

	/** 이 맵이 열렸을 때만 놓는다. */
	UPROPERTY(EditAnywhere, Category = "!Spawn")
	TSoftObjectPtr<UWorld> TargetWorld;

	UPROPERTY(EditAnywhere, Category = "!Spawn")
	TArray<FGameFeatureSpawnActorEntry> Actors;
};

/**
 * 피처가 켜진 월드에 액터를 놓고, 피처가 꺼지면 치운다.
 * 한 모드에만 있어야 하는 액터(보스 스포너 등)를 맵에 직접 두면 그 맵을 쓰는 다른 모드도 액터의 콘텐츠를 함께 불러온다.
 * 피처가 놓으면 그 모드에서만, 피처가 켜질 때 비동기로 불러온다.
 * 액터는 권한이 있는 쪽(서버·단독 실행)에서만 놓는다. 클라이언트는 클래스만 미리 불러 두어, 복제된 액터가 도착할 때 동기 로딩으로 멈추지 않게 한다.
 */
UCLASS(meta = (DisplayName = "Spawn Actors"))
class LABPROJECT_API UGameFeatureAction_SpawnActors : public UGameFeatureAction_WorldNetworkBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "!Spawn")
	TArray<FGameFeatureSpawnWorldEntry> SpawnEntries;

protected:
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

private:
	struct FSpawnedActors
	{
		TSharedPtr<FStreamableHandle> LoadHandle;
		TArray<TWeakObjectPtr<AActor>> Actors;
	};

	TArray<const FGameFeatureSpawnActorEntry*> FindEntriesForWorld(const UWorld& World) const;
	void SpawnLoadedActors(TWeakObjectPtr<UWorld> WeakWorld, FGameFeatureStateChangeContext ChangeContext);

	TMap<FGameFeatureStateChangeContext, FSpawnedActors> SpawnedActorsByContext;
};
