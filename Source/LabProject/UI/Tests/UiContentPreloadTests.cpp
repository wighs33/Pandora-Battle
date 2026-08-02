#if WITH_DEV_AUTOMATION_TESTS

#include "Definition/Player/CharacterActionDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdUiContentPreloadPathCollectionTest,
	"LabProject.UI.ContentPreload.PathCollection",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ClientContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::EngineFilter)

bool FPdUiContentPreloadPathCollectionTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const UWidgetClassDefinition* ConfiguredWidgetDefinition =
		LoadObject<UWidgetClassDefinition>(
			nullptr,
			TEXT("/Game/UI/DA_Widget.DA_Widget"));
	TestNotNull(
		TEXT("Configured DA_Widget is available before quick match."),
		ConfiguredWidgetDefinition);
	if (ConfiguredWidgetDefinition)
	{
		TestNotNull(
			TEXT("DA_Widget resolves the quick-match loading screen class."),
			ConfiguredWidgetDefinition->GetConnectingPopupWidgetClass().Get());
	}

	TestNotNull(
		TEXT("Configured DA_Setting can be loaded for lobby entry."),
		LoadObject<UGameSettingDefinition>(
			nullptr,
			TEXT("/Game/Data/DA_Setting.DA_Setting")));

	const UWidgetClassDefinition* WidgetDefinition =
		NewObject<UWidgetClassDefinition>(GetTransientPackage());
	TestNotNull(TEXT("A transient widget definition can be created."), WidgetDefinition);
	if (!WidgetDefinition)
	{
		return false;
	}

	TArray<FSoftObjectPath> AssetPaths;
	WidgetDefinition->GetRuntimePreloadAssetPaths(AssetPaths);
	TestTrue(TEXT("The default UI composition exposes preloadable assets."), !AssetPaths.IsEmpty());

	const TSet<FSoftObjectPath> UniquePaths(AssetPaths);
	TestEqual(
		TEXT("The preload list contains no duplicate paths."),
		AssetPaths.Num(),
		UniquePaths.Num());

	for (int32 PathIndex = 1; PathIndex < AssetPaths.Num(); ++PathIndex)
	{
		TestTrue(
			TEXT("The preload list has deterministic lexical ordering."),
			AssetPaths[PathIndex - 1].ToString() < AssetPaths[PathIndex].ToString());
	}

	TestTrue(
		TEXT("Input-action assets from nested UI settings are collected."),
		UniquePaths.Contains(
			FSoftObjectPath(TEXT("/Game/Input/Action/IA_Skill1.IA_Skill1"))));
	TestTrue(
		TEXT("Map textures from nested UI settings are collected."),
		UniquePaths.Contains(
			FSoftObjectPath(TEXT("/Game/Map/Map.Map"))));

	const UCharacterActionDefinition* ActionDefinition =
		NewObject<UCharacterActionDefinition>(GetTransientPackage());
	TestNotNull(
		TEXT("A transient character-action definition can be created."),
		ActionDefinition);
	if (ActionDefinition)
	{
		TArray<FSoftObjectPath> ActionAssetPaths;
		ActionDefinition->GetRuntimePreloadAssetPaths(ActionAssetPaths);
		const TSet<FSoftObjectPath> UniqueActionPaths(ActionAssetPaths);

		TestTrue(
			TEXT("The weapon-swap input action is exposed for async preload."),
			UniqueActionPaths.Contains(
				FSoftObjectPath(
					TEXT("/Game/Input/Action/IA_SelectPandora.IA_SelectPandora"))));
		TestTrue(
			TEXT("The grapple input action is exposed for async preload."),
			UniqueActionPaths.Contains(
				FSoftObjectPath(
					TEXT("/Game/Input/Action/IA_Grapple.IA_Grapple"))));
	}

	const UGameSettingDefinition* GameSettingDefinition =
		NewObject<UGameSettingDefinition>(GetTransientPackage());
	TestNotNull(
		TEXT("A transient game-setting definition can be created."),
		GameSettingDefinition);
	if (GameSettingDefinition)
	{
		TestTrue(
			TEXT("Game settings use the registered GameSetting primary-asset type."),
			GameSettingDefinition->GetPrimaryAssetId().PrimaryAssetType
				== FPrimaryAssetType(TEXT("GameSetting")));

		TArray<FSoftObjectPath> GameSettingAssetPaths;
		GameSettingDefinition->GetRuntimePreloadAssetPaths(
			GameSettingAssetPaths);
		const TSet<FSoftObjectPath> UniqueGameSettingPaths(
			GameSettingAssetPaths);
		TestEqual(
			TEXT("The game-setting preload list contains no duplicate paths."),
			GameSettingAssetPaths.Num(),
			UniqueGameSettingPaths.Num());
		for (int32 PathIndex = 1;
			PathIndex < GameSettingAssetPaths.Num();
			++PathIndex)
		{
			TestTrue(
				TEXT("The game-setting preload list has deterministic lexical ordering."),
				GameSettingAssetPaths[PathIndex - 1].ToString()
					< GameSettingAssetPaths[PathIndex].ToString());
		}
		TestTrue(
			TEXT("The achievement definition is exposed for async preload."),
			UniqueGameSettingPaths.Contains(
				FSoftObjectPath(
					TEXT("/Game/Data/DA_Achievement.DA_Achievement"))));
		TestTrue(
			TEXT("Status-effect definitions are exposed for async preload."),
			UniqueGameSettingPaths.Contains(
				FSoftObjectPath(
					TEXT("/Game/StatusEffects/DA_StatusEffect_Burn.DA_StatusEffect_Burn"))));

#if WITH_EDITOR
		const auto TestClientBundleProperty =
			[this](const FName PropertyName)
			{
				const FProperty* Property =
					FindFProperty<FProperty>(
						UGameSettingDefinition::StaticClass(),
						PropertyName);
				TestNotNull(
					*FString::Printf(
						TEXT("%s is a reflected game-setting property."),
						*PropertyName.ToString()),
					Property);
				if (Property)
				{
					TestTrue(
						*FString::Printf(
							TEXT("%s is included in the Client runtime bundle."),
							*PropertyName.ToString()),
						Property->GetMetaData(TEXT("AssetBundles"))
							.Contains(TEXT("Client")));
				}
			};

		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				MouseCursorTexture));
		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				SoundEnabledButtonTexture));
		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				SoundMutedButtonTexture));
		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				StatusEffectDataAssets));
		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				StartupBgm));
		TestClientBundleProperty(
			GET_MEMBER_NAME_CHECKED(
				UGameSettingDefinition,
				GameplayBgm));
#endif
	}

	return true;
}

#endif
