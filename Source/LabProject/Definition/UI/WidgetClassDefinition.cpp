#include "Definition/UI/WidgetClassDefinition.h"

#include "Definition/UI/WidgetDefinitionFragments.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Lobby/UI/GameConfigWidget.h"
#include "Lobby/UI/GameResultWidget.h"
#include "Lobby/UI/LobbyUserWidget.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "Room/CreateRoomPopupWidget.h"
#include "Room/RoomItemWidget.h"
#include "UI/UiSubsystem.h"
#include "UI/Shop/ShopWidget.h"
#include "UI/Widget/ActionSlotEntryWidget.h"
#include "UI/Widget/GuideWidget.h"
#include "UI/Presenter/InfoUiPresenter.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/KillBoxWidget.h"
#include "UI/Widget/MapWidget.h"
#include "UI/Widget/MenuPopupWidget.h"
#include "UI/Widget/NotificationEntryWidget.h"
#include "UI/Widget/PandoraDescriptionWidget.h"
#include "UI/Widget/QuickSlotEntryWidget.h"
#include "UI/Widget/RecordEntryWidget.h"
#include "UI/Widget/RecordWidget.h"
#include "UI/Widget/SelectPandoraWidget.h"
#include "UI/Widget/PandoraTreeWidget.h"
#include "UI/Widget/RightNotificationsWidget.h"
#include "UI/Widget/StatusEffectWidget.h"
#include "UI/Widget/TrainingRoomMenuPopupWidget.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WidgetClassDefinition)

namespace
{
	class FWidgetRuntimeSoftPathCollector
	{
	public:
		explicit FWidgetRuntimeSoftPathCollector(TSet<FSoftObjectPath>& InPaths)
			: Paths(InPaths)
		{
		}

		void Collect(const UObject* Object)
		{
			if (IsValid(Object))
			{
				CollectStruct(Object->GetClass(), Object);
			}
		}

		void CollectStructValue(const UStruct* Struct, const void* StructValue)
		{
			CollectStruct(Struct, StructValue);
		}

		void CollectSoftPath(const FSoftObjectPath& AssetPath)
		{
			AddPath(AssetPath);
		}

	private:
		void CollectStruct(const UStruct* Struct, const void* StructValue)
		{
			if (!Struct || !StructValue || VisitedStructValues.Contains(StructValue))
			{
				return;
			}

			VisitedStructValues.Add(StructValue);

			for (TPropertyValueIterator<const FProperty> It(Struct, StructValue); It; ++It)
			{
				const FProperty* Property = It.Key();
				const void* PropertyValue = It.Value();
				FSoftObjectPath FoundPath;

				if (const FSoftClassProperty* SoftClassProperty =
					CastField<FSoftClassProperty>(Property))
				{
					FoundPath =
						SoftClassProperty->GetPropertyValue(PropertyValue).ToSoftObjectPath();
				}
				else if (const FSoftObjectProperty* SoftObjectProperty =
					CastField<FSoftObjectProperty>(Property))
				{
					FoundPath =
						SoftObjectProperty->GetPropertyValue(PropertyValue).ToSoftObjectPath();
				}
				else if (const FStructProperty* StructProperty =
					CastField<FStructProperty>(Property))
				{
					if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get()
						|| StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
					{
						// FSoftClassPath and FSoftObjectPath are binary-compatible.
						FoundPath =
							*reinterpret_cast<const FSoftObjectPath*>(PropertyValue);
						It.SkipRecursiveProperty();
					}
				}

				AddPath(FoundPath);
			}
		}

		void AddPath(const FSoftObjectPath& AssetPath)
		{
			if (AssetPath.IsValid() && !AssetPath.IsNull())
			{
				Paths.Add(AssetPath);
			}
		}

		TSet<FSoftObjectPath>& Paths;
		TSet<const void*> VisitedStructValues;
	};

	const UWidgetClassDefinition* ResolveWidgetDefinitionFromLocalPlayer(const ULocalPlayer* LocalPlayer)
	{
		const UUiSubsystem* UiSubsystem =
			LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
		return UiSubsystem ? UiSubsystem->GetWidgetClassDefinition() : nullptr;
	}

}

void UWidgetClassDefinition::GetRuntimePreloadAssetPaths(
	const EWidgetContentBundle Bundle,
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	TSet<FSoftObjectPath> UniquePaths;
	FWidgetRuntimeSoftPathCollector Collector(UniquePaths);
	// Do not reflect over the root object here. It references every fragment and
	// settings from unrelated screen lifetimes, so collect only effective getters.
	const auto CollectSettings = [&Collector](const auto& Settings)
	{
		using FSettingsType = std::decay_t<decltype(Settings)>;
		Collector.CollectStructValue(FSettingsType::StaticStruct(), &Settings);
	};

	switch (Bundle)
	{
	case EWidgetContentBundle::Core:
		CollectSettings(GetConnectingPopupWidgetSettings());
		CollectSettings(GetTitleAuxiliaryWidgetSettings());
		break;

	case EWidgetContentBundle::Lobby:
		CollectSettings(GetRecordWidgetSettings());
		CollectSettings(GetRoomListWidgetSettings());
		CollectSettings(GetLobbyWidgetSettings());
		break;

	case EWidgetContentBundle::InGame:
		CollectSettings(GetPlayerHudWidgetSettings());
		CollectSettings(GetKillBoxWidgetSettings());
		CollectSettings(GetSelectPandoraWidgetSettings());
		CollectSettings(GetAimCrosshairWidgetSettings());
		CollectSettings(GetRightNotificationsWidgetSettings());
		CollectSettings(GetMenuPopupWidgetSettings());
		CollectSettings(GetStatusEffectsBarWidgetSettings());
		CollectSettings(GetGameResultWidgetSettings());
		CollectSettings(GetCharacterWidgetSettings());
		CollectSettings(GetAbilitySlotWidgetSettings());
		CollectSettings(GetSkillTipEffectIconSettings());
		CollectSettings(GetQuickSlotWidgetSettings());
		CollectSettings(GetActionSlotWidgetSettings());
		Collector.CollectSoftPath(GetKillBoxEntryWidgetClass().ToSoftObjectPath());
		Collector.CollectSoftPath(GetQuickSlotEntryWidgetClass().ToSoftObjectPath());
		Collector.CollectSoftPath(GetActionSlotEntryWidgetClass().ToSoftObjectPath());
		Collector.CollectSoftPath(GetTogglePandoraTreeInputAction().ToSoftObjectPath());
		break;

	case EWidgetContentBundle::Info:
		CollectSettings(GetInfoWidgetSettings());
		CollectSettings(GetPandoraTreeWidgetSettings());
		CollectSettings(GetPandoraWidgetSettings());
		CollectSettings(GetRightPandoraWidgetSettings());
		CollectSettings(GetInfoAuxiliaryWidgetSettings());
		CollectSettings(GetInventoryWidgetSettings());
		CollectSettings(GetSkinWidgetSettings());
		CollectSettings(GetPandoraDescriptionEffectIconSettings());
		break;

	case EWidgetContentBundle::Map:
		CollectSettings(GetMapWidgetSettings());
		break;

	}

	OutAssetPaths = UniquePaths.Array();
	OutAssetPaths.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
	{
		return Left.ToString() < Right.ToString();
	});
}

FPrimaryAssetId UWidgetClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetClassDefinition"), GetFName());
}
const UWidgetClassDefinition* UWidgetClassDefinition::ResolveWidgetClassDefinition(const UObject* WorldContextObject)
{
	if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(WorldContextObject))
	{
		return ResolveWidgetDefinitionFromLocalPlayer(LocalPlayer);
	}

	if (const UUserWidget* UserWidget = Cast<UUserWidget>(WorldContextObject))
	{
		if (const UWidgetClassDefinition* WidgetDefinition =
			ResolveWidgetDefinitionFromLocalPlayer(UserWidget->GetOwningLocalPlayer()))
		{
			return WidgetDefinition;
		}
	}

	if (const APlayerController* PlayerController = Cast<APlayerController>(WorldContextObject))
	{
		if (const UWidgetClassDefinition* WidgetDefinition =
			ResolveWidgetDefinitionFromLocalPlayer(Cast<ULocalPlayer>(PlayerController->Player)))
		{
			return WidgetDefinition;
		}
	}

	if (const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		if (!GameInstance)
		{
			return nullptr;
		}

		for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (!LocalPlayer || !LocalPlayer->GetPlayerController(World))
			{
				continue;
			}

			if (const UWidgetClassDefinition* WidgetDefinition =
				ResolveWidgetDefinitionFromLocalPlayer(LocalPlayer))
			{
				return WidgetDefinition;
			}
		}
	}

#if WITH_EDITOR
	const UWorld* ContextWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!ContextWorld || !ContextWorld->IsGameWorld())
	{
		return UUiSubsystem::LoadConfiguredEditorWidgetClassDefinition();
	}
#endif

	return nullptr;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::FindWidgetClassByTag(FGameplayTag WidgetTag) const
{
	if (!WidgetTag.IsValid())
	{
		return nullptr;
	}

	const FGameplayTag& PlayerHudTag = GetPlayerHudWidgetSettings().WidgetTag;
	if (PlayerHudTag.MatchesTagExact(WidgetTag))
	{
		return GetPlayerHudWidgetClass();
	}

	const FGameplayTag& InfoTag = GetInfoWidgetSettings().WidgetTag;
	if (InfoTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetInfoWidgetClass().Get());
	}

	const FGameplayTag& SelectPandoraTag = GetSelectPandoraWidgetSettings().WidgetTag;
	if (SelectPandoraTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetSelectPandoraWidgetClass().Get());
	}

	const FGameplayTag& AimCrosshairTag = GetAimCrosshairWidgetSettings().WidgetTag;
	if (AimCrosshairTag.MatchesTagExact(WidgetTag))
	{
		return GetAimCrosshairWidgetClass();
	}

	const FGameplayTag& PandoraTreeTag = GetPandoraTreeWidgetSettings().WidgetTag;
	if (PandoraTreeTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetPandoraTreeWidgetClass().Get());
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetPlayerHudWidgetClass() const
{
	return GetPlayerHudWidgetSettings().WidgetClass;
}

TSubclassOf<UInfoWidget> UWidgetClassDefinition::GetInfoWidgetClass() const
{
	return GetInfoWidgetSettings().WidgetClass;
}

TSubclassOf<USelectPandoraWidget> UWidgetClassDefinition::GetSelectPandoraWidgetClass() const
{
	return GetSelectPandoraWidgetSettings().WidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetAimCrosshairWidgetClass() const
{
	return GetAimCrosshairWidgetSettings().WidgetClass;
}

TSubclassOf<UPandoraTreeWidget> UWidgetClassDefinition::GetPandoraTreeWidgetClass() const
{
	return GetPandoraTreeWidgetSettings().WidgetClass;
}

TSubclassOf<URightNotificationsWidget> UWidgetClassDefinition::GetRightNotificationsWidgetClass() const
{
	return GetRightNotificationsWidgetSettings().WidgetClass;
}

TSubclassOf<UMenuPopupWidget> UWidgetClassDefinition::GetMenuPopupWidgetClass() const
{
	return GetMenuPopupWidgetSettings().MenuPopupWidgetClass;
}

TSubclassOf<UTrainingRoomMenuPopupWidget> UWidgetClassDefinition::GetTrainingRoomMenuPopupWidgetClass() const
{
	return GetMenuPopupWidgetSettings().TrainingRoomMenuPopupWidgetClass;
}

TSubclassOf<UConnectingPopupWidget> UWidgetClassDefinition::GetConnectingPopupWidgetClass() const
{
	return GetConnectingPopupWidgetSettings().WidgetClass;
}

TSubclassOf<UShopWidget> UWidgetClassDefinition::GetShopWidgetClass() const
{
	return GetTitleAuxiliaryWidgetSettings().ShopWidgetClass;
}

TSubclassOf<UGuideWidget> UWidgetClassDefinition::GetGuideWidgetClass() const
{
	return GetTitleAuxiliaryWidgetSettings().GuideWidgetClass;
}

TSubclassOf<URecordWidget> UWidgetClassDefinition::GetRecordWidgetClass() const
{
	return GetTitleAuxiliaryWidgetSettings().RecordWidgetClass;
}

TSubclassOf<UMapWidget> UWidgetClassDefinition::GetTotalMapWidgetClass() const
{
	return GetInfoAuxiliaryWidgetSettings().TotalMapWidgetClass;
}

TSubclassOf<AActor> UWidgetClassDefinition::GetCharacterPreviewClass() const
{
	return GetInfoAuxiliaryWidgetSettings().CharacterPreviewClass;
}

TSubclassOf<UPandoraDescriptionWidget> UWidgetClassDefinition::GetPandoraDescriptionWidgetClass() const
{
	return GetInfoAuxiliaryWidgetSettings().PandoraDescriptionWidgetClass;
}

TSubclassOf<URecordEntryWidget> UWidgetClassDefinition::GetRecordEntryWidgetClass() const
{
	return GetRecordWidgetSettings().RecordEntryWidgetClass;
}

TSubclassOf<UStatusEffectWidget> UWidgetClassDefinition::GetStatusEffectWidgetClass() const
{
	return GetStatusEffectsBarWidgetSettings().StatusEffectWidgetClass;
}

TSubclassOf<URoomItemWidget> UWidgetClassDefinition::GetRoomItemWidgetClass() const
{
	return GetRoomListWidgetSettings().RoomItemWidgetClass;
}

TSubclassOf<UCreateRoomPopupWidget> UWidgetClassDefinition::GetCreateRoomPopupWidgetClass() const
{
	return GetRoomListWidgetSettings().CreateRoomPopupWidgetClass;
}

TSubclassOf<ULobbyUserWidget> UWidgetClassDefinition::GetLobbyUserWidgetClass() const
{
	return GetLobbyWidgetSettings().LobbyUserWidgetClass;
}

TSubclassOf<UGameConfigWidget> UWidgetClassDefinition::GetGameConfigWidgetClass() const
{
	return GetLobbyWidgetSettings().GameConfigWidgetClass;
}

TSubclassOf<UGameResultWidget> UWidgetClassDefinition::GetGameResultWidgetClass() const
{
	return GetGameResultWidgetSettings().GameResultWidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetHealthBarWidgetClass() const
{
	return GetCharacterWidgetSettings().HealthBarWidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetEnemyAvatarWidgetClass() const
{
	return GetCharacterWidgetSettings().EnemyAvatarWidgetClass;
}

TSubclassOf<UInfoUiPresenter> UWidgetClassDefinition::GetInfoPresenterClass() const
{
	return GetInfoWidgetSettings().PresenterClass;
}

TSubclassOf<UNotificationEntryWidget> UWidgetClassDefinition::GetNotificationEntryWidgetClass() const
{
	return GetRightNotificationsWidgetSettings().NotificationEntryWidgetClass;
}

TSoftClassPtr<UKillBoxWidget> UWidgetClassDefinition::GetKillBoxEntryWidgetClass() const
{
	return GetKillBoxWidgetSettings().EntryWidgetClass;
}

TSoftClassPtr<UQuickSlotEntryWidget> UWidgetClassDefinition::GetQuickSlotEntryWidgetClass() const
{
	return GetQuickSlotWidgetSettings().EntryWidgetClass;
}

TSoftClassPtr<UActionSlotEntryWidget> UWidgetClassDefinition::GetActionSlotEntryWidgetClass() const
{
	return GetActionSlotWidgetSettings().EntryWidgetClass;
}

TSoftObjectPtr<UInputAction> UWidgetClassDefinition::GetTogglePandoraTreeInputAction() const
{
	return InputIcons->TogglePandoraTreeInputAction;
}

int32 UWidgetClassDefinition::GetInventoryItemCountLimit(const bool bTrainingRoom) const
{
	const FInventoryWidgetSettings& Settings = GetInventoryWidgetSettings();
	return FMath::Max(
		bTrainingRoom
			? Settings.TrainingRoomInventoryItemCountLimit
			: Settings.GameInventoryItemCountLimit,
		0);
}

const FKillBoxWidgetSettings& UWidgetClassDefinition::GetKillBoxWidgetSettings() const
{
	return Style->KillBoxWidgetSettings;
}

const FInfoWidgetSettings& UWidgetClassDefinition::GetInfoWidgetSettings() const
{
	return Style->InfoWidgetSettings;
}

const FSelectPandoraWidgetSettings& UWidgetClassDefinition::GetSelectPandoraWidgetSettings() const
{
	return Style->SelectPandoraWidgetSettings;
}

const FPandoraTreeWidgetSettings& UWidgetClassDefinition::GetPandoraTreeWidgetSettings() const
{
	return Style->PandoraTreeWidgetSettings;
}

const FPandoraWidgetSettings& UWidgetClassDefinition::GetPandoraWidgetSettings() const
{
	return Style->PandoraWidgetSettings;
}

const FRightPandoraWidgetSettings& UWidgetClassDefinition::GetRightPandoraWidgetSettings() const
{
	return Style->RightPandoraWidgetSettings;
}

const FRightNotificationsWidgetSettings& UWidgetClassDefinition::GetRightNotificationsWidgetSettings() const
{
	return Style->RightNotificationsWidgetSettings;
}

const FTitleAuxiliaryWidgetSettings& UWidgetClassDefinition::GetTitleAuxiliaryWidgetSettings() const
{
	return Style->TitleAuxiliaryWidgetSettings;
}

const FMapWidgetSettings& UWidgetClassDefinition::GetMapWidgetSettings() const
{
	return MapUI->MapWidgetSettings;
}

const FRecordWidgetSettings& UWidgetClassDefinition::GetRecordWidgetSettings() const
{
	return Style->RecordWidgetSettings;
}

const FStatusEffectsBarWidgetSettings& UWidgetClassDefinition::GetStatusEffectsBarWidgetSettings() const
{
	return Style->StatusEffectsBarWidgetSettings;
}

const FRoomListWidgetSettings& UWidgetClassDefinition::GetRoomListWidgetSettings() const
{
	return Style->RoomListWidgetSettings;
}

const FLobbyWidgetSettings& UWidgetClassDefinition::GetLobbyWidgetSettings() const
{
	return Style->LobbyWidgetSettings;
}

const FInventoryWidgetSettings& UWidgetClassDefinition::GetInventoryWidgetSettings() const
{
	return Style->InventoryWidgetSettings;
}

const FSkinWidgetSettings& UWidgetClassDefinition::GetSkinWidgetSettings() const
{
	return Style->SkinWidgetSettings;
}

const FAbilitySlotWidgetSettings& UWidgetClassDefinition::GetAbilitySlotWidgetSettings() const
{
	return Style->AbilitySlotWidgetSettings;
}

const FSkillTipWidgetSettings& UWidgetClassDefinition::GetSkillTipEffectIconSettings() const
{
	return InputIcons->SkillTipEffectIconSettings;
}

const FSkillTipWidgetSettings& UWidgetClassDefinition::GetPandoraDescriptionEffectIconSettings() const
{
	return InputIcons->PandoraDescriptionEffectIconSettings;
}

const FQuickSlotWidgetSettings& UWidgetClassDefinition::GetQuickSlotWidgetSettings() const
{
	return Style->QuickSlotWidgetSettings;
}

const FActionSlotWidgetSettings& UWidgetClassDefinition::GetActionSlotWidgetSettings() const
{
	return InputIcons->ActionSlotWidgetSettings;
}
