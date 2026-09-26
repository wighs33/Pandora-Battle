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

/** Per-mode quantities for one asset in the shared provision catalog. */
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

/** Per-mode scalar values shared by lobby, training, and gameplay. */
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

/** Per-mode switch used by shared default-provision policies. */
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

/** Per-mode Pandora levels. -1 does not grant; 0 grants LV0. */
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

/** Item quantity and optional quick-slot assignment shared by every mode. */
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

/** Pandora asset selected once, with its starting level configured per mode. */
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

/** Gesture-to-slot assignment granted identically in every mode. */
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
	const FDefaultProvisionModeFlags& GetGrantAllWeapons() const { return GrantAllWeapons; }
	const FDefaultProvisionModeFlags& GetGrantAllEquipment() const { return GrantAllEquipment; }
	const TArray<FDefaultProvisionPandoraGrant>& GetPandoraGrants() const { return PandoraGrants; }
	const TArray<FDefaultProvisionItemStackGrant>& GetItemGrants() const { return ItemGrants; }
	const TArray<FDefaultProvisionGestureSlotGrant>& GetGestureGrants() const { return GestureGrants; }

	void GetPandoraKeys(
		EDefaultProvisionMode Mode,
		TArray<FName>& OutPandoraKeys) const;
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
