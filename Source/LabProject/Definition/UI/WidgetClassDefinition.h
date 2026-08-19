#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Common/GameSessionConstants.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Layout/Margin.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Definition/UI/WidgetContentBundle.h"
#include "UObject/SoftObjectPath.h"

#include "WidgetClassDefinition.generated.h"

class UInfoWidget;
class UInfoUiPresenter;
class AActor;
class UActionSlotEntryWidget;
class UCharacterActionDefinition;
class UConnectingPopupWidget;
class UCreateRoomPopupWidget;
class UGameResultWidget;
class UGameConfigWidget;
class UGuideWidget;
class UInputAction;
class UKillBoxWidget;
class ULobbyUserWidget;
class UMapWidget;
class UMaterialInterface;
class UMenuPopupWidget;
class UNotificationEntryWidget;
class UPandoraDescriptionWidget;
class UQuickSlotEntryWidget;
class USelectPandoraWidget;
class UPandoraTreeWidget;
class URecordEntryWidget;
class URecordWidget;
class URightNotificationsWidget;
class URoomItemWidget;
class UShopWidget;
class UStatusEffectWidget;
class UTexture2D;
class UTrainingRoomMenuPopupWidget;
class UWidgetInputIconsDefinition;
class UWidgetMapUIDefinition;
class UWidgetStyleDefinition;

USTRUCT(BlueprintType)
struct LABPROJECT_API FInputKeyIconSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Input Key")
	bool bHideInputKeyIcon = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Input Key", meta = (ClampMin = "1.0"))
	FVector2D IconSize = FVector2D(32.0f, 32.0f);
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FAbilitySlotWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillSlot|Disabled", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DisabledSlotOpacity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillSlot|Cooldown")
	bool bShowCooldownTimeRemaining = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillTipWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillEffectIcon|Images", meta = (AssetBundles = "Client", DisplayName = "Burn Effect Image"))
	TSoftObjectPtr<UTexture2D> BurnImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillEffectIcon|Images", meta = (AssetBundles = "Client", DisplayName = "Frostbite Effect Image"))
	TSoftObjectPtr<UTexture2D> FrostbiteImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillEffectIcon|Images", meta = (AssetBundles = "Client", DisplayName = "Electric Shock Effect Image"))
	TSoftObjectPtr<UTexture2D> ElectricShockImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SkillEffectIcon|Images", meta = (AssetBundles = "Client", DisplayName = "Shield Effect Image"))
	TSoftObjectPtr<UTexture2D> ShieldImage;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FQuickSlotWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|QuickSlot", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UQuickSlotEntryWidget> EntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|QuickSlot", meta = (ClampMin = "1"))
	int32 SlotCount = 8;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FActionSlotWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UActionSlotEntryWidget> EntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot")
	FMargin SlotPadding = FMargin(5.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Cooldown")
	bool bShowCooldownTimeRemaining = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Input")
	float ReadyInputKeyOpacity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Input")
	float CooldownInputKeyOpacity = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Input")
	FInputKeyIconSettings InputKeyIconSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Actions")
	TSoftObjectPtr<UCharacterActionDefinition> CharacterActionDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Input Actions")
	TSoftObjectPtr<UInputAction> PandoraWeaponSwapInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ActionSlot|Input Actions")
	TSoftObjectPtr<UInputAction> GrappleHookInputAction;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerHudWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD")
	TSubclassOf<UUserWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FKillBoxWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|KillBox")
	TSoftClassPtr<UKillBoxWidget> EntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|KillBox", meta = (ClampMin = "0.01"))
	float RefreshInterval = 0.2f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FInfoWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget")
	TSubclassOf<UInfoWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget")
	TSubclassOf<UInfoUiPresenter> PresenterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Animation", meta = (ClampMin = "0.0"))
	float HideAnimationDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Animation")
	FVector2D MapSlideStartOffset = FVector2D(0.0f, -96.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Animation", meta = (ClampMin = "0.01"))
	float MapSlideDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Character Preview")
	bool bUseCharacterPreviewCamera = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Character Preview")
	bool bReturnCameraOnHide = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Detail")
	FVector2D DetailPopupOffset = FVector2D(18.0f, 0.0f);

};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSelectPandoraWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget")
	TSubclassOf<USelectPandoraWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (ClampMin = "0.0"))
	double DeadZoneRadius = 0.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|SelectPandoraWidget", meta = (ClampMin = "0.001"))
	double SegmentAngle = 90.0;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FAimCrosshairWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget")
	TSubclassOf<UUserWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPandoraTreeWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget", meta = (Categories = "UI.Widget"))
	FGameplayTag WidgetTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget")
	TSubclassOf<UPandoraTreeWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget")
	FText PandoraPointsFormat = NSLOCTEXT("PandoraTreeWidget", "PandoraPointsFormat", "Soul Dust: {0}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Input")
	bool bCloseOnToggleKey = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget")
	bool bSetInputModeOnShowHide = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Animation", meta = (ClampMin = "0.0"))
	float HideAnimationDelay = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Character Preview")
	bool bUseCharacterPreviewCamera = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Character Preview", meta = (ClampMin = "0.0"))
	float PreviewCameraShowBlendTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Character Preview", meta = (ClampMin = "0.0"))
	float PreviewCameraHideBlendTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraTreeWidget|Character Preview")
	bool bReturnCameraOnHide = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPandoraWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraWidget|Display")
	bool bShowMaxText = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraWidget|Button Hold", meta = (ClampMin = "0.0"))
	double ButtonHoldDuration = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PandoraWidget|Button Hold", meta = (ClampMin = "0.001"))
	float ButtonHoldUpdateInterval = 0.033333f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRightPandoraWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightPandora|Filter", meta = (Categories = "Pandora"))
	FGameplayTag OffensiveTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightPandora|Filter", meta = (Categories = "Pandora"))
	FGameplayTag DefensiveTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightPandora|Filter", meta = (Categories = "Pandora"))
	FGameplayTag SupportTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightPandora|Filter", meta = (Categories = "Pandora"))
	FGameplayTag SpecialTypeTag;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRightNotificationsWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget")
	TSubclassOf<URightNotificationsWidget> WidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget")
	TSubclassOf<UNotificationEntryWidget> NotificationEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget", meta = (ClampMin = "1"))
	int32 MaxVisibleNotifications = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget", meta = (ClampMin = "0.0"))
	float NotificationLifetime = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RightNotificationsWidget", meta = (ClampMin = "0.0"))
	float NotificationDequeueInterval = 0.5f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMenuPopupWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|MenuPopup")
	TSubclassOf<UMenuPopupWidget> MenuPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|MenuPopup")
	TSubclassOf<UTrainingRoomMenuPopupWidget> TrainingRoomMenuPopupWidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FConnectingPopupWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ConnectingPopup")
	TSubclassOf<UConnectingPopupWidget> WidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FTitleAuxiliaryWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title")
	TSubclassOf<UGuideWidget> GuideWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title")
	TSubclassOf<URecordWidget> RecordWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title|QuickMatch", meta = (ClampMin = "1"))
	int32 QuickMatchMaxSearchResults = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title|QuickMatch", meta = (ClampMin = "1"))
	int32 QuickMatchMaxPublicConnections = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title|QuickMatch")
	FString QuickMatchRoomName = TEXT("Quick Match");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title|QuickMatch")
	bool bQuickMatchLAN = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Title|QuickMatch")
	bool bQuickMatchUseLobbies = true;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FInfoAuxiliaryWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Auxiliary")
	TSubclassOf<UMapWidget> TotalMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Auxiliary")
	TSubclassOf<AActor> CharacterPreviewClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Auxiliary")
	TSubclassOf<UPandoraDescriptionWidget> PandoraDescriptionWidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMapWidgetProjectionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Size")
	FVector2D AreaMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Size", meta = (FormerlySerializedAs = "SurfaceMapTotalSizeBoxSize"))
	FVector2D WindmillMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Size", meta = (FormerlySerializedAs = "UndergroundMapTotalSizeBoxSize"))
	FVector2D DomeMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Size")
	FVector2D TempleMapTotalSizeBoxSize = FVector2D(900.0f, 900.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Plane", meta = (FormerlySerializedAs = "SurfacePlaneTransform"))
	FTransform WindmillPlaneTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Plane")
	FTransform TemplePlaneTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Plane", meta = (FormerlySerializedAs = "UndergroundPlaneTransform"))
	FTransform DomePlaneTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float PlaneLocalSize = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker")
	bool bRotateMarksToPawnForward = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker")
	float MarkerRotationOffsetDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMapWidgetProjectionOverride
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map")
	TSoftClassPtr<UMapWidget> MapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map")
	FMapWidgetProjectionSettings ProjectionSettings;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMapWidgetTeamMarkImage
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker")
	ETeamColor TeamColor = ETeamColor::Red;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> TeamMarkImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (ClampMin = "0.0"))
	FVector2D TeamMarkImageSize = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FMapWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Texture")
	TSoftObjectPtr<UTexture2D> AreaMapTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Texture", meta = (FormerlySerializedAs = "SurfaceMapTexture"))
	TSoftObjectPtr<UTexture2D> WindmillMapTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Texture", meta = (FormerlySerializedAs = "UndergroundMapTexture"))
	TSoftObjectPtr<UTexture2D> DomeMapTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Texture")
	TSoftObjectPtr<UTexture2D> TempleMapTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> CharacterMarkImage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (ClampMin = "0.0"))
	FVector2D CharacterMarkImageSize = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (TitleProperty = "TeamColor"))
	TArray<FMapWidgetTeamMarkImage> TeamMarkImages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker")
	bool bShowRemotePlayerMarks = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float MarkerUpdateInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Marker", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float RemotePlayerListRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Projection")
	bool bUseDefaultProjectionSettings = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Projection", meta = (EditCondition = "bUseDefaultProjectionSettings"))
	FMapWidgetProjectionSettings DefaultProjectionSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Map|Projection", meta = (TitleProperty = "MapWidgetClass"))
	TArray<FMapWidgetProjectionOverride> ProjectionOverrides;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRecordWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Record")
	TSubclassOf<URecordEntryWidget> RecordEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Record", meta = (ClampMin = "1"))
	int32 MaxVisibleRecordEntries = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Record", meta = (ClampMin = "1.0"))
	float MaxTierProgressWinCount = 160.0f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FStatusEffectsBarWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|StatusEffect")
	TSubclassOf<UStatusEffectWidget> StatusEffectWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|StatusEffect|Entry", meta = (ClampMin = "0.001"))
	float MeterUpdateInterval = 0.033333f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|StatusEffect|Entry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialIconOpacity = 0.65f;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FRoomListWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList")
	TSubclassOf<URoomItemWidget> RoomItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList")
	TSubclassOf<UCreateRoomPopupWidget> CreateRoomPopupWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|UI", meta = (ClampMin = "1"))
	int32 MaxRoomSlots = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Session", meta = (ClampMin = "1"))
	int32 MaxSearchResults = 50;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Session")
	bool bSearchLAN = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Session")
	bool bUseLobbies = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Create Room")
	FString DefaultRoomName = TEXT("New Room");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Create Room", meta = (ClampMin = "1"))
	int32 MaxPublicConnections = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|RoomList|Create Room")
	bool bCreateLAN = false;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby")
	TSubclassOf<ULobbyUserWidget> LobbyUserWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby")
	TSubclassOf<UGameConfigWidget> GameConfigWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby|UI", meta = (ClampMin = "1"))
	int32 MaxLobbySlots = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby|Countdown")
	FText GameStartCountdownFormatText = NSLOCTEXT("Lobby", "GameStartCountdownFormatText", "{Seconds}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby|Countdown")
	FText GameStartCountdownFinishedText = NSLOCTEXT("Lobby", "GameStartCountdownFinishedText", "Start");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby|Countdown", meta = (ClampMin = "0.01"))
	float GameStartCountdownTickInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Lobby|Team", meta = (MultiLine = "true"))
	FText TeamBalanceWarningText;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FGameResultWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|GameResult")
	TSubclassOf<UGameResultWidget> GameResultWidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FCharacterWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Character")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Character")
	TSubclassOf<UUserWidget> EnemyAvatarWidgetClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FInventoryWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory", meta = (ClampMin = "0"))
	int32 GameInventoryItemCountLimit = 40;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory", meta = (ClampMin = "0"))
	int32 TrainingRoomInventoryItemCountLimit = 120;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Style", meta = (DisplayName = "Upgradeable Item Background Color", HideAlphaChannel))
	FLinearColor UpgradeableItemBackgroundColor = FLinearColor(1.0f, 0.55f, 0.55f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Style", meta = (DisplayName = "Assigned Item Background Color", HideAlphaChannel))
	FLinearColor AssignedItemBackgroundColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Filter", meta = (Categories = "Item"))
	FGameplayTag WeaponTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Filter", meta = (Categories = "Item"))
	FGameplayTag EquipmentTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Filter", meta = (Categories = "Item"))
	FGameplayTag ValuableTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Inventory|Filter", meta = (Categories = "Item"))
	FGameplayTag ConsumableTypeTag;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FSkinWidgetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Slots", meta = (ClampMin = "0"))
	int32 SkinSlotCount = 40;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Filter", meta = (Categories = "Skin"))
	FGameplayTag PandoraTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Filter", meta = (Categories = "Skin"))
	FGameplayTag CosmeticsTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Filter", meta = (Categories = "Skin"))
	FGameplayTag GestureTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Filter", meta = (Categories = "Skin"))
	FGameplayTag RidingTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Filter", meta = (Categories = "Skin"))
	FGameplayTag PetTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Paint", meta = (DisplayName = "Brush Material"))
	TSoftObjectPtr<UMaterialInterface> PaintBrushMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Paint", meta = (DisplayName = "Canvas Display Material"))
	TSoftObjectPtr<UMaterialInterface> PaintCanvasDisplayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Face Decal", meta = (DisplayName = "Face Decal Material"))
	TSoftObjectPtr<UMaterialInterface> PaintCanvasFaceDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Face Decal", meta = (DisplayName = "Face Decal Socket Name"))
	FName PaintCanvasFaceDecalSocketName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Face Decal", meta = (DisplayName = "Face Decal Transform Offset"))
	FTransform PaintCanvasFaceDecalTransformOffset = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::OneVector);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Face Decal", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Face Decal Size"))
	FVector PaintCanvasFaceDecalSize = FVector(8.0, 20.0, 20.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Skin|Face Decal", meta = (DisplayName = "Face Decal Texture Parameter Name"))
	FName PaintCanvasFaceDecalTextureParameterName = TEXT("RenderTarget");
};

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Widget Class Definition"))
class LABPROJECT_API UWidgetClassDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const UWidgetClassDefinition* ResolveWidgetClassDefinition(const UObject* WorldContextObject);

	/** Collects the effective soft assets for one screen-lifetime bundle. */
	void GetRuntimePreloadAssetPaths(
		EWidgetContentBundle Bundle,
		TArray<FSoftObjectPath>& OutAssetPaths) const;

	UFUNCTION(BlueprintPure, Category = "!UI|Widget")
	TSubclassOf<UUserWidget> FindWidgetClassByTag(FGameplayTag WidgetTag) const;

	TSubclassOf<UUserWidget> GetPlayerHudWidgetClass() const;
	TSubclassOf<UInfoWidget> GetInfoWidgetClass() const;
	TSubclassOf<USelectPandoraWidget> GetSelectPandoraWidgetClass() const;
	TSubclassOf<UUserWidget> GetAimCrosshairWidgetClass() const;
	TSubclassOf<UPandoraTreeWidget> GetPandoraTreeWidgetClass() const;
	TSubclassOf<URightNotificationsWidget> GetRightNotificationsWidgetClass() const;
	TSubclassOf<UMenuPopupWidget> GetMenuPopupWidgetClass() const;
	TSubclassOf<UTrainingRoomMenuPopupWidget> GetTrainingRoomMenuPopupWidgetClass() const;
	TSubclassOf<UConnectingPopupWidget> GetConnectingPopupWidgetClass() const;
	TSubclassOf<UShopWidget> GetShopWidgetClass() const;
	TSubclassOf<UGuideWidget> GetGuideWidgetClass() const;
	TSubclassOf<URecordWidget> GetRecordWidgetClass() const;
	TSubclassOf<UMapWidget> GetTotalMapWidgetClass() const;
	TSubclassOf<AActor> GetCharacterPreviewClass() const;
	TSubclassOf<UPandoraDescriptionWidget> GetPandoraDescriptionWidgetClass() const;
	TSubclassOf<URecordEntryWidget> GetRecordEntryWidgetClass() const;
	TSubclassOf<UStatusEffectWidget> GetStatusEffectWidgetClass() const;
	TSubclassOf<URoomItemWidget> GetRoomItemWidgetClass() const;
	TSubclassOf<UCreateRoomPopupWidget> GetCreateRoomPopupWidgetClass() const;
	TSubclassOf<ULobbyUserWidget> GetLobbyUserWidgetClass() const;
	TSubclassOf<UGameConfigWidget> GetGameConfigWidgetClass() const;
	TSubclassOf<UGameResultWidget> GetGameResultWidgetClass() const;
	TSubclassOf<UUserWidget> GetHealthBarWidgetClass() const;
	TSubclassOf<UUserWidget> GetEnemyAvatarWidgetClass() const;
	TSubclassOf<UInfoUiPresenter> GetInfoPresenterClass() const;
	TSubclassOf<UNotificationEntryWidget> GetNotificationEntryWidgetClass() const;
	TSoftClassPtr<UKillBoxWidget> GetKillBoxEntryWidgetClass() const;
	TSoftClassPtr<UQuickSlotEntryWidget> GetQuickSlotEntryWidgetClass() const;
	TSoftClassPtr<UActionSlotEntryWidget> GetActionSlotEntryWidgetClass() const;
	TSoftObjectPtr<UInputAction> GetTogglePandoraTreeInputAction() const;
	int32 GetInventoryItemCountLimit(bool bTrainingRoom) const;
	const FPlayerHudWidgetSettings& GetPlayerHudWidgetSettings() const { return PlayerHudWidgetSettings; }
	const FKillBoxWidgetSettings& GetKillBoxWidgetSettings() const;
	const FInfoWidgetSettings& GetInfoWidgetSettings() const;
	const FSelectPandoraWidgetSettings& GetSelectPandoraWidgetSettings() const;
	const FAimCrosshairWidgetSettings& GetAimCrosshairWidgetSettings() const { return AimCrosshairWidgetSettings; }
	const FPandoraTreeWidgetSettings& GetPandoraTreeWidgetSettings() const;
	const FPandoraWidgetSettings& GetPandoraWidgetSettings() const;
	const FRightPandoraWidgetSettings& GetRightPandoraWidgetSettings() const;
	const FRightNotificationsWidgetSettings& GetRightNotificationsWidgetSettings() const;
	const FMenuPopupWidgetSettings& GetMenuPopupWidgetSettings() const { return MenuPopupWidgetSettings; }
	const FConnectingPopupWidgetSettings& GetConnectingPopupWidgetSettings() const { return ConnectingPopupWidgetSettings; }
	const FTitleAuxiliaryWidgetSettings& GetTitleAuxiliaryWidgetSettings() const;
	const FInfoAuxiliaryWidgetSettings& GetInfoAuxiliaryWidgetSettings() const { return InfoAuxiliaryWidgetSettings; }
	const FMapWidgetSettings& GetMapWidgetSettings() const;
	const FRecordWidgetSettings& GetRecordWidgetSettings() const;
	const FStatusEffectsBarWidgetSettings& GetStatusEffectsBarWidgetSettings() const;
	const FRoomListWidgetSettings& GetRoomListWidgetSettings() const;
	const FLobbyWidgetSettings& GetLobbyWidgetSettings() const;
	const FGameResultWidgetSettings& GetGameResultWidgetSettings() const { return GameResultWidgetSettings; }
	const FCharacterWidgetSettings& GetCharacterWidgetSettings() const { return CharacterWidgetSettings; }
	const FInventoryWidgetSettings& GetInventoryWidgetSettings() const;
	const FSkinWidgetSettings& GetSkinWidgetSettings() const;
	const FAbilitySlotWidgetSettings& GetAbilitySlotWidgetSettings() const;
	const FSkillTipWidgetSettings& GetSkillTipEffectIconSettings() const;
	const FSkillTipWidgetSettings& GetPandoraDescriptionEffectIconSettings() const;
	const FQuickSlotWidgetSettings& GetQuickSlotWidgetSettings() const;
	const FActionSlotWidgetSettings& GetActionSlotWidgetSettings() const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Fragments", meta = (AllowPrivateAccess = "true", IncludeAssetBundles))
	TObjectPtr<UWidgetStyleDefinition> Style;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Fragments", meta = (AllowPrivateAccess = "true", IncludeAssetBundles))
	TObjectPtr<UWidgetInputIconsDefinition> InputIcons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Fragments", meta = (AllowPrivateAccess = "true", IncludeAssetBundles))
	TObjectPtr<UWidgetMapUIDefinition> MapUI;

	// Settings not owned by one of the domain fragments remain on this asset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|PlayerHUD", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FPlayerHudWidgetSettings PlayerHudWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|AimCrosshairWidget", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FAimCrosshairWidgetSettings AimCrosshairWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|MenuPopup", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FMenuPopupWidgetSettings MenuPopupWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|ConnectingPopup", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FConnectingPopupWidgetSettings ConnectingPopupWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|InfoWidget|Auxiliary", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FInfoAuxiliaryWidgetSettings InfoAuxiliaryWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|GameResult", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FGameResultWidgetSettings GameResultWidgetSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!UI|Widget|Character", meta = (AllowPrivateAccess = "true", AssetBundles = "Client"))
	FCharacterWidgetSettings CharacterWidgetSettings;
};
