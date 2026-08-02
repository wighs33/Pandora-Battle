#include "Definition/UI/WidgetClassDefinition.h"

#include "Chat/ChatEntryWidget.h"
#include "Common/LabGameplayTags.h"
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
#include "UI/InfoUiPresenter.h"
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

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

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

#if WITH_EDITOR
	void MarkWidgetClassInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	void AddWarning(FDataValidationContext& Context, const FText& Message)
	{
		Context.AddWarning(Message);
	}

	FText WidgetFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	void ValidateRequiredTag(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FGameplayTag& Tag,
		const TCHAR* FieldName)
	{
		if (!Tag.IsValid())
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "MissingGameplayTag", "{0} must be set."),
				WidgetFieldText(FieldName)));
		}
	}

	template <typename T>
	void ValidateRequiredClass(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSubclassOf<T>& WidgetClass,
		const TCHAR* FieldName)
	{
		if (!WidgetClass)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "MissingRequiredClass", "{0} must be set."),
				WidgetFieldText(FieldName)));
		}
	}

	template <typename T>
	void ValidateRecommendedClass(
		FDataValidationContext& Context,
		const TSubclassOf<T>& WidgetClass,
		const TCHAR* FieldName)
	{
		if (!WidgetClass)
		{
			AddWarning(Context, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "MissingRecommendedClass", "{0} is not set."),
				WidgetFieldText(FieldName)));
		}
	}

	template <typename T>
	void ValidateRequiredSoftClass(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TSoftClassPtr<T>& WidgetClass,
		const TCHAR* FieldName)
	{
		if (WidgetClass.IsNull())
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "MissingRequiredSoftClass", "{0} must be set."),
				WidgetFieldText(FieldName)));
		}
	}

	template <typename T>
	void ValidateRecommendedSoftObject(
		FDataValidationContext& Context,
		const TSoftObjectPtr<T>& Object,
		const TCHAR* FieldName)
	{
		if (Object.IsNull())
		{
			AddWarning(Context, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "MissingRecommendedSoftObject", "{0} is not set."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidatePositiveFloat(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidPositiveFloat", "{0} must be greater than 0."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidateNonNegativeFloat(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidNonNegativeFloat", "{0} cannot be negative."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidateFloatRangeInclusive(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float Value,
		const float MinValue,
		const float MaxValue,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value) || Value < MinValue || Value > MaxValue)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidFloatRange", "{0} must be between {1} and {2}."),
				WidgetFieldText(FieldName),
				FText::AsNumber(MinValue),
				FText::AsNumber(MaxValue)));
		}
	}

	void ValidatePositiveInt32(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const int32 Value,
		const TCHAR* FieldName)
	{
		if (Value <= 0)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidPositiveInt32", "{0} must be greater than 0."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidateNonNegativeInt32(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const int32 Value,
		const TCHAR* FieldName)
	{
		if (Value < 0)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidNonNegativeInt32", "{0} cannot be negative."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidatePositiveVector2D(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FVector2D& Value,
		const TCHAR* FieldName)
	{
		if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || Value.X <= 0.0f || Value.Y <= 0.0f)
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidPositiveVector2D", "{0} must have positive X and Y."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidateOptionalImageSize(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FVector2D& Value,
		const TCHAR* FieldName)
	{
		const bool bHasAnySize = Value.X > 0.0f || Value.Y > 0.0f;
		if (!FMath::IsFinite(Value.X) || !FMath::IsFinite(Value.Y) || (bHasAnySize && (Value.X <= 0.0f || Value.Y <= 0.0f)))
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "InvalidOptionalImageSize", "{0} must be zero on both axes or positive on both axes."),
				WidgetFieldText(FieldName)));
		}
	}

	void ValidateInputKeyIconSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FPdInputKeyIconSettings& Settings,
		const TCHAR* FieldName)
	{
		if (!Settings.bHideInputKeyIcon)
		{
			ValidatePositiveVector2D(Context, Result, Settings.IconSize, FieldName);
		}

		if (!Settings.bHideInputKeyIcon && Settings.StringToIconMapping.IsEmpty())
		{
			AddWarning(Context, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "EmptyInputKeyIconMapping", "{0}.StringToIconMapping is empty while key icons are visible."),
				WidgetFieldText(FieldName)));
		}

		for (const TPair<FString, TSoftObjectPtr<UObject>>& Pair : Settings.StringToIconMapping)
		{
			if (Pair.Key.TrimStartAndEnd().IsEmpty())
			{
				MarkWidgetClassInvalid(Context, Result, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "EmptyInputKeyIconKey", "{0}.StringToIconMapping contains an empty key name."),
					WidgetFieldText(FieldName)));
			}

			if (Pair.Value.IsNull())
			{
				AddWarning(Context, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "EmptyInputKeyIconValue", "{0}.StringToIconMapping[{1}] has no icon asset."),
					WidgetFieldText(FieldName),
					FText::FromString(Pair.Key)));
			}
		}
	}

	void ValidateProjectionSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FMapWidgetProjectionSettings& Settings,
		const TCHAR* FieldName)
	{
		ValidatePositiveVector2D(Context, Result, Settings.AreaMapTotalSizeBoxSize, TEXT("MapWidgetSettings.AreaMapTotalSizeBoxSize"));
		ValidatePositiveVector2D(Context, Result, Settings.WindmillMapTotalSizeBoxSize, TEXT("MapWidgetSettings.WindmillMapTotalSizeBoxSize"));
		ValidatePositiveVector2D(Context, Result, Settings.DomeMapTotalSizeBoxSize, TEXT("MapWidgetSettings.DomeMapTotalSizeBoxSize"));
		ValidatePositiveVector2D(Context, Result, Settings.TempleMapTotalSizeBoxSize, TEXT("MapWidgetSettings.TempleMapTotalSizeBoxSize"));
		ValidatePositiveFloat(Context, Result, Settings.PlaneLocalSize, FieldName);
	}

	void ValidateMapSettings(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FMapWidgetSettings& Settings)
	{
		if (Settings.AreaMapTexture.IsNull()
			&& Settings.WindmillMapTexture.IsNull()
			&& Settings.DomeMapTexture.IsNull()
			&& Settings.TempleMapTexture.IsNull())
		{
			AddWarning(Context, NSLOCTEXT("WidgetClassDefinition", "NoMapTextures", "MapWidgetSettings has no map textures configured."));
		}

		ValidateOptionalImageSize(Context, Result, Settings.CharacterMarkImageSize, TEXT("MapWidgetSettings.CharacterMarkImageSize"));
		ValidatePositiveFloat(Context, Result, Settings.MarkerUpdateInterval, TEXT("MapWidgetSettings.MarkerUpdateInterval"));
		ValidatePositiveFloat(Context, Result, Settings.RemotePlayerListRefreshInterval, TEXT("MapWidgetSettings.RemotePlayerListRefreshInterval"));

		if (Settings.bUseDefaultProjectionSettings)
		{
			ValidateProjectionSettings(Context, Result, Settings.DefaultProjectionSettings, TEXT("MapWidgetSettings.DefaultProjectionSettings.PlaneLocalSize"));
		}

		TSet<FSoftObjectPath> ProjectionOverrideClasses;
		for (int32 Index = 0; Index < Settings.ProjectionOverrides.Num(); ++Index)
		{
			const FMapWidgetProjectionOverride& Override = Settings.ProjectionOverrides[Index];
			if (Override.MapWidgetClass.IsNull())
			{
				MarkWidgetClassInvalid(Context, Result, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "MissingProjectionOverrideClass", "MapWidgetSettings.ProjectionOverrides[{0}].MapWidgetClass must be set."),
					FText::AsNumber(Index)));
				continue;
			}

			const FSoftObjectPath MapWidgetClassPath = Override.MapWidgetClass.ToSoftObjectPath();
			if (ProjectionOverrideClasses.Contains(MapWidgetClassPath))
			{
				MarkWidgetClassInvalid(Context, Result, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "DuplicateProjectionOverrideClass", "MapWidgetSettings.ProjectionOverrides contains duplicate MapWidgetClass: {0}"),
					FText::FromString(Override.MapWidgetClass.ToString())));
			}
			ProjectionOverrideClasses.Add(MapWidgetClassPath);

			ValidateProjectionSettings(Context, Result, Override.ProjectionSettings, TEXT("MapWidgetSettings.ProjectionOverrides.PlaneLocalSize"));
		}

		TSet<int32> ConfiguredTeamColors;
		for (int32 Index = 0; Index < Settings.TeamMarkImages.Num(); ++Index)
		{
			const FMapWidgetTeamMarkImage& TeamMarkImage = Settings.TeamMarkImages[Index];
			const int32 TeamColorIndex = static_cast<int32>(TeamMarkImage.TeamColor);
			if (ConfiguredTeamColors.Contains(TeamColorIndex))
			{
				MarkWidgetClassInvalid(Context, Result, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "DuplicateTeamMarkImage", "MapWidgetSettings.TeamMarkImages contains duplicate team color entry: {0}"),
					FText::AsNumber(TeamColorIndex)));
			}
			ConfiguredTeamColors.Add(TeamColorIndex);

			if (TeamMarkImage.TeamMarkImage.IsNull())
			{
				AddWarning(Context, FText::Format(
					NSLOCTEXT("WidgetClassDefinition", "MissingTeamMarkImage", "MapWidgetSettings.TeamMarkImages[{0}] has no mark image."),
					FText::AsNumber(Index)));
			}

			ValidateOptionalImageSize(Context, Result, TeamMarkImage.TeamMarkImageSize, TEXT("MapWidgetSettings.TeamMarkImages.TeamMarkImageSize"));
		}
	}
#endif
}

void UWidgetClassDefinition::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	TSet<FSoftObjectPath> UniquePaths;
	FWidgetRuntimeSoftPathCollector Collector(UniquePaths);
	Collector.Collect(this);
	Collector.Collect(UIClasses);
	Collector.Collect(Style);
	Collector.Collect(InputIcons);
	Collector.Collect(MapUI);

	OutAssetPaths = UniquePaths.Array();
	OutAssetPaths.Sort([](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
	{
		return Left.ToString() < Right.ToString();
	});
}

UWidgetClassDefinition::UWidgetClassDefinition()
{
	if (const UWidgetUIClassesDefinition* UIClassDefaults = GetDefault<UWidgetUIClassesDefinition>())
	{
		PlayerHudWidgetSettings.WidgetTag = UIClassDefaults->PlayerHudWidgetTag;
		KillBoxWidgetSettings.EntryWidgetClass = UIClassDefaults->KillBoxEntryWidgetClass;
		InfoWidgetSettings.WidgetTag = UIClassDefaults->InfoWidgetTag;
		InfoWidgetSettings.PresenterClass = UIClassDefaults->InfoPresenterClass;
		SelectPandoraWidgetSettings.WidgetTag = UIClassDefaults->SelectPandoraWidgetTag;
		AimCrosshairWidgetSettings.WidgetTag = UIClassDefaults->AimCrosshairWidgetTag;
		PandoraTreeWidgetSettings.WidgetTag = UIClassDefaults->PandoraTreeWidgetTag;
		QuickSlotWidgetSettings.EntryWidgetClass = UIClassDefaults->QuickSlotEntryWidgetClass;
		ActionSlotWidgetSettings.EntryWidgetClass = UIClassDefaults->ActionSlotEntryWidgetClass;
	}

	if (const UWidgetStyleDefinition* StyleDefaults = GetDefault<UWidgetStyleDefinition>())
	{
		ChatWidgetSettings.ChatScrollBoxCandidateNames =
			StyleDefaults->ChatWidgetSettings.ChatScrollBoxCandidateNames;
		ChatWidgetSettings.ChatInputCandidateNames =
			StyleDefaults->ChatWidgetSettings.ChatInputCandidateNames;
	}

	if (const UWidgetInputIconsDefinition* InputDefaults = GetDefault<UWidgetInputIconsDefinition>())
	{
		AbilitySlotWidgetSettings = InputDefaults->AbilitySlotWidgetSettings;
		QuickSlotWidgetSettings.InputKeyIconSettings =
			InputDefaults->QuickSlotWidgetSettings.InputKeyIconSettings;
		QuickSlotWidgetSettings.SlotCount = InputDefaults->QuickSlotWidgetSettings.SlotCount;
		QuickSlotWidgetSettings.QuickSlot1InputAction = InputDefaults->QuickSlotWidgetSettings.QuickSlot1InputAction;
		QuickSlotWidgetSettings.QuickSlot2InputAction = InputDefaults->QuickSlotWidgetSettings.QuickSlot2InputAction;
		QuickSlotWidgetSettings.QuickSlot3InputAction = InputDefaults->QuickSlotWidgetSettings.QuickSlot3InputAction;
		QuickSlotWidgetSettings.QuickSlot4InputAction = InputDefaults->QuickSlotWidgetSettings.QuickSlot4InputAction;
		QuickSlotWidgetSettings.GestureSlot1InputAction = InputDefaults->QuickSlotWidgetSettings.GestureSlot1InputAction;
		QuickSlotWidgetSettings.GestureSlot2InputAction = InputDefaults->QuickSlotWidgetSettings.GestureSlot2InputAction;
		QuickSlotWidgetSettings.GestureSlot3InputAction = InputDefaults->QuickSlotWidgetSettings.GestureSlot3InputAction;
		QuickSlotWidgetSettings.GestureSlot4InputAction = InputDefaults->QuickSlotWidgetSettings.GestureSlot4InputAction;
		ActionSlotWidgetSettings.InputKeyIconSettings =
			InputDefaults->ActionSlotWidgetSettings.InputKeyIconSettings;
		ActionSlotWidgetSettings.CharacterActionDefinition =
			InputDefaults->ActionSlotWidgetSettings.CharacterActionDefinition;
		ActionSlotWidgetSettings.PandoraWeaponSwapInputAction =
			InputDefaults->ActionSlotWidgetSettings.PandoraWeaponSwapInputAction;
		ActionSlotWidgetSettings.GrappleHookInputAction =
			InputDefaults->ActionSlotWidgetSettings.GrappleHookInputAction;
	}

	if (const UWidgetMapUIDefinition* MapDefaults = GetDefault<UWidgetMapUIDefinition>())
	{
		MapWidgetSettings = MapDefaults->MapWidgetSettings;
		MenuPopupWidgetSettings.TrainingRoomMapNames = MapDefaults->TrainingRoomMapNames;
	}
}

FPrimaryAssetId UWidgetClassDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WidgetClassDefinition"), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UWidgetClassDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	const FGameplayTag PlayerHudWidgetTag = UIClasses && UIClasses->PlayerHudWidgetTag.IsValid()
		? UIClasses->PlayerHudWidgetTag
		: PlayerHudWidgetSettings.WidgetTag;
	const FGameplayTag InfoWidgetTag = UIClasses && UIClasses->InfoWidgetTag.IsValid()
		? UIClasses->InfoWidgetTag
		: InfoWidgetSettings.WidgetTag;
	const FGameplayTag SelectPandoraWidgetTag = UIClasses && UIClasses->SelectPandoraWidgetTag.IsValid()
		? UIClasses->SelectPandoraWidgetTag
		: SelectPandoraWidgetSettings.WidgetTag;
	const FGameplayTag AimCrosshairWidgetTag = UIClasses && UIClasses->AimCrosshairWidgetTag.IsValid()
		? UIClasses->AimCrosshairWidgetTag
		: AimCrosshairWidgetSettings.WidgetTag;
	const FGameplayTag PandoraTreeWidgetTag = UIClasses && UIClasses->PandoraTreeWidgetTag.IsValid()
		? UIClasses->PandoraTreeWidgetTag
		: PandoraTreeWidgetSettings.WidgetTag;

	ValidateRequiredTag(Context, Result, PlayerHudWidgetTag, TEXT("UIClasses.PlayerHudWidgetTag"));
	ValidateRequiredTag(Context, Result, InfoWidgetTag, TEXT("UIClasses.InfoWidgetTag"));
	ValidateRequiredTag(Context, Result, SelectPandoraWidgetTag, TEXT("UIClasses.SelectPandoraWidgetTag"));
	ValidateRequiredTag(Context, Result, AimCrosshairWidgetTag, TEXT("UIClasses.AimCrosshairWidgetTag"));
	ValidateRequiredTag(Context, Result, PandoraTreeWidgetTag, TEXT("UIClasses.PandoraTreeWidgetTag"));

	TMap<FGameplayTag, FString> WidgetTagOwners;
	const auto RegisterWidgetTag = [&Context, &Result, &WidgetTagOwners](const FGameplayTag& WidgetTag, const TCHAR* FieldName)
	{
		if (!WidgetTag.IsValid())
		{
			return;
		}

		if (const FString* ExistingOwner = WidgetTagOwners.Find(WidgetTag))
		{
			MarkWidgetClassInvalid(Context, Result, FText::Format(
				NSLOCTEXT("WidgetClassDefinition", "DuplicateWidgetTag", "{0} duplicates widget tag already used by {1}: {2}"),
				WidgetFieldText(FieldName),
				FText::FromString(*ExistingOwner),
				FText::FromString(WidgetTag.ToString())));
			return;
		}

		WidgetTagOwners.Add(WidgetTag, FieldName);
	};

	RegisterWidgetTag(PlayerHudWidgetTag, TEXT("UIClasses.PlayerHudWidgetTag"));
	RegisterWidgetTag(InfoWidgetTag, TEXT("UIClasses.InfoWidgetTag"));
	RegisterWidgetTag(SelectPandoraWidgetTag, TEXT("UIClasses.SelectPandoraWidgetTag"));
	RegisterWidgetTag(AimCrosshairWidgetTag, TEXT("UIClasses.AimCrosshairWidgetTag"));
	RegisterWidgetTag(PandoraTreeWidgetTag, TEXT("UIClasses.PandoraTreeWidgetTag"));

	ValidateRequiredClass(Context, Result, GetPlayerHudWidgetClass(), TEXT("UIClasses.PlayerHudWidgetClass"));
	ValidateRequiredSoftClass(Context, Result, GetKillBoxEntryWidgetClass(), TEXT("UIClasses.KillBoxEntryWidgetClass"));
	ValidateRequiredClass(Context, Result, GetInfoWidgetClass(), TEXT("UIClasses.InfoWidgetClass"));
	ValidateRequiredClass(Context, Result, GetInfoPresenterClass(), TEXT("UIClasses.InfoPresenterClass"));
	ValidateRequiredClass(Context, Result, GetSelectPandoraWidgetClass(), TEXT("UIClasses.SelectPandoraWidgetClass"));
	ValidateRequiredClass(Context, Result, GetAimCrosshairWidgetClass(), TEXT("UIClasses.AimCrosshairWidgetClass"));
	ValidateRequiredClass(Context, Result, GetPandoraTreeWidgetClass(), TEXT("UIClasses.PandoraTreeWidgetClass"));
	ValidateRequiredClass(Context, Result, GetRightNotificationsWidgetClass(), TEXT("UIClasses.RightNotificationsWidgetClass"));
	ValidateRequiredClass(Context, Result, GetNotificationEntryWidgetClass(), TEXT("UIClasses.NotificationEntryWidgetClass"));
	ValidateRequiredClass(Context, Result, GetConnectingPopupWidgetClass(), TEXT("UIClasses.ConnectingPopupWidgetClass"));
	ValidateRequiredClass(Context, Result, GetLobbyUserWidgetClass(), TEXT("UIClasses.LobbyUserWidgetClass"));
	ValidateRequiredClass(Context, Result, GetGameConfigWidgetClass(), TEXT("UIClasses.GameConfigWidgetClass"));
	ValidateRequiredClass(Context, Result, GetGameResultWidgetClass(), TEXT("UIClasses.GameResultWidgetClass"));
	ValidateRequiredSoftClass(Context, Result, GetQuickSlotEntryWidgetClass(), TEXT("UIClasses.QuickSlotEntryWidgetClass"));
	ValidateRequiredSoftClass(Context, Result, GetActionSlotEntryWidgetClass(), TEXT("UIClasses.ActionSlotEntryWidgetClass"));

	ValidateRecommendedClass(Context, GetMenuPopupWidgetClass(), TEXT("UIClasses.MenuPopupWidgetClass"));
	ValidateRecommendedClass(Context, GetTrainingRoomMenuPopupWidgetClass(), TEXT("UIClasses.TrainingRoomMenuPopupWidgetClass"));
	ValidateRecommendedClass(Context, GetShopWidgetClass(), TEXT("UIClasses.ShopWidgetClass"));
	ValidateRecommendedClass(Context, GetGuideWidgetClass(), TEXT("UIClasses.GuideWidgetClass"));
	ValidateRecommendedClass(Context, GetRecordWidgetClass(), TEXT("UIClasses.RecordWidgetClass"));
	ValidateRecommendedClass(Context, GetTotalMapWidgetClass(), TEXT("UIClasses.TotalMapWidgetClass"));
	ValidateRecommendedClass(Context, GetCharacterPreviewClass(), TEXT("UIClasses.CharacterPreviewClass"));
	ValidateRecommendedClass(Context, GetPandoraDescriptionWidgetClass(), TEXT("UIClasses.PandoraDescriptionWidgetClass"));
	ValidateRecommendedClass(Context, GetRecordEntryWidgetClass(), TEXT("UIClasses.RecordEntryWidgetClass"));
	ValidateRecommendedClass(Context, GetStatusEffectWidgetClass(), TEXT("UIClasses.StatusEffectWidgetClass"));
	ValidateRecommendedClass(Context, GetChatEntryWidgetClass(), TEXT("UIClasses.ChatEntryWidgetClass"));
	ValidateRecommendedClass(Context, GetRoomItemWidgetClass(), TEXT("UIClasses.RoomItemWidgetClass"));
	ValidateRecommendedClass(Context, GetCreateRoomPopupWidgetClass(), TEXT("UIClasses.CreateRoomPopupWidgetClass"));
	ValidateRecommendedClass(Context, GetHealthBarWidgetClass(), TEXT("UIClasses.HealthBarWidgetClass"));
	ValidateRecommendedClass(Context, GetEnemyAvatarWidgetClass(), TEXT("UIClasses.EnemyAvatarWidgetClass"));

	ValidateRecommendedSoftObject(Context, GetActionSlotWidgetSettings().CharacterActionDefinition, TEXT("ActionSlotWidgetSettings.CharacterActionDefinition"));
	ValidateRecommendedSoftObject(Context, GetAbilitySlotWidgetSettings().Skill1InputAction, TEXT("AbilitySlotWidgetSettings.Skill1InputAction"));
	ValidateRecommendedSoftObject(Context, GetAbilitySlotWidgetSettings().Skill2InputAction, TEXT("AbilitySlotWidgetSettings.Skill2InputAction"));
	ValidateRecommendedSoftObject(Context, GetAbilitySlotWidgetSettings().Skill3InputAction, TEXT("AbilitySlotWidgetSettings.Skill3InputAction"));
	ValidateRecommendedSoftObject(Context, GetAbilitySlotWidgetSettings().Skill4InputAction, TEXT("AbilitySlotWidgetSettings.Skill4InputAction"));
	ValidateRecommendedSoftObject(Context, GetSkillTipEffectIconSettings().BurnImage, TEXT("SkillTipWidgetSettings.BurnImage"));
	ValidateRecommendedSoftObject(Context, GetSkillTipEffectIconSettings().FrostbiteImage, TEXT("SkillTipWidgetSettings.FrostbiteImage"));
	ValidateRecommendedSoftObject(Context, GetSkillTipEffectIconSettings().ElectricShockImage, TEXT("SkillTipWidgetSettings.ElectricShockImage"));
	ValidateRecommendedSoftObject(Context, GetSkillTipEffectIconSettings().ShieldImage, TEXT("SkillTipWidgetSettings.ShieldImage"));
	ValidateRecommendedSoftObject(Context, GetPandoraDescriptionEffectIconSettings().BurnImage, TEXT("PandoraDescriptionEffectIconSettings.BurnImage"));
	ValidateRecommendedSoftObject(Context, GetPandoraDescriptionEffectIconSettings().FrostbiteImage, TEXT("PandoraDescriptionEffectIconSettings.FrostbiteImage"));
	ValidateRecommendedSoftObject(Context, GetPandoraDescriptionEffectIconSettings().ElectricShockImage, TEXT("PandoraDescriptionEffectIconSettings.ElectricShockImage"));
	ValidateRecommendedSoftObject(Context, GetPandoraDescriptionEffectIconSettings().ShieldImage, TEXT("PandoraDescriptionEffectIconSettings.ShieldImage"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().QuickSlot1InputAction, TEXT("QuickSlotWidgetSettings.QuickSlot1InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().QuickSlot2InputAction, TEXT("QuickSlotWidgetSettings.QuickSlot2InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().QuickSlot3InputAction, TEXT("QuickSlotWidgetSettings.QuickSlot3InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().QuickSlot4InputAction, TEXT("QuickSlotWidgetSettings.QuickSlot4InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().GestureSlot1InputAction, TEXT("QuickSlotWidgetSettings.GestureSlot1InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().GestureSlot2InputAction, TEXT("QuickSlotWidgetSettings.GestureSlot2InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().GestureSlot3InputAction, TEXT("QuickSlotWidgetSettings.GestureSlot3InputAction"));
	ValidateRecommendedSoftObject(Context, GetQuickSlotWidgetSettings().GestureSlot4InputAction, TEXT("QuickSlotWidgetSettings.GestureSlot4InputAction"));
	ValidateRecommendedSoftObject(Context, GetActionSlotWidgetSettings().PandoraWeaponSwapInputAction, TEXT("ActionSlotWidgetSettings.PandoraWeaponSwapInputAction"));
	ValidateRecommendedSoftObject(Context, GetActionSlotWidgetSettings().GrappleHookInputAction, TEXT("ActionSlotWidgetSettings.GrappleHookInputAction"));

	ValidateFloatRangeInclusive(Context, Result, GetAbilitySlotWidgetSettings().DisabledSlotOpacity, 0.0f, 1.0f, TEXT("AbilitySlotWidgetSettings.DisabledSlotOpacity"));
	ValidateFloatRangeInclusive(Context, Result, GetAbilitySlotWidgetSettings().ReadyInputKeyOpacity, 0.0f, 1.0f, TEXT("AbilitySlotWidgetSettings.ReadyInputKeyOpacity"));
	ValidateFloatRangeInclusive(Context, Result, GetAbilitySlotWidgetSettings().CooldownInputKeyOpacity, 0.0f, 1.0f, TEXT("AbilitySlotWidgetSettings.CooldownInputKeyOpacity"));
	ValidateFloatRangeInclusive(Context, Result, GetActionSlotWidgetSettings().ReadyInputKeyOpacity, 0.0f, 1.0f, TEXT("ActionSlotWidgetSettings.ReadyInputKeyOpacity"));
	ValidateFloatRangeInclusive(Context, Result, GetActionSlotWidgetSettings().CooldownInputKeyOpacity, 0.0f, 1.0f, TEXT("ActionSlotWidgetSettings.CooldownInputKeyOpacity"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().HideAnimationDelay, TEXT("InfoWidgetSettings.HideAnimationDelay"));
	ValidatePositiveVector2D(Context, Result, GetInfoWidgetSettings().DesignResolution, TEXT("InfoWidgetSettings.DesignResolution"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().LeftPanelWidth, TEXT("InfoWidgetSettings.LeftPanelWidth"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().RightPanelWidth, TEXT("InfoWidgetSettings.RightPanelWidth"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().MinCenterPreviewWidth, TEXT("InfoWidgetSettings.MinCenterPreviewWidth"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().BottomNavigationReservedHeight, TEXT("InfoWidgetSettings.BottomNavigationReservedHeight"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().BottomTabBarWidth, TEXT("InfoWidgetSettings.BottomTabBarWidth"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().BottomTabBarHeight, TEXT("InfoWidgetSettings.BottomTabBarHeight"));
	ValidateNonNegativeFloat(Context, Result, GetInfoWidgetSettings().BottomTabBarBottomPadding, TEXT("InfoWidgetSettings.BottomTabBarBottomPadding"));
	ValidatePositiveFloat(Context, Result, GetInfoWidgetSettings().MapSlideDuration, TEXT("InfoWidgetSettings.MapSlideDuration"));
	ValidatePositiveFloat(Context, Result, static_cast<float>(GetSelectPandoraWidgetSettings().SegmentAngle), TEXT("SelectPandoraWidgetSettings.SegmentAngle"));
	ValidateNonNegativeFloat(Context, Result, static_cast<float>(GetSelectPandoraWidgetSettings().DeadZoneRadius), TEXT("SelectPandoraWidgetSettings.DeadZoneRadius"));
	ValidateNonNegativeFloat(Context, Result, GetPandoraTreeWidgetSettings().HideAnimationDelay, TEXT("PandoraTreeWidgetSettings.HideAnimationDelay"));
	ValidateNonNegativeFloat(Context, Result, GetPandoraTreeWidgetSettings().PreviewCameraShowBlendTime, TEXT("PandoraTreeWidgetSettings.PreviewCameraShowBlendTime"));
	ValidateNonNegativeFloat(Context, Result, GetPandoraTreeWidgetSettings().PreviewCameraHideBlendTime, TEXT("PandoraTreeWidgetSettings.PreviewCameraHideBlendTime"));
	ValidatePositiveFloat(Context, Result, GetKillBoxWidgetSettings().RefreshInterval, TEXT("KillBoxWidgetSettings.RefreshInterval"));
	ValidatePositiveInt32(Context, Result, GetRightNotificationsWidgetSettings().MaxVisibleNotifications, TEXT("RightNotificationsWidgetSettings.MaxVisibleNotifications"));
	ValidateNonNegativeFloat(Context, Result, GetRightNotificationsWidgetSettings().NotificationLifetime, TEXT("RightNotificationsWidgetSettings.NotificationLifetime"));
	ValidateNonNegativeFloat(Context, Result, GetRightNotificationsWidgetSettings().NotificationDequeueInterval, TEXT("RightNotificationsWidgetSettings.NotificationDequeueInterval"));
	ValidateNonNegativeFloat(Context, Result, static_cast<float>(GetPandoraWidgetSettings().ButtonHoldDuration), TEXT("PandoraWidgetSettings.ButtonHoldDuration"));
	ValidatePositiveFloat(Context, Result, GetPandoraWidgetSettings().ButtonHoldUpdateInterval, TEXT("PandoraWidgetSettings.ButtonHoldUpdateInterval"));
	ValidatePositiveInt32(Context, Result, GetTitleAuxiliaryWidgetSettings().QuickMatchMaxSearchResults, TEXT("TitleAuxiliaryWidgetSettings.QuickMatchMaxSearchResults"));
	ValidatePositiveInt32(Context, Result, GetTitleAuxiliaryWidgetSettings().QuickMatchMaxPublicConnections, TEXT("TitleAuxiliaryWidgetSettings.QuickMatchMaxPublicConnections"));
	ValidatePositiveInt32(Context, Result, GetRecordWidgetSettings().MaxVisibleRecordEntries, TEXT("RecordWidgetSettings.MaxVisibleRecordEntries"));
	ValidatePositiveFloat(Context, Result, GetRecordWidgetSettings().MaxTierProgressWinCount, TEXT("RecordWidgetSettings.MaxTierProgressWinCount"));
	ValidatePositiveFloat(Context, Result, GetStatusEffectsBarWidgetSettings().MeterUpdateInterval, TEXT("StatusEffectsBarWidgetSettings.MeterUpdateInterval"));
	ValidateFloatRangeInclusive(Context, Result, GetStatusEffectsBarWidgetSettings().InitialIconOpacity, 0.0f, 1.0f, TEXT("StatusEffectsBarWidgetSettings.InitialIconOpacity"));
	ValidatePositiveVector2D(Context, Result, GetStatusEffectsBarWidgetSettings().IconImageSize, TEXT("StatusEffectsBarWidgetSettings.IconImageSize"));
	ValidatePositiveFloat(Context, Result, GetChatWidgetSettings().ScrollMultiplier, TEXT("ChatWidgetSettings.ScrollMultiplier"));
	ValidatePositiveInt32(Context, Result, GetQuickSlotWidgetSettings().SlotCount, TEXT("QuickSlotWidgetSettings.SlotCount"));
	ValidatePositiveInt32(Context, Result, GetRoomListWidgetSettings().MaxRoomSlots, TEXT("RoomListWidgetSettings.MaxRoomSlots"));
	ValidatePositiveInt32(Context, Result, GetRoomListWidgetSettings().MaxSearchResults, TEXT("RoomListWidgetSettings.MaxSearchResults"));
	ValidatePositiveInt32(Context, Result, GetRoomListWidgetSettings().MaxPublicConnections, TEXT("RoomListWidgetSettings.MaxPublicConnections"));
	ValidatePositiveInt32(Context, Result, GetLobbyWidgetSettings().MaxLobbySlots, TEXT("LobbyWidgetSettings.MaxLobbySlots"));
	ValidatePositiveFloat(Context, Result, GetLobbyWidgetSettings().GameStartCountdownTickInterval, TEXT("LobbyWidgetSettings.GameStartCountdownTickInterval"));
	ValidateNonNegativeInt32(Context, Result, GetInventoryWidgetSettings().GameInventoryItemCountLimit, TEXT("InventoryWidgetSettings.GameInventoryItemCountLimit"));
	ValidateNonNegativeInt32(Context, Result, GetInventoryWidgetSettings().TrainingRoomInventoryItemCountLimit, TEXT("InventoryWidgetSettings.TrainingRoomInventoryItemCountLimit"));
	ValidateNonNegativeInt32(Context, Result, GetSkinWidgetSettings().SkinSlotCount, TEXT("SkinWidgetSettings.SkinSlotCount"));

	ValidateInputKeyIconSettings(Context, Result, GetAbilitySlotWidgetSettings().InputKeyIconSettings, TEXT("AbilitySlotWidgetSettings.InputKeyIconSettings"));
	ValidateInputKeyIconSettings(Context, Result, GetQuickSlotWidgetSettings().InputKeyIconSettings, TEXT("QuickSlotWidgetSettings.InputKeyIconSettings"));
	ValidateInputKeyIconSettings(Context, Result, GetActionSlotWidgetSettings().InputKeyIconSettings, TEXT("ActionSlotWidgetSettings.InputKeyIconSettings"));
	ValidateMapSettings(Context, Result, GetMapWidgetSettings());

	return Result;
}
#endif

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

	const FGameplayTag PlayerHudTag = UIClasses && UIClasses->PlayerHudWidgetTag.IsValid()
		? UIClasses->PlayerHudWidgetTag
		: PlayerHudWidgetSettings.WidgetTag;
	if (PlayerHudTag.MatchesTagExact(WidgetTag))
	{
		return GetPlayerHudWidgetClass();
	}

	const FGameplayTag InfoTag = UIClasses && UIClasses->InfoWidgetTag.IsValid()
		? UIClasses->InfoWidgetTag
		: InfoWidgetSettings.WidgetTag;
	if (InfoTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetInfoWidgetClass().Get());
	}

	const FGameplayTag SelectPandoraTag = UIClasses && UIClasses->SelectPandoraWidgetTag.IsValid()
		? UIClasses->SelectPandoraWidgetTag
		: SelectPandoraWidgetSettings.WidgetTag;
	if (SelectPandoraTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetSelectPandoraWidgetClass().Get());
	}

	const FGameplayTag AimCrosshairTag = UIClasses && UIClasses->AimCrosshairWidgetTag.IsValid()
		? UIClasses->AimCrosshairWidgetTag
		: AimCrosshairWidgetSettings.WidgetTag;
	if (AimCrosshairTag.MatchesTagExact(WidgetTag))
	{
		return GetAimCrosshairWidgetClass();
	}

	const FGameplayTag PandoraTreeTag = UIClasses && UIClasses->PandoraTreeWidgetTag.IsValid()
		? UIClasses->PandoraTreeWidgetTag
		: PandoraTreeWidgetSettings.WidgetTag;
	if (PandoraTreeTag.MatchesTagExact(WidgetTag))
	{
		return TSubclassOf<UUserWidget>(GetPandoraTreeWidgetClass().Get());
	}

	return nullptr;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetPlayerHudWidgetClass() const
{
	if (UIClasses && UIClasses->PlayerHudWidgetClass)
	{
		return UIClasses->PlayerHudWidgetClass;
	}
	return PlayerHudWidgetSettings.WidgetClass;
}

TSubclassOf<UInfoWidget> UWidgetClassDefinition::GetInfoWidgetClass() const
{
	if (UIClasses && UIClasses->InfoWidgetClass)
	{
		return UIClasses->InfoWidgetClass;
	}
	return InfoWidgetSettings.WidgetClass;
}

TSubclassOf<USelectPandoraWidget> UWidgetClassDefinition::GetSelectPandoraWidgetClass() const
{
	if (UIClasses && UIClasses->SelectPandoraWidgetClass)
	{
		return UIClasses->SelectPandoraWidgetClass;
	}
	return SelectPandoraWidgetSettings.WidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetAimCrosshairWidgetClass() const
{
	if (UIClasses && UIClasses->AimCrosshairWidgetClass)
	{
		return UIClasses->AimCrosshairWidgetClass;
	}
	return AimCrosshairWidgetSettings.WidgetClass;
}

TSubclassOf<UPandoraTreeWidget> UWidgetClassDefinition::GetPandoraTreeWidgetClass() const
{
	if (UIClasses && UIClasses->PandoraTreeWidgetClass)
	{
		return UIClasses->PandoraTreeWidgetClass;
	}
	return PandoraTreeWidgetSettings.WidgetClass;
}

TSubclassOf<URightNotificationsWidget> UWidgetClassDefinition::GetRightNotificationsWidgetClass() const
{
	if (UIClasses && UIClasses->RightNotificationsWidgetClass)
	{
		return UIClasses->RightNotificationsWidgetClass;
	}
	return RightNotificationsWidgetSettings.WidgetClass;
}

TSubclassOf<UMenuPopupWidget> UWidgetClassDefinition::GetMenuPopupWidgetClass() const
{
	if (UIClasses && UIClasses->MenuPopupWidgetClass)
	{
		return UIClasses->MenuPopupWidgetClass;
	}
	return MenuPopupWidgetSettings.MenuPopupWidgetClass;
}

TSubclassOf<UTrainingRoomMenuPopupWidget> UWidgetClassDefinition::GetTrainingRoomMenuPopupWidgetClass() const
{
	if (UIClasses && UIClasses->TrainingRoomMenuPopupWidgetClass)
	{
		return UIClasses->TrainingRoomMenuPopupWidgetClass;
	}
	return MenuPopupWidgetSettings.TrainingRoomMenuPopupWidgetClass;
}

TSubclassOf<UConnectingPopupWidget> UWidgetClassDefinition::GetConnectingPopupWidgetClass() const
{
	if (UIClasses && UIClasses->ConnectingPopupWidgetClass)
	{
		return UIClasses->ConnectingPopupWidgetClass;
	}
	return ConnectingPopupWidgetSettings.WidgetClass;
}

TSubclassOf<UShopWidget> UWidgetClassDefinition::GetShopWidgetClass() const
{
	if (UIClasses && UIClasses->ShopWidgetClass)
	{
		return UIClasses->ShopWidgetClass;
	}
	return TitleAuxiliaryWidgetSettings.ShopWidgetClass;
}

TSubclassOf<UGuideWidget> UWidgetClassDefinition::GetGuideWidgetClass() const
{
	if (UIClasses && UIClasses->GuideWidgetClass)
	{
		return UIClasses->GuideWidgetClass;
	}
	return TitleAuxiliaryWidgetSettings.GuideWidgetClass;
}

TSubclassOf<URecordWidget> UWidgetClassDefinition::GetRecordWidgetClass() const
{
	if (UIClasses && UIClasses->RecordWidgetClass)
	{
		return UIClasses->RecordWidgetClass;
	}
	return TitleAuxiliaryWidgetSettings.RecordWidgetClass;
}

TSubclassOf<UMapWidget> UWidgetClassDefinition::GetTotalMapWidgetClass() const
{
	if (UIClasses && UIClasses->TotalMapWidgetClass)
	{
		return UIClasses->TotalMapWidgetClass;
	}
	return InfoAuxiliaryWidgetSettings.TotalMapWidgetClass;
}

TSubclassOf<AActor> UWidgetClassDefinition::GetCharacterPreviewClass() const
{
	if (UIClasses && UIClasses->CharacterPreviewClass)
	{
		return UIClasses->CharacterPreviewClass;
	}
	return InfoAuxiliaryWidgetSettings.CharacterPreviewClass;
}

TSubclassOf<UPandoraDescriptionWidget> UWidgetClassDefinition::GetPandoraDescriptionWidgetClass() const
{
	if (UIClasses && UIClasses->PandoraDescriptionWidgetClass)
	{
		return UIClasses->PandoraDescriptionWidgetClass;
	}
	return InfoAuxiliaryWidgetSettings.PandoraDescriptionWidgetClass;
}

TSubclassOf<URecordEntryWidget> UWidgetClassDefinition::GetRecordEntryWidgetClass() const
{
	if (UIClasses && UIClasses->RecordEntryWidgetClass)
	{
		return UIClasses->RecordEntryWidgetClass;
	}
	return RecordWidgetSettings.RecordEntryWidgetClass;
}

TSubclassOf<UStatusEffectWidget> UWidgetClassDefinition::GetStatusEffectWidgetClass() const
{
	if (UIClasses && UIClasses->StatusEffectWidgetClass)
	{
		return UIClasses->StatusEffectWidgetClass;
	}
	return StatusEffectsBarWidgetSettings.StatusEffectWidgetClass;
}

TSubclassOf<UChatEntryWidget> UWidgetClassDefinition::GetChatEntryWidgetClass() const
{
	if (UIClasses && UIClasses->ChatEntryWidgetClass)
	{
		return UIClasses->ChatEntryWidgetClass;
	}
	return ChatWidgetSettings.ChatEntryWidgetClass;
}

TSubclassOf<URoomItemWidget> UWidgetClassDefinition::GetRoomItemWidgetClass() const
{
	if (UIClasses && UIClasses->RoomItemWidgetClass)
	{
		return UIClasses->RoomItemWidgetClass;
	}
	return RoomListWidgetSettings.RoomItemWidgetClass;
}

TSubclassOf<UCreateRoomPopupWidget> UWidgetClassDefinition::GetCreateRoomPopupWidgetClass() const
{
	if (UIClasses && UIClasses->CreateRoomPopupWidgetClass)
	{
		return UIClasses->CreateRoomPopupWidgetClass;
	}
	return RoomListWidgetSettings.CreateRoomPopupWidgetClass;
}

TSubclassOf<ULobbyUserWidget> UWidgetClassDefinition::GetLobbyUserWidgetClass() const
{
	if (UIClasses && UIClasses->LobbyUserWidgetClass)
	{
		return UIClasses->LobbyUserWidgetClass;
	}
	return LobbyWidgetSettings.LobbyUserWidgetClass;
}

TSubclassOf<UGameConfigWidget> UWidgetClassDefinition::GetGameConfigWidgetClass() const
{
	if (UIClasses && UIClasses->GameConfigWidgetClass)
	{
		return UIClasses->GameConfigWidgetClass;
	}
	return LobbyWidgetSettings.GameConfigWidgetClass;
}

TSubclassOf<UGameResultWidget> UWidgetClassDefinition::GetGameResultWidgetClass() const
{
	if (UIClasses && UIClasses->GameResultWidgetClass)
	{
		return UIClasses->GameResultWidgetClass;
	}
	return GameResultWidgetSettings.GameResultWidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetHealthBarWidgetClass() const
{
	if (UIClasses && UIClasses->HealthBarWidgetClass)
	{
		return UIClasses->HealthBarWidgetClass;
	}
	return CharacterWidgetSettings.HealthBarWidgetClass;
}

TSubclassOf<UUserWidget> UWidgetClassDefinition::GetEnemyAvatarWidgetClass() const
{
	if (UIClasses && UIClasses->EnemyAvatarWidgetClass)
	{
		return UIClasses->EnemyAvatarWidgetClass;
	}
	return CharacterWidgetSettings.EnemyAvatarWidgetClass;
}

TSubclassOf<UInfoUiPresenter> UWidgetClassDefinition::GetInfoPresenterClass() const
{
	if (UIClasses && UIClasses->InfoPresenterClass)
	{
		return UIClasses->InfoPresenterClass;
	}
	return InfoWidgetSettings.PresenterClass;
}

TSubclassOf<UNotificationEntryWidget> UWidgetClassDefinition::GetNotificationEntryWidgetClass() const
{
	if (UIClasses && UIClasses->NotificationEntryWidgetClass)
	{
		return UIClasses->NotificationEntryWidgetClass;
	}
	return RightNotificationsWidgetSettings.NotificationEntryWidgetClass;
}

TSoftClassPtr<UKillBoxWidget> UWidgetClassDefinition::GetKillBoxEntryWidgetClass() const
{
	if (UIClasses && !UIClasses->KillBoxEntryWidgetClass.IsNull())
	{
		return UIClasses->KillBoxEntryWidgetClass;
	}
	return KillBoxWidgetSettings.EntryWidgetClass;
}

TSoftClassPtr<UQuickSlotEntryWidget> UWidgetClassDefinition::GetQuickSlotEntryWidgetClass() const
{
	if (UIClasses && !UIClasses->QuickSlotEntryWidgetClass.IsNull())
	{
		return UIClasses->QuickSlotEntryWidgetClass;
	}
	return QuickSlotWidgetSettings.EntryWidgetClass;
}

TSoftClassPtr<UActionSlotEntryWidget> UWidgetClassDefinition::GetActionSlotEntryWidgetClass() const
{
	if (UIClasses && !UIClasses->ActionSlotEntryWidgetClass.IsNull())
	{
		return UIClasses->ActionSlotEntryWidgetClass;
	}
	return ActionSlotWidgetSettings.EntryWidgetClass;
}

TSoftObjectPtr<UInputAction> UWidgetClassDefinition::GetTogglePandoraTreeInputAction() const
{
	if (InputIcons)
	{
		return InputIcons->TogglePandoraTreeInputAction;
	}

	const UWidgetInputIconsDefinition* InputDefaults = GetDefault<UWidgetInputIconsDefinition>();
	return InputDefaults
		? InputDefaults->TogglePandoraTreeInputAction
		: TSoftObjectPtr<UInputAction>();
}

const TArray<FName>& UWidgetClassDefinition::GetTrainingRoomMapNames() const
{
	if (MapUI)
	{
		return MapUI->TrainingRoomMapNames;
	}
	return MenuPopupWidgetSettings.TrainingRoomMapNames;
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
	return Style ? Style->KillBoxWidgetSettings : KillBoxWidgetSettings;
}

const FInfoWidgetSettings& UWidgetClassDefinition::GetInfoWidgetSettings() const
{
	return Style ? Style->InfoWidgetSettings : InfoWidgetSettings;
}

const FSelectPandoraWidgetSettings& UWidgetClassDefinition::GetSelectPandoraWidgetSettings() const
{
	return Style ? Style->SelectPandoraWidgetSettings : SelectPandoraWidgetSettings;
}

const FPandoraTreeWidgetSettings& UWidgetClassDefinition::GetPandoraTreeWidgetSettings() const
{
	return Style ? Style->PandoraTreeWidgetSettings : PandoraTreeWidgetSettings;
}

const FPandoraWidgetSettings& UWidgetClassDefinition::GetPandoraWidgetSettings() const
{
	return Style ? Style->PandoraWidgetSettings : PandoraWidgetSettings;
}

const FRightPandoraWidgetSettings& UWidgetClassDefinition::GetRightPandoraWidgetSettings() const
{
	return Style ? Style->RightPandoraWidgetSettings : RightPandoraWidgetSettings;
}

const FRightNotificationsWidgetSettings& UWidgetClassDefinition::GetRightNotificationsWidgetSettings() const
{
	return Style ? Style->RightNotificationsWidgetSettings : RightNotificationsWidgetSettings;
}

const FTitleAuxiliaryWidgetSettings& UWidgetClassDefinition::GetTitleAuxiliaryWidgetSettings() const
{
	return Style ? Style->TitleAuxiliaryWidgetSettings : TitleAuxiliaryWidgetSettings;
}

const FMapWidgetSettings& UWidgetClassDefinition::GetMapWidgetSettings() const
{
	return MapUI ? MapUI->MapWidgetSettings : MapWidgetSettings;
}

const FRecordWidgetSettings& UWidgetClassDefinition::GetRecordWidgetSettings() const
{
	return Style ? Style->RecordWidgetSettings : RecordWidgetSettings;
}

const FStatusEffectsBarWidgetSettings& UWidgetClassDefinition::GetStatusEffectsBarWidgetSettings() const
{
	return Style ? Style->StatusEffectsBarWidgetSettings : StatusEffectsBarWidgetSettings;
}

const FChatWidgetSettings& UWidgetClassDefinition::GetChatWidgetSettings() const
{
	return Style ? Style->ChatWidgetSettings : ChatWidgetSettings;
}

const FRoomListWidgetSettings& UWidgetClassDefinition::GetRoomListWidgetSettings() const
{
	return Style ? Style->RoomListWidgetSettings : RoomListWidgetSettings;
}

const FLobbyWidgetSettings& UWidgetClassDefinition::GetLobbyWidgetSettings() const
{
	return Style ? Style->LobbyWidgetSettings : LobbyWidgetSettings;
}

const FInventoryWidgetSettings& UWidgetClassDefinition::GetInventoryWidgetSettings() const
{
	return Style ? Style->InventoryWidgetSettings : InventoryWidgetSettings;
}

const FSkinWidgetSettings& UWidgetClassDefinition::GetSkinWidgetSettings() const
{
	return Style ? Style->SkinWidgetSettings : SkinWidgetSettings;
}

const FAbilitySlotWidgetSettings& UWidgetClassDefinition::GetAbilitySlotWidgetSettings() const
{
	return InputIcons ? InputIcons->AbilitySlotWidgetSettings : AbilitySlotWidgetSettings;
}

const FSkillTipWidgetSettings& UWidgetClassDefinition::GetSkillTipEffectIconSettings() const
{
	return InputIcons ? InputIcons->SkillTipEffectIconSettings : SkillTipWidgetSettings;
}

const FSkillTipWidgetSettings& UWidgetClassDefinition::GetPandoraDescriptionEffectIconSettings() const
{
	return InputIcons
		? InputIcons->PandoraDescriptionEffectIconSettings
		: PandoraDescriptionEffectIconSettings;
}

const FQuickSlotWidgetSettings& UWidgetClassDefinition::GetQuickSlotWidgetSettings() const
{
	return InputIcons ? InputIcons->QuickSlotWidgetSettings : QuickSlotWidgetSettings;
}

const FActionSlotWidgetSettings& UWidgetClassDefinition::GetActionSlotWidgetSettings() const
{
	return InputIcons ? InputIcons->ActionSlotWidgetSettings : ActionSlotWidgetSettings;
}
