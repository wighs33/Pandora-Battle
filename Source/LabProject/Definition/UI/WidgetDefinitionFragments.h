#pragma once

#include "Definition/UI/WidgetClassDefinition.h"

#include "WidgetDefinitionFragments.generated.h"

/** Per-widget construction, presentation and behavior settings. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Style Definition"))
class LABPROJECT_API UWidgetStyleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FKillBoxWidgetSettings KillBoxWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FInfoWidgetSettings InfoWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FSelectPandoraWidgetSettings SelectPandoraWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FPandoraTreeWidgetSettings PandoraTreeWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FPandoraWidgetSettings PandoraWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FRightPandoraWidgetSettings RightPandoraWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FRightNotificationsWidgetSettings RightNotificationsWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FTitleAuxiliaryWidgetSettings TitleAuxiliaryWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FRecordWidgetSettings RecordWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FStatusEffectsBarWidgetSettings StatusEffectsBarWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FAbilitySlotWidgetSettings AbilitySlotWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FQuickSlotWidgetSettings QuickSlotWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FRoomListWidgetSettings RoomListWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FLobbyWidgetSettings LobbyWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FInventoryWidgetSettings InventoryWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Style", meta = (AssetBundles = "Client"))
	FSkinWidgetSettings SkinWidgetSettings;
};

/** Input actions and their visual key/icon configuration. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Input Icons Definition"))
class LABPROJECT_API UWidgetInputIconsDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings SkillTipEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings PandoraDescriptionEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FActionSlotWidgetSettings ActionSlotWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UInputAction> TogglePandoraTreeInputAction;
};

/** Total-map images, marker presentation and world-to-map projection settings. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Map UI Definition"))
class LABPROJECT_API UWidgetMapUIDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Map", meta = (AssetBundles = "Client"))
	FMapWidgetSettings MapWidgetSettings;
};
