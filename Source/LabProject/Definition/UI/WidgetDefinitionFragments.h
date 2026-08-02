#pragma once

#include "Definition/UI/WidgetClassDefinition.h"

#include "WidgetDefinitionFragments.generated.h"

/**
 * Widget construction types live separately from presentation data so a UI
 * composition can swap classes without duplicating all style/input settings.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Widget UI Classes Definition"))
class LABPROJECT_API UWidgetUIClassesDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWidgetUIClassesDefinition();
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Player HUD")
	FGameplayTag PlayerHudWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Player HUD")
	TSubclassOf<UUserWidget> PlayerHudWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Player HUD", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UKillBoxWidget> KillBoxEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	FGameplayTag InfoWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	TSubclassOf<UInfoWidget> InfoWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	TSubclassOf<UInfoUiPresenter> InfoPresenterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	TSubclassOf<UMapWidget> TotalMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	TSubclassOf<AActor> CharacterPreviewClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Info")
	TSubclassOf<UPandoraDescriptionWidget> PandoraDescriptionWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Selection")
	FGameplayTag SelectPandoraWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Selection")
	TSubclassOf<USelectPandoraWidget> SelectPandoraWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Selection")
	FGameplayTag AimCrosshairWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Selection")
	TSubclassOf<UUserWidget> AimCrosshairWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Pandora")
	FGameplayTag PandoraTreeWidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Pandora")
	TSubclassOf<UPandoraTreeWidget> PandoraTreeWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Notifications")
	TSubclassOf<URightNotificationsWidget> RightNotificationsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Notifications")
	TSubclassOf<UNotificationEntryWidget> NotificationEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Popup")
	TSubclassOf<UMenuPopupWidget> MenuPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Popup")
	TSubclassOf<UTrainingRoomMenuPopupWidget> TrainingRoomMenuPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Popup")
	TSubclassOf<UConnectingPopupWidget> ConnectingPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Title")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Title")
	TSubclassOf<UGuideWidget> GuideWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Title")
	TSubclassOf<URecordWidget> RecordWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Record")
	TSubclassOf<URecordEntryWidget> RecordEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Status")
	TSubclassOf<UStatusEffectWidget> StatusEffectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Chat")
	TSubclassOf<UChatEntryWidget> ChatEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Room")
	TSubclassOf<URoomItemWidget> RoomItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Room")
	TSubclassOf<UCreateRoomPopupWidget> CreateRoomPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Lobby")
	TSubclassOf<ULobbyUserWidget> LobbyUserWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Lobby")
	TSubclassOf<UGameConfigWidget> GameConfigWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Game Result")
	TSubclassOf<UGameResultWidget> GameResultWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Character")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Character")
	TSubclassOf<UUserWidget> EnemyAvatarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Slots", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UQuickSlotEntryWidget> QuickSlotEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Classes|Slots", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UActionSlotEntryWidget> ActionSlotEntryWidgetClass;
};

/** Presentation/layout/behavior settings which do not own input icon maps or map projection data. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Style Definition"))
class LABPROJECT_API UWidgetStyleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWidgetStyleDefinition();
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
	FChatWidgetSettings ChatWidgetSettings;

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
	UWidgetInputIconsDefinition();
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FAbilitySlotWidgetSettings AbilitySlotWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings SkillTipEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings PandoraDescriptionEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FQuickSlotWidgetSettings QuickSlotWidgetSettings;

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
	UWidgetMapUIDefinition();
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Map", meta = (AssetBundles = "Client"))
	FMapWidgetSettings MapWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Map")
	TArray<FName> TrainingRoomMapNames;
};
