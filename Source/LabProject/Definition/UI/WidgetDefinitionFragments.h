#pragma once

#include "Definition/UI/WidgetClassDefinition.h"

#include "WidgetDefinitionFragments.generated.h"

/** 위젯별 생성·표시·동작 설정을 정의한다. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Style Definition"))
class LABPROJECT_API UWidgetStyleDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
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

/** 입력 액션과 화면에 표시할 키·아이콘 설정을 정의한다. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Input Icons Definition"))
class LABPROJECT_API UWidgetInputIconsDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings SkillTipEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FSkillTipWidgetSettings PandoraDescriptionEffectIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	FActionSlotWidgetSettings ActionSlotWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UInputAction> TogglePandoraTreeInputAction;
};

/** 전체 맵 이미지·마커 표시·월드 좌표의 맵 투영 설정을 정의한다. */
UCLASS(BlueprintType, meta = (DisplayName = "Widget Map UI Definition"))
class LABPROJECT_API UWidgetMapUIDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Map", meta = (AssetBundles = "Client"))
	FMapWidgetSettings MapWidgetSettings;
};
