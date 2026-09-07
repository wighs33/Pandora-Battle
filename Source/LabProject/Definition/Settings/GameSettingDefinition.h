#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "GameSettingDefinition.generated.h"

class UTexture2D;
class USoundBase;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EBgmContext : uint8
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
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	void GetRuntimePreloadAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Memo",
		meta = (DisplayName = "Memo", MultiLine = "true"))
	TArray<FText> Memo;

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
			ToolTip = "Skips the equip montage so weapon attachment completes immediately. Unequip montages still play when removing or switching weapons."))
	bool bEquipWeaponsWithoutAnimation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Equipment|Gameplay Effect",
		meta = (DisplayName = "Equipped Item Gameplay Effect",
			ToolTip = "Infinite GE used to grant the equipped item's gameplay tag."))
	TSubclassOf<UGameplayEffect> EquippedItemGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Equipment|Gameplay Effect",
		meta = (DisplayName = "Equipment Stat Gameplay Effect",
			ToolTip = "Instant GE used to apply and remove equipment stat values."))
	TSubclassOf<UGameplayEffect> EquipmentStatGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Camera|Clamp", meta = (ClampMin = "-89.9", ClampMax = "89.9", UIMin = "-89.9", UIMax = "89.9", DisplayName = "Player View Pitch Min"))
	float PlayerViewPitchMin = -60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Camera|Clamp", meta = (ClampMin = "-89.9", ClampMax = "89.9", UIMin = "-89.9", UIMax = "89.9", DisplayName = "Player View Pitch Max"))
	float PlayerViewPitchMax = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Effect Removal|Death",
		meta = (Categories = "GameplayCue", DisplayName = "Gameplay Cues To Remove"))
	FGameplayTagContainer DeathGameplayCuesToRemove;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Effect Removal|Respawn",
		meta = (Categories = "GameplayCue", DisplayName = "Gameplay Cues To Remove"))
	FGameplayTagContainer RespawnGameplayCuesToRemove;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Cost",
		meta = (DisplayName = "Ability Cost Gameplay Effect",
			ToolTip = "Instant GE with additive Mana/Data.ManaCost and Stamina/Data.StaminaCost SetByCaller modifiers."))
	TSubclassOf<UGameplayEffect> AbilityCostGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Cooldown",
		meta = (DisplayName = "Ability Cooldown Gameplay Effect",
			ToolTip = "Shared Has Duration GE whose duration magnitude reads the Data.Cooldown SetByCaller tag."))
	TSubclassOf<UGameplayEffect> AbilityCooldownGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Movement",
		meta = (DisplayName = "Movement Speed Gameplay Effect",
			ToolTip = "Infinite GE that applies Data.MovementSpeed to the MovementSpeed attribute."))
	TSubclassOf<UGameplayEffect> MovementSpeedGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Stamina",
		meta = (DisplayName = "Stamina Regen Gameplay Effect",
			AssetBundles = "Server",
			ToolTip = "Gameplay Effect applied after stamina is consumed to regenerate stamina."))
	TSubclassOf<UGameplayEffect> StaminaRegenGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Setting|Ability System|Recovery",
		meta = (DisplayName = "Recovery Heal Gameplay Effect",
			AssetBundles = "Server",
			ToolTip = "Infinite Gameplay Effect that restores health and mana using Data.Heal and Data.Mana."))
	TSubclassOf<UGameplayEffect> RecoveryHealGameplayEffectClass;

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
