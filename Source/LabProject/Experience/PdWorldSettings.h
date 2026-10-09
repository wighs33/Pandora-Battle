#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PdWorldSettings.generated.h"

UCLASS()
class LABPROJECT_API APdWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	APdWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 월드가 이 월드 설정을 쓰면 그 Experience를, 아니면 빈 값을 돌려준다.
	// 보스 레이드로 연 월드는 BossRaidExperienceId가 있으면 그것을 쓴다(보스 레이드 콘텐츠는 그 Experience가 켜는 GameFeature에 있다).
	static FPrimaryAssetId FindExperienceId(const UWorld* World, bool bBossRaid);

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition", AllowPrivateAccess = "true"))
	FPrimaryAssetId DefaultExperienceId;

	/** 이 맵을 보스 레이드로 열 때 쓰는 Experience. 비우면 보스 레이드에서도 DefaultExperienceId를 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Experience", meta = (AllowedTypes = "ExperienceDefinition", AllowPrivateAccess = "true"))
	FPrimaryAssetId BossRaidExperienceId;
};
