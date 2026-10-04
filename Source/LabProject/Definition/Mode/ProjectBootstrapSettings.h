#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/PrimaryAssetId.h"
#include "ProjectBootstrapSettings.generated.h"

class UPdGameInstanceDefinition;
class UWidgetComponent;

/**
 * 네이티브 코드에 콘텐츠 이름을 넣지 않고 프로젝트 초기화 에셋을 선택하며,
 * 게임 진입 시 미리 로드할 콘텐츠 목록을 관리한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Project Bootstrap"))
class LABPROJECT_API UProjectBootstrapSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// Public API ------------------------------------------------------------------------------------------------------
	const TSoftObjectPtr<UPdGameInstanceDefinition>& GetBootstrapDefinition() const
	{
		return BootstrapDefinition;
	}
	const TArray<FPrimaryAssetId>& GetGameEntryRequiredPrimaryAssets() const
	{
		return GameEntryRequiredPrimaryAssets;
	}
	const TArray<FPrimaryAssetType>& GetGameEntryRequiredPrimaryAssetTypes() const
	{
		return GameEntryRequiredPrimaryAssetTypes;
	}
	const TSoftClassPtr<UWidgetComponent>& GetCharacterHealthBarComponentClass() const
	{
		return CharacterHealthBarComponentClass;
	}

private:
	UPROPERTY(Config, EditAnywhere, Category = "Content",
		meta = (AllowedTypes = "GameInstanceDefinition",
			DisplayName = "Game Instance Bootstrap Definition"))
	TSoftObjectPtr<UPdGameInstanceDefinition> BootstrapDefinition;

	/** 게임플레이에 들어가기 전에 반드시 읽어야 하는 정확한 주 애셋 목록. */
	UPROPERTY(Config, EditAnywhere, Category = "Content|Game Entry",
		meta = (TitleProperty = "PrimaryAssetName"))
	TArray<FPrimaryAssetId> GameEntryRequiredPrimaryAssets;

	/** 이 주 애셋 유형으로 등록된 애셋은 게임플레이 전에 모두 읽는다. */
	UPROPERTY(Config, EditAnywhere, Category = "Content|Game Entry")
	TArray<FPrimaryAssetType> GameEntryRequiredPrimaryAssetTypes;

	/** 캐릭터가 머리 위 체력바로 만드는 위젯 컴포넌트 클래스. 캐릭터 BP는 이 하위 객체의 값을 덮어쓴다. */
	UPROPERTY(Config, EditAnywhere, Category = "Character|Presentation")
	TSoftClassPtr<UWidgetComponent> CharacterHealthBarComponentClass;
};
