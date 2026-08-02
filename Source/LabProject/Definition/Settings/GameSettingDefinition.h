#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/AbilityAttributeConfig.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "GameSettingDefinition.generated.h"

class UTexture2D;
class USoundBase;
class UAchievementDefinition;
class UStatusEffectDefinition;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdGameplayEffectRemovalPolicy
{
	GENERATED_BODY()

	/** Effect asset/dynamic asset tags. Matching this query is sufficient for removal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability System|Effect Removal")
	FGameplayTagQuery EffectTagQuery;

	/** Tags granted to the effect owner. Matching this query is also sufficient for removal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability System|Effect Removal")
	FGameplayTagQuery OwningTagQuery;

	/** Remaining loose tags matching this query are cleared after active effects are removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability System|Effect Removal")
	FGameplayTagQuery LooseTagQuery;

	/** Persistent cue fallbacks to remove after matching effects are removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Ability System|Effect Removal",
		meta = (Categories = "GameplayCue"))
	FGameplayTagContainer GameplayCuesToRemove;
};

UENUM(BlueprintType)
enum class EPdBgmContext : uint8
{
	Startup,
	Lobby,
	RoomList,
	Shop,
	TrainingRoom,
	Gameplay,
	Guide
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UGameSettingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGameSettingDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	void GetRuntimePreloadAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Memo", meta = (DisplayName = "Memo", ToolTip = "긴 한글 입력 시 IME 조합이 깨지지 않도록 내용을 여러 항목으로 나누어 적습니다."))
	TArray<FString> Memo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Mouse Cursor")
	bool bUseCustomMouseCursor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Mouse Cursor", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> MouseCursorTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Mouse Cursor", meta = (ClampMin = "1.0"))
	FVector2D MouseCursorSize = FVector2D(32.0, 32.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Mouse Cursor", meta = (ClampMin = "0.0"))
	FVector2D MouseCursorHotSpot = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Mouse Cursor")
	TEnumAsByte<EMouseCursor::Type> MouseCursorType = EMouseCursor::Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Debug|Attack", meta = (DisplayName = "Draw Attack Debug Visualization"))
	bool bDrawAttackDebugVisualization = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Equipment",
		meta = (DisplayName = "Equip Weapons Without Animation",
			ToolTip = "Skips both unequip and equip montages so weapon selection completes immediately."))
	bool bEquipWeaponsWithoutAnimation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Camera|Clamp", meta = (ClampMin = "-89.9", ClampMax = "89.9", UIMin = "-89.9", UIMax = "89.9", DisplayName = "Player View Pitch Min"))
	float PlayerViewPitchMin = -60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Camera|Clamp", meta = (ClampMin = "-89.9", ClampMax = "89.9", UIMin = "-89.9", UIMax = "89.9", DisplayName = "Player View Pitch Max"))
	float PlayerViewPitchMax = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Achievement", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UAchievementDefinition> AchievementData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Status Effect UI", meta = (AssetBundles = "Client"))
	TArray<TSoftObjectPtr<UStatusEffectDefinition>> StatusEffectDataAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Effect Removal")
	FPdGameplayEffectRemovalPolicy RemoveOnDeathPolicy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Effect Removal")
	FPdGameplayEffectRemovalPolicy RemoveOnRespawnPolicy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Effect Removal")
	FPdGameplayEffectRemovalPolicy RemoveOnPandoraResetPolicy;

	/**
	 * Core tag-to-attribute mappings shared by players and enemies.
	 * GameFeature attribute configs can override these entries at runtime.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Attributes",
		meta = (TitleProperty = "StatTag"))
	FPdAttributeConfig CoreAttributeConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Low",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", ForceUnits = "%"))
	float LowStaminaThresholdPercent = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Low",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", ForceUnits = "%"))
	float LowStaminaMovementSpeedReductionPercent = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Low",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LowStaminaFillTintValue = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Low",
		meta = (ToolTip = "Name or component tag of the effect component activated at or below the low-stamina threshold."))
	FName LowStaminaEffectComponentName = TEXT("NS_NoStamina");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Critical",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", ForceUnits = "%"))
	float CriticalStaminaThresholdPercent = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Critical",
		meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0", ForceUnits = "%"))
	float CriticalStaminaMovementSpeedReductionPercent = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Critical",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CriticalStaminaFillTintValue = 0.01f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Stamina|Cost",
		meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Action Stamina Cost",
			ToolTip = "Stamina consumed by one player skill or primary weapon attack."))
	float ActionStaminaCost = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100"))
	int32 DefaultMasterVolumePercent = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> SoundEnabledButtonTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> SoundMutedButtonTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio")
	bool bPlayStartupBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> StartupBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (ClampMin = "0.0"))
	float StartupBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio", meta = (ClampMin = "0.01"))
	float StartupBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio")
	bool bPersistStartupBgmAcrossLevelTransition = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Lobby")
	bool bPlayLobbyBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Lobby", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> LobbyBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Lobby", meta = (ClampMin = "0.0"))
	float LobbyBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Lobby", meta = (ClampMin = "0.01"))
	float LobbyBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Lobby")
	bool bPersistLobbyBgmAcrossLevelTransition = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Room List")
	bool bPlayRoomListBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Room List", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> RoomListBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Room List", meta = (ClampMin = "0.0"))
	float RoomListBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Room List", meta = (ClampMin = "0.01"))
	float RoomListBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Room List")
	bool bPersistRoomListBgmAcrossLevelTransition = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Shop")
	bool bPlayShopBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Shop", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> ShopBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Shop", meta = (ClampMin = "0.0"))
	float ShopBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Shop", meta = (ClampMin = "0.01"))
	float ShopBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Shop")
	bool bPersistShopBgmAcrossLevelTransition = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Guide")
	bool bPlayGuideBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Guide", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> GuideBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Guide", meta = (ClampMin = "0.0"))
	float GuideBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Guide", meta = (ClampMin = "0.01"))
	float GuideBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Guide")
	bool bPersistGuideBgmAcrossLevelTransition = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Training Room")
	bool bPlayTrainingRoomBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Training Room", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> TrainingRoomBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Training Room", meta = (ClampMin = "0.0"))
	float TrainingRoomBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Training Room", meta = (ClampMin = "0.01"))
	float TrainingRoomBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Training Room")
	bool bPersistTrainingRoomBgmAcrossLevelTransition = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Gameplay")
	bool bPlayGameplayBgm = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Gameplay", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USoundBase> GameplayBgm;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Gameplay", meta = (ClampMin = "0.0"))
	float GameplayBgmVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Gameplay", meta = (ClampMin = "0.01"))
	float GameplayBgmPitch = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Audio|Gameplay")
	bool bPersistGameplayBgmAcrossLevelTransition = true;
};
