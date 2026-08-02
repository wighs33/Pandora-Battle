#include "Component/Player/PlayerNotificationComponent.h"

#include "Engine/AssetManager.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "UObject/PrimaryAssetId.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerNotificationComponent)

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

		return;
	}


	PlayerController->Client_ShowRightNotification(NotificationData);
}

void UPlayerNotificationComponent::SendRewardNotifications(
	const TArray<FPrimaryAssetId>& RewardItemDefinitions,
	const TArray<FPrimaryAssetId>& RewardSkinDefinitions,
	const TArray<FPrimaryAssetId>& RewardPandoraDefinitions)
{
	TArray<FPrimaryAssetId> AssetsToLoad;
	AssetsToLoad.Append(RewardItemDefinitions);
	AssetsToLoad.Append(RewardSkinDefinitions);
	AssetsToLoad.Append(RewardPandoraDefinitions);
	AssetsToLoad.RemoveAll([](const FPrimaryAssetId& AssetId)
	{
		return !AssetId.IsValid();
	});

	if (AssetsToLoad.IsEmpty())
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		SendLoadedRewardNotifications(
			RewardItemDefinitions,
			RewardSkinDefinitions,
			RewardPandoraDefinitions);
		return;
	}

	const uint64 RequestId = NextRewardNotificationRequestId++;
	ActiveRewardNotificationRequestIds.Add(RequestId);
	TSharedPtr<FStreamableHandle> LoadHandle =
		ContentSubsystem->PreloadPrimaryAssetsAsync(
			AssetsToLoad,
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this,
					RequestId,
					RewardItemDefinitions,
					RewardSkinDefinitions,
					RewardPandoraDefinitions]()
				{
					CompleteRewardNotificationLoad(
						RequestId,
						RewardItemDefinitions,
						RewardSkinDefinitions,
						RewardPandoraDefinitions);
				}));

	// Primary assets that are already resident may complete before the load call returns.
	if (ActiveRewardNotificationRequestIds.Contains(RequestId))
	{
		if (LoadHandle.IsValid())
		{
			PendingRewardNotificationLoadHandles.Add(RequestId, MoveTemp(LoadHandle));
		}
		else
		{
			CompleteRewardNotificationLoad(
				RequestId,
				RewardItemDefinitions,
				RewardSkinDefinitions,
				RewardPandoraDefinitions);
		}
	}
	else if (LoadHandle.IsValid())
	{
		LoadHandle->ReleaseHandle();
	}
}

void UPlayerNotificationComponent::CompleteRewardNotificationLoad(
	const uint64 RequestId,
	TArray<FPrimaryAssetId> RewardItemDefinitions,
	TArray<FPrimaryAssetId> RewardSkinDefinitions,
	TArray<FPrimaryAssetId> RewardPandoraDefinitions)
{
	if (!ActiveRewardNotificationRequestIds.Remove(RequestId))
	{
		return;
	}

	SendLoadedRewardNotifications(
		RewardItemDefinitions,
		RewardSkinDefinitions,
		RewardPandoraDefinitions);

	TSharedPtr<FStreamableHandle> CompletedHandle;
	if (PendingRewardNotificationLoadHandles.RemoveAndCopyValue(RequestId, CompletedHandle)
		&& CompletedHandle.IsValid())
	{
		CompletedHandle->ReleaseHandle();
	}
}

void UPlayerNotificationComponent::SendLoadedRewardNotifications(
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

void UPlayerNotificationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleasePendingRewardNotificationLoads();
	Super::EndPlay(EndPlayReason);
}

void UPlayerNotificationComponent::ReleasePendingRewardNotificationLoads()
{
	ActiveRewardNotificationRequestIds.Reset();
	for (TPair<uint64, TSharedPtr<FStreamableHandle>>& PendingLoad :
		PendingRewardNotificationLoadHandles)
	{
		if (PendingLoad.Value.IsValid())
		{
			PendingLoad.Value->CancelHandle();
			PendingLoad.Value->ReleaseHandle();
		}
	}
	PendingRewardNotificationLoadHandles.Reset();
}

void UPlayerNotificationComponent::SendExperienceRewardNotification(
	const float RewardAmount,
	UObject* IconResource) const
{
	SendNumericRewardNotification(
		NSLOCTEXT("PlayerNotificationComponent", "ExperienceRewardName", "Experience"),
		RewardAmount,
		IconResource);
}

void UPlayerNotificationComponent::SendSoulDustRewardNotification(
	const int32 RewardAmount,
	UObject* IconResource) const
{
	SendNumericRewardNotification(
		NSLOCTEXT("PlayerNotificationComponent", "SoulDustRewardName", "Soul Dust"),
		static_cast<float>(RewardAmount),
		IconResource);
}

void UPlayerNotificationComponent::SendNumericRewardNotification(
	const FText& RewardName,
	const float RewardAmount,
	UObject* IconResource) const
{
	if (!FMath::IsFinite(RewardAmount) || RewardAmount <= 0.0f)
	{
		return;
	}

	FPdNotificationData NotificationData;
	NotificationData.Text = FText::Format(
		NSLOCTEXT("PlayerNotificationComponent", "NumericRewardFormat", "{0} +{1}"),
		RewardName,
		FText::AsNumber(RewardAmount));
	NotificationData.IconResource = IconResource;
	SendNotification(NotificationData);
}

void UPlayerNotificationComponent::SendRewardNotificationForAsset(const FPrimaryAssetId& AssetId, const FText& FallbackText) const
{
	if (!AssetId.IsValid())
	{

		return;
	}

	FPdNotificationData NotificationData;
	NotificationData.Text = FallbackText;


	UObject* RewardObject = ResolvePrimaryAssetObject(AssetId);
	if (const UItemDefinition* ItemDefinition = Cast<UItemDefinition>(RewardObject))
	{
		NotificationData.Text = FormatRewardNotificationText(ItemDefinition->DisplayName.IsEmpty() ? FText::FromName(ItemDefinition->GetFName()) : ItemDefinition->DisplayName);
		NotificationData.IconResource = ItemDefinition->IconTexture.Get();
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

	SendNotification(NotificationData);
}

UObject* UPlayerNotificationComponent::ResolvePrimaryAssetObject(const FPrimaryAssetId& AssetId) const
{
	if (!AssetId.IsValid())
	{

		return nullptr;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	UObject* LoadedObject = AssetManager.GetPrimaryAssetObject(AssetId);
	if (LoadedObject)
	{

		return LoadedObject;
	}

	const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
	return AssetPath.IsValid() ? AssetPath.ResolveObject() : nullptr;
}

APdPlayerState* UPlayerNotificationComponent::GetPdPlayerState() const
{
	return Cast<APdPlayerState>(GetOwner());
}
