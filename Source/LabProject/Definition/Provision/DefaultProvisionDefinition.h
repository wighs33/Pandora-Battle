#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"
#include "DefaultProvisionDefinition.generated.h"

UENUM()
enum class EDefaultProvisionMode : uint8
{
	Lobby,
	TrainingRoom,
	Gameplay
};

/** 공용 지급 목록의 애셋 하나를 모드별로 몇 개 줄지 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionModeCounts
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Count",
		meta = (ClampMin = "0"))
	int32 Lobby = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Count",
		meta = (ClampMin = "0"))
	int32 TrainingRoom = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Count",
		meta = (ClampMin = "0"))
	int32 Gameplay = 0;

	// Public API ------------------------------------------------------------------------------------------------------
	int32 GetCount(EDefaultProvisionMode Mode) const;
};

/** 로비·훈련장·경기가 함께 쓰는 모드별 수치 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionModeValues
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Value",
		meta = (ClampMin = "0.0"))
	float Lobby = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Value",
		meta = (ClampMin = "0.0"))
	float TrainingRoom = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Value",
		meta = (ClampMin = "0.0"))
	float Gameplay = 0.0f;

	// Public API ------------------------------------------------------------------------------------------------------
	float GetValue(EDefaultProvisionMode Mode) const;
};

/** 공용 기본 지급 정책의 모드별 켜기/끄기 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionModeFlags
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Policy")
	bool Lobby = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Policy")
	bool TrainingRoom = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Policy")
	bool Gameplay = false;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsEnabled(EDefaultProvisionMode Mode) const;
};

/** 모드별 판도라 레벨. -1은 주지 않고 0은 LV0으로 연다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionModeLevels
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Level",
		meta = (ClampMin = "-1", ToolTip = "-1 does not grant this Pandora; 0 grants it at LV0."))
	int32 Lobby = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Level",
		meta = (ClampMin = "-1", ToolTip = "-1 does not grant this Pandora; 0 grants it at LV0."))
	int32 TrainingRoom = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Level",
		meta = (ClampMin = "-1", ToolTip = "-1 does not grant this Pandora; 0 grants it at LV0."))
	int32 Gameplay = INDEX_NONE;

	// Public API ------------------------------------------------------------------------------------------------------
	int32 GetLevel(EDefaultProvisionMode Mode) const;
};

/** 모든 모드가 함께 쓰는 아이템 수량과 퀵슬롯 지정(선택) */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionItemStackGrant
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FDefaultProvisionItemStackGrant() = default;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Item",
		meta = (AllowedTypes = "ItemDefinition"))
	FPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Item",
		meta = (ClampMin = "-1", ClampMax = "3", ToolTip = "-1 leaves the item unassigned; 0-3 map to quick-slot keys 1-4."))
	int32 QuickSlotIndex = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Item")
	FDefaultProvisionModeCounts Counts;
};

/** 한 번 고른 판도라 애셋과 모드별 시작 레벨 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionPandoraGrant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Pandora",
		meta = (AllowedTypes = "PandoraDefinition"))
	FPrimaryAssetId PandoraDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Pandora")
	FDefaultProvisionModeLevels Levels;
};

/** 모든 모드에서 똑같이 주는 제스처와 칸 지정 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FDefaultProvisionGestureSlotGrant
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FDefaultProvisionGestureSlotGrant() = default;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Gesture",
		meta = (AllowedTypes = "SkinDefinition"))
	FPrimaryAssetId SkinDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Gesture",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "0-3 map to gesture keys 5-8."))
	int32 GestureSlotIndex = 0;
};

/** 로비·훈련·게임플레이에서 사용하는 공통 기본 지급 항목과 모드 값을 정의한다. */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UDefaultProvisionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UDefaultProvisionDefinition();

	static FSoftObjectPath GetDefaultDefinitionPath();
	static const UDefaultProvisionDefinition* ResolveDefaultDefinition();

	const FDefaultProvisionModeValues& GetStatusPointValues() const { return StatusPointValues; }
	const FDefaultProvisionModeCounts& GetSoulDustValues() const { return SoulDustValues; }
	const FDefaultProvisionModeFlags& GetGrantAllItems() const { return GrantAllItems; }
	const FDefaultProvisionModeFlags& GetGrantAllWeapons() const { return GrantAllWeapons; }
	const FDefaultProvisionModeFlags& GetGrantAllEquipment() const { return GrantAllEquipment; }
	const TArray<FDefaultProvisionPandoraGrant>& GetPandoraGrants() const { return PandoraGrants; }
	const TArray<FDefaultProvisionItemStackGrant>& GetItemGrants() const { return ItemGrants; }
	const TArray<FDefaultProvisionGestureSlotGrant>& GetGestureGrants() const { return GestureGrants; }

	bool HasPandoraGrants(EDefaultProvisionMode Mode) const;
	bool IsPandoraKeyGranted(FName PandoraKeyName, EDefaultProvisionMode Mode) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Values|Status Points",
		meta = (AllowPrivateAccess = "true"))
	FDefaultProvisionModeValues StatusPointValues;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Values|Soul Dust",
		meta = (AllowPrivateAccess = "true"))
	FDefaultProvisionModeCounts SoulDustValues;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Inventory Policy|All Items",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "Grants one of every ItemDefinition. Explicit ItemGrants quantities and quick slots take precedence."))
	FDefaultProvisionModeFlags GrantAllItems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Inventory Policy|All Weapons",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "Grants one of every weapon definition enabled for each play space."))
	FDefaultProvisionModeFlags GrantAllWeapons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "!Default Provision|Inventory Policy|All Equipment",
		meta = (AllowPrivateAccess = "true",
			ToolTip = "Grants one of every equipment definition enabled for each play space."))
	FDefaultProvisionModeFlags GrantAllEquipment;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Assets|Pandora",
		meta = (AllowPrivateAccess = "true", TitleProperty = "PandoraDefinitionId"))
	TArray<FDefaultProvisionPandoraGrant> PandoraGrants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Assets|Item",
		meta = (AllowPrivateAccess = "true", TitleProperty = "ItemDefinitionId"))
	TArray<FDefaultProvisionItemStackGrant> ItemGrants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Default Provision|Assets|Gesture",
		meta = (AllowPrivateAccess = "true", TitleProperty = "SkinDefinitionId"))
	TArray<FDefaultProvisionGestureSlotGrant> GestureGrants;
};
