#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EffectSetByCallerDefinition.generated.h"

/**
 * <이펙트 SetByCaller 태그 정의 데이터 애셋>
 * - 프로젝트에서 공통으로 사용하는 SetByCaller 태그를 한 곳에 모아 관리합니다.
 * - 데미지 수치 전달용 태그와 StatUp Operation 전달용 태그를 제공합니다.
 * - 에디터의 Data 하위 태그만 선택할 수 있도록 제한합니다.
 */
UCLASS(BlueprintType)
class LABPROJECT_API UEffectSetByCallerDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|SetByCaller")
	const FGameplayTag& GetDamageMagnitudeTag() const { return DamageMagnitudeTag; }

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem|SetByCaller")
	const FGameplayTag& GetStatUpOperationTag() const { return StatUpOperationTag; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|SetByCaller", meta = (Categories = "Data"))
	FGameplayTag DamageMagnitudeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|SetByCaller", meta = (Categories = "Data"))
	FGameplayTag StatUpOperationTag;
};
