#include "Component/Player/PlayerNotificationComponent.h"

#include "Component/Player/ControllerPresentationComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Mode/PdPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerNotificationComponent)

namespace
{
	FText FormatRewardNotificationText(const FText& DisplayName)
	{
		return FText::Format(NSLOCTEXT("PlayerNotificationComponent", "RewardCollectedFormat", "{0} Collected"), DisplayName);
	}
}

// 알림은 사건이 발생했을 때만 처리하며, 네트워크 전송은 소유 Controller의 RPC에 맡긴다.
UPlayerNotificationComponent::UPlayerNotificationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 서버에서 지급한 보상의 종류와 ID를 전달한다. 같은 보상이 여러 번 지급되면 알림도 각각 유지한다.
void UPlayerNotificationComponent::SendRewardNotifications(const TArray<FPrimaryAssetId>& RewardItemDefinitions,
	const TArray<FPrimaryAssetId>& RewardSkinDefinitions, const TArray<FPrimaryAssetId>& RewardPandoraDefinitions) const
{
	APdPlayerController* Controller = GetController<APdPlayerController>();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	TArray<FPdRewardNotification> Rewards;
	const auto AppendRewards = [&Rewards](const TArray<FPrimaryAssetId>& AssetIds, EPdRewardNotificationType Type)
	{
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			if (AssetId.IsValid())
			{
				FPdRewardNotification& Reward = Rewards.AddDefaulted_GetRef();
				Reward.Type = Type;
				Reward.AssetId = AssetId;
			}
		}
	};
	AppendRewards(RewardItemDefinitions, EPdRewardNotificationType::Item);
	AppendRewards(RewardSkinDefinitions, EPdRewardNotificationType::Skin);
	AppendRewards(RewardPandoraDefinitions, EPdRewardNotificationType::Pandora);
	if (!Rewards.IsEmpty())
	{
		Controller->Client_ShowRewardNotifications(Rewards);
	}
}

// 처치 등으로 지급한 경험치 획득량을 알린다.
void UPlayerNotificationComponent::SendExperienceRewardNotification(const float RewardAmount, UObject* IconResource) const
{
	SendNumericRewardNotification(EPdRewardNotificationType::Experience, RewardAmount, IconResource);
}

// 지급한 소울 더스트 수량을 정수 정밀도를 유지한 채 알린다.
void UPlayerNotificationComponent::SendSoulDustRewardNotification(const int32 RewardAmount, UObject* IconResource) const
{
	SendNumericRewardNotification(EPdRewardNotificationType::SoulDust, RewardAmount, IconResource);
}

// 수치 보상은 종류·수량·아이콘 경로를 보내 클라이언트가 문구와 아이콘을 준비하게 한다.
void UPlayerNotificationComponent::SendNumericRewardNotification(
	const EPdRewardNotificationType Type, const double RewardAmount, UObject* IconResource) const
{
	APdPlayerController* Controller = GetController<APdPlayerController>();
	if (!Controller || !Controller->HasAuthority() || !FMath::IsFinite(RewardAmount) || RewardAmount <= 0.0)
	{
		return;
	}

	FPdRewardNotification Reward;
	Reward.Type = Type;
	Reward.Amount = RewardAmount;
	Reward.IconPath = FSoftObjectPath(IconResource);
	Controller->Client_ShowRewardNotifications({Reward});
}

// 소유 클라이언트에서 보상 정의와 Client 번들의 아이콘을 읽어 알림을 준비한다.
void UPlayerNotificationComponent::ShowRewardNotifications(const TArray<FPdRewardNotification>& Rewards)
{
	if (!IsLocalController() || Rewards.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const TArray<FName> ClientBundles = {FName(TEXT("Client"))};
	TArray<FSoftObjectPath> PathsToLoad;
	for (const FPdRewardNotification& Reward : Rewards)
	{
		if (Reward.AssetId.IsValid())
		{
			AssetManager.GetPrimaryAssetLoadList(PathsToLoad, Reward.AssetId, ClientBundles, false);
		}
		if (Reward.IconPath.IsValid())
		{
			PathsToLoad.AddUnique(Reward.IconPath);
		}
	}

	if (PathsToLoad.IsEmpty())
	{
		ShowLoadedRewardNotifications(Rewards);
		return;
	}

	const uint64 RequestId = NextRewardNotificationRequestId++;
	// 완료 콜백이 로딩 함수의 반환보다 먼저 실행되어도 요청을 찾을 수 있도록 빈 항목부터 등록한다.
	PendingRewardNotificationLoadHandles.Add(RequestId);
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.GetStreamableManager().RequestAsyncLoad(
		PathsToLoad, FStreamableDelegate::CreateWeakLambda(this, [this, RequestId, Rewards]()
		{
			CompleteRewardNotificationLoad(RequestId, Rewards);
		}));

	if (TSharedPtr<FStreamableHandle>* PendingHandle = PendingRewardNotificationLoadHandles.Find(RequestId))
	{
		if (LoadHandle.IsValid())
		{
			*PendingHandle = MoveTemp(LoadHandle);
		}
		else
		{
			CompleteRewardNotificationLoad(RequestId, Rewards);
		}
	}
	else if (LoadHandle.IsValid())
	{
		LoadHandle->ReleaseHandle();
	}
}

// 로딩이 끝난 요청의 알림을 한 번만 표시하고, 위젯에 아이콘을 넘긴 뒤 로딩 참조를 해제한다.
void UPlayerNotificationComponent::CompleteRewardNotificationLoad(
	const uint64 RequestId, const TArray<FPdRewardNotification>& Rewards)
{
	TSharedPtr<FStreamableHandle> CompletedHandle;
	if (!PendingRewardNotificationLoadHandles.RemoveAndCopyValue(RequestId, CompletedHandle))
	{
		return;
	}

	ShowLoadedRewardNotifications(Rewards);
	if (CompletedHandle.IsValid())
	{
		CompletedHandle->ReleaseHandle();
	}
}

// 보상 정보를 화면용 문구와 아이콘으로 바꿔 알림 UI에 전달한다. 에셋을 읽지 못해도 기본 문구는 표시한다.
void UPlayerNotificationComponent::ShowLoadedRewardNotifications(const TArray<FPdRewardNotification>& Rewards) const
{
	const APdPlayerController* Controller = GetController<APdPlayerController>();
	UControllerPresentationComponent* Presentation = Controller ? Controller->GetControllerPresentationComponent() : nullptr;
	if (!Presentation || !Controller->IsLocalController())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPdRewardNotification& Reward : Rewards)
	{
		FPdNotificationData NotificationData;
		switch (Reward.Type)
		{
		case EPdRewardNotificationType::Item:
			NotificationData.Text = NSLOCTEXT("PlayerNotificationComponent", "ItemRewardFallback", "Item Collected");
			if (const UItemDefinition* Definition = Cast<UItemDefinition>(AssetManager.GetPrimaryAssetObject(Reward.AssetId)))
			{
				const FText Name = Definition->DisplayName.IsEmpty() ? FText::FromName(Definition->GetFName()) : Definition->DisplayName;
				NotificationData.Text = FormatRewardNotificationText(Name);
				NotificationData.IconResource = Definition->IconTexture.Get();
			}
			break;

		case EPdRewardNotificationType::Skin:
			NotificationData.Text = NSLOCTEXT("PlayerNotificationComponent", "SkinRewardFallback", "Skin Collected");
			if (const USkinDefinition* Definition = Cast<USkinDefinition>(AssetManager.GetPrimaryAssetObject(Reward.AssetId)))
			{
				const FText Name = Definition->DisplayName.IsEmpty() ? FText::FromName(Definition->GetFName()) : Definition->DisplayName;
				NotificationData.Text = FormatRewardNotificationText(Name);
				NotificationData.IconResource = Definition->IconTexture.Get();
			}
			break;

		case EPdRewardNotificationType::Pandora:
			NotificationData.Text = NSLOCTEXT("PlayerNotificationComponent", "PandoraRewardFallback", "Pandora Collected");
			if (const UPandoraDefinition* Definition = Cast<UPandoraDefinition>(AssetManager.GetPrimaryAssetObject(Reward.AssetId)))
			{
				const FText Name = Definition->GetDisplayName().IsEmpty()
					? FText::FromName(Definition->GetFName()) : Definition->GetDisplayName();
				NotificationData.Text = FormatRewardNotificationText(Name);
				NotificationData.IconResource = Definition->GetIconResource();
			}
			break;

		case EPdRewardNotificationType::Experience:
		case EPdRewardNotificationType::SoulDust:
			NotificationData.Text = FText::Format(
				NSLOCTEXT("PlayerNotificationComponent", "NumericRewardFormat", "{0} +{1}"),
				Reward.Type == EPdRewardNotificationType::Experience
					? NSLOCTEXT("PlayerNotificationComponent", "ExperienceRewardName", "Experience")
					: NSLOCTEXT("PlayerNotificationComponent", "SoulDustRewardName", "Soul Dust"),
				FText::AsNumber(Reward.Amount));
			NotificationData.IconResource = Reward.IconPath.ResolveObject();
			break;

		default:
			continue;
		}
		Presentation->ShowRightNotification(NotificationData);
	}
}

// Controller 종료 후에는 대기 중인 알림을 취소하고 표시용 에셋의 로딩 참조를 정리한다.
void UPlayerNotificationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	auto PendingLoads = MoveTemp(PendingRewardNotificationLoadHandles);
	for (const TPair<uint64, TSharedPtr<FStreamableHandle>>& PendingLoad : PendingLoads)
	{
		if (PendingLoad.Value.IsValid())
		{
			PendingLoad.Value->CancelHandle();
		}
	}
	Super::EndPlay(EndPlayReason);
}
