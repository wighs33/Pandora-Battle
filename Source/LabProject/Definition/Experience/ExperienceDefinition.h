#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceDefinition.generated.h"

class APawn;

/**
 * 맵에서 사용할 Pawn과 활성화할 GameFeature 구성을 정의한다.
 *
 * 로딩 상태와 활성화 수명은 각 월드의 ExperienceManagerComponent가 관리한다.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	//------------------------------------------------------------------------------------------------------------------

	bool ResolveGameFeaturePluginURLs(TArray<FString>& OutPluginURLs, FText& OutError) const;

	// 미지정 시 해당 GameMode의 기본 Pawn을 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Gameplay")
	TSubclassOf<APawn> DefaultPawnClass;

	// 기존 에셋 선택 방식을 유지한다. GameFeatureData 에셋 이름은 플러그인 이름과 같아야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience|Game Features", meta = (AllowedTypes = "GameFeatureData"))
	TArray<FPrimaryAssetId> GameFeaturesToEnable;
};
