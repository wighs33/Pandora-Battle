#include "PlayerComponent/PlayerNotificationComponent.h"

#include "Engine/AssetManager.h"
#include "Item/ItemDefinition.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraDefinition.h"
#include "Skin/SkinDefinition.h"
#include "UObject/PrimaryAssetId.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerNotificationComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPlayerNotificationComponent, Log, All);

namespace
{
	FText FormatRewardNotificationText(const FText& DisplayName)
	{
		return FText::Format(
			NSLOCTEXT("PlayerNotificationComponent", "RewardCollectedFormat", "{0} Collected"),
			DisplayName);
	}
}

UPlayerNotificationComponent::UPlayerNotificationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPlayerNotificationComponent::SendNotification(const FPdNotificationData& NotificationData) const
{
	APdPlayerState* PlayerState = GetPdPlayerState();
	APdPlayerController* PlayerController = PlayerState ? Cast<APdPlayerController>(PlayerState->GetOwner()) : nullptr;
	if (!PlayerController)
	{
		UE_LOG(LogPlayerNotificationComponent, Warning,
			TEXT("[Notification] skipped: player controller missing. component=%s owner=%s playerState=%s text=%s icon=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(PlayerState),
			*NotificationData.Text.ToString(),
			*GetNameSafe(NotificationData.IconResource));
		return;
	}

	UE_LOG(LogPlayerNotificationComponent, Log,
		TEXT("[Notification] client rpc. controller=%s text=%s icon=%s"),
		*GetNameSafe(PlayerController),
		*NotificationData.Text.ToString(),
		*GetNameSafe(NotificationData.IconResource));
	PlayerController->Client_ShowRightNotification(NotificationData);
}

void UPlayerNotificationComponent::SendRewardNotifications(
	const TArray<FPrimaryAssetId>& RewardItemDefinitions,
	const TArray<FPrimaryAssetId>& RewardSkinDefinitions,
	const TArray<FPrimaryAssetId>& RewardPandoraDefinitions) const
{
	for (const FPrimaryAssetId& RewardItemDefinition : RewardItemDefinitions)
	{
		SendRewardNotificationForAsset(
			RewardItemDefinition,
			NSLOCTEXT("PlayerNotificationComponent", "ItemRewardFallback", "Item Collected"));
	}

	for (const FPrimaryAssetId& RewardSkinDefinition : RewardSkinDefinitions)
	{
		SendRewardNotificationForAsset(
			RewardSkinDefinition,
			NSLOCTEXT("PlayerNotificationComponent", "SkinRewardFallback", "Skin Collected"));
	}

	for (const FPrimaryAssetId& RewardPandoraDefinition : RewardPandoraDefinitions)
	{
		SendRewardNotificationForAsset(
			RewardPandoraDefinition,
			NSLOCTEXT("PlayerNotificationComponent", "PandoraRewardFallback", "Pandora Collected"));
	}
}

void UPlayerNotificationComponent::SendRewardNotificationForAsset(const FPrimaryAssetId& AssetId, const FText& FallbackText) const
{
	if (!AssetId.IsValid())
	{
		UE_LOG(LogPlayerNotificationComponent, Warning,
			TEXT("[Notification] skipped: invalid reward asset id. component=%s owner=%s assetId=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*AssetId.ToString());
		return;
	}

	FPdNotificationData NotificationData;
	NotificationData.Text = FallbackText;

	UE_LOG(LogPlayerNotificationComponent, Log,
		TEXT("[Notification] resolving reward asset. component=%s assetId=%s"),
		*GetNameSafe(this),
		*AssetId.ToString());
	UObject* RewardObject = ResolvePrimaryAssetObject(AssetId);
	if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(RewardObject))
	{
		NotificationData.Text = FormatRewardNotificationText(ItemDefinition->DisplayName.IsEmpty() ? FText::FromName(ItemDefinition->GetFName()) : ItemDefinition->DisplayName);
		NotificationData.IconResource = ItemDefinition->IconTexture.LoadSynchronous();
	}
	else if (const USkinDefinition* SkinDefinition = Cast<USkinDefinition>(RewardObject))
	{
		NotificationData.Text = FormatRewardNotificationText(SkinDefinition->DisplayName.IsEmpty() ? FText::FromName(SkinDefinition->GetFName()) : SkinDefinition->DisplayName);
		NotificationData.IconResource = SkinDefinition->IconTexture;
	}
	else if (const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(RewardObject))
	{
		NotificationData.Text = FormatRewardNotificationText(PandoraDefinition->GetDisplayName().IsEmpty() ? FText::FromName(PandoraDefinition->GetFName()) : PandoraDefinition->GetDisplayName());
		NotificationData.IconResource = PandoraDefinition->GetIconResource();
	}
	else
	{
		UE_LOG(LogPlayerNotificationComponent, Warning,
			TEXT("[Notification] warning: reward asset resolved to unsupported object. assetId=%s object=%s class=%s"),
			*AssetId.ToString(),
			*GetNameSafe(RewardObject),
			*GetNameSafe(RewardObject ? RewardObject->GetClass() : nullptr));
	}

	SendNotification(NotificationData);
}

UObject* UPlayerNotificationComponent::ResolvePrimaryAssetObject(const FPrimaryAssetId& AssetId) const
{
	if (!AssetId.IsValid())
	{
		UE_LOG(LogPlayerNotificationComponent, Warning,
			TEXT("[Notification] resolve skipped: invalid asset id."));
		return nullptr;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	UObject* LoadedObject = AssetManager.GetPrimaryAssetObject(AssetId);
	if (LoadedObject)
	{
		UE_LOG(LogPlayerNotificationComponent, Log,
			TEXT("[Notification] resolve found loaded object. assetId=%s object=%s"),
			*AssetId.ToString(),
			*GetNameSafe(LoadedObject));
		return LoadedObject;
	}

	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
	if (!AssetPath.IsValid())
	{
		UE_LOG(LogPlayerNotificationComponent, Warning,
			TEXT("[Notification] resolve failed: no valid primary asset path. assetId=%s"),
			*AssetId.ToString());
		return nullptr;
	}

	UObject* LoadedFromPath = AssetPath.TryLoad();
	UE_LOG(LogPlayerNotificationComponent, Log,
		TEXT("[Notification] resolve TryLoad. assetId=%s path=%s object=%s"),
		*AssetId.ToString(),
		*AssetPath.ToString(),
		*GetNameSafe(LoadedFromPath));
	return LoadedFromPath;
}

APdPlayerState* UPlayerNotificationComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}
