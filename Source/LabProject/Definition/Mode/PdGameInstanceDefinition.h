#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#include "PdGameInstanceDefinition.generated.h"

class UCharacterActionDefinition;
class UCharacterBaseDefinition;
class UControllerInputDefinition;
class UEnemyBaseDefinition;
class UAchievementDefinition;
class UGameSettingDefinition;
class UGuideDefinition;
class ULobbyModeDefinition;
class UDefaultProvisionDefinition;
class UMatchRuleDefinition;
class UPlayerControllerDefinition;
class UPlayerPawnDefinition;
class URecordDefinition;
class URewardDefinition;
class UStatUpgradeDefinition;
class UStatusEffectDefinition;

UENUM(BlueprintType)
enum class EStartupWindowMode : uint8
{
	Windowed,
	WindowedFullscreen,
	Fullscreen
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGameInstanceLifecycleSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle|Window")
	EStartupWindowMode StartupWindowMode = EStartupWindowMode::WindowedFullscreen;
};

/** Default project content selected once by the GameInstance bootstrap asset. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FProjectDefinitionReferences
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UGameSettingDefinition> GameSetting;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Online",
		meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UAchievementDefinition> Achievement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|UI",
		meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UGuideDefinition> Guide;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|UI",
		meta = (AssetBundles = "Client"))
	TSoftObjectPtr<URecordDefinition> Record;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Ability System",
		meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<UStatusEffectDefinition>> StatusEffects;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Match",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UMatchRuleDefinition> MatchRule;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Lobby",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<ULobbyModeDefinition> LobbyMode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server", FormerlySerializedAs = "LobbyPreview"))
	TSoftObjectPtr<UDefaultProvisionDefinition> DefaultProvision;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Character",
		meta = (AssetBundles = "Client,Server", FormerlySerializedAs = "CharacterHumanoid"))
	TSoftObjectPtr<UCharacterBaseDefinition> Character;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Character",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UEnemyBaseDefinition> EnemyBase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UPlayerPawnDefinition> PlayerPawn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UPlayerControllerDefinition> PlayerController;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UControllerInputDefinition> ControllerInput;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UCharacterActionDefinition> CharacterAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Player",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UStatUpgradeDefinition> StatUpgrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions|Reward",
		meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<URewardDefinition> Reward;
};

/**
 * Data-driven fragments consumed by GameInstance-lifetime subsystems.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPdGameInstanceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static FSoftObjectPath GetDefaultDefinitionPath();
	static const FProjectDefinitionReferences& GetConfiguredDefinitionReferences();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Memo",
		meta = (DisplayName = "Memo", MultiLine = "true"))
	TArray<FText> Memo;

	const FGameInstanceLifecycleSettings& GetLifecycleSettings() const { return Lifecycle; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Lifecycle",
		meta = (AllowPrivateAccess = "true"))
	FGameInstanceLifecycleSettings Lifecycle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Game Instance|Definitions",
		meta = (AllowPrivateAccess = "true"))
	FProjectDefinitionReferences Definitions;
};
