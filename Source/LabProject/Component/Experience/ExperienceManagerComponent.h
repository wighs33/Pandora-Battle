#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GameFeaturePluginOperationResult.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceManagerComponent.generated.h"

class UExperienceDefinition;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(PdExperienceManagerLog, Log, All);

UENUM(BlueprintType)
enum class EExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	Loaded,
	Failed,
	Deactivating
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPdExperienceLoaded, const UExperienceDefinition*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPdExperienceLoadFailed, FPrimaryAssetId, const FString&);

/**
 * 서버가 선택한 Experience를 각 월드에서 로드하고 준비 결과를 알린다.
 *
 * Loaded는 이 월드의 에셋·GameFeature 준비 완료이며, 모든 클라이언트의 입장 완료를 뜻하지 않는다.
 * 경기 시작과 실패 후 대체 실행 정책은 GameMode 등 호출자가 결정한다.
 */
UCLASS()
class LABPROJECT_API UExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------

	void SetCurrentExperienceAuth(FPrimaryAssetId ExperienceId);
	bool IsExperienceLoaded() const { return LoadState == EExperienceLoadState::Loaded; }
	bool HasExperienceLoadFailed() const { return LoadState == EExperienceLoadState::Failed; }
	EExperienceLoadState GetLoadState() const { return LoadState; }
	const UExperienceDefinition* GetCurrentExperienceChecked() const;
	FDelegateHandle CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate Delegate);
	FDelegateHandle CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate Delegate);
	void RemoveOnExperienceLoaded(FDelegateHandle DelegateHandle);

private:
	UFUNCTION()
	void HandleCurrentExperienceIdReplicated();

	void StartExperienceLoad();
	void HandleExperienceAssetLoaded(TSharedPtr<FStreamableHandle> LoadHandle, FPrimaryAssetId LoadedExperienceId);
	void StartGameFeatureLoads();
	void HandleGameFeaturesLoaded(const TMap<FString, UE::GameFeatures::FResult>& Results);
	void FinishExperienceLoad();
	void FailExperienceLoad(FPrimaryAssetId FailedExperienceId, FString FailureMessage);
	void DeactivateExperience();
	void ReleaseRuntimeResources();

	UPROPERTY(ReplicatedUsing = HandleCurrentExperienceIdReplicated)
	FPrimaryAssetId CurrentExperienceId;

	UPROPERTY(Transient)
	TObjectPtr<const UExperienceDefinition> CurrentExperience;

	EExperienceLoadState LoadState = EExperienceLoadState::Unloaded;
	TSharedPtr<FStreamableHandle> ExperienceLoadHandle;
	TArray<FString> GameFeaturePluginURLs;
	FPrimaryAssetId LastFailedExperienceId;
	FString LastLoadFailureMessage;
	FOnPdExperienceLoaded OnExperienceLoaded;
	FOnPdExperienceLoadFailed OnExperienceLoadFailed;
};
