#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "PandoraDefinition.generated.h"

class UGameplayAbility;
class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(PandoraDefinitionLog, Log, All);

USTRUCT(BlueprintType, Blueprintable)
struct FSkill
{
	GENERATED_BODY()

public:
	/** 스킬 표시 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText DisplayName;

	/** 스킬 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText Description;

	/** 스킬 아이콘 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
	
	/** 어빌리티 클래스 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="!Pandora")
	TSubclassOf<UGameplayAbility> AbilityClass;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPandoraDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	/** 아이템 표시 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText DisplayName;

	/** 아이템 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FText Description;

	/** 아이콘 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
	
	/** 게임 플레이 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Pandora")
	FGameplayTag IdTag;

	/** 스킬 목록 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Pandora")
	TArray<FSkill> Skill;
	
	/** 티어 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	int Tier;
};
