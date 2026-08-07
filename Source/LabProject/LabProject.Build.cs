using UnrealBuildTool;

public class LabProject : ModuleRules
{
	public LabProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add(ModuleDirectory);

		SetupIrisSupport(Target);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			// Required by types inherited from or included by LabProject headers.
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"IrisCore",
			"ModularGameplay",
			"GameFeatures",
			"GameplayTags",
			"GameplayAbilities",
			"GameplayStateTreeModule",
			"ModelViewViewModel",
			"StateTreeModule",
			"UMG",
			"SlateCore",
			"AIModule",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Used only by LabProject implementation files or forward-declared APIs.
			"NetCore",
			"GameplayTasks",
			"AudioWidgets",
			"AnimGraphRuntime",
			"Niagara",
			"AssetRegistry",
			"CableComponent",
			"Slate",
			"NavigationSystem"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

		if (Target.Platform == UnrealTargetPlatform.Win64 &&
			Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			// Keep local packaged-build testing on the same Steam App ID as the
			// compile-time Shipping configuration. Steam-distributed builds are
			// still launched by the Steam client in the normal way.
			RuntimeDependencies.Add(
				"$(TargetOutputDir)/steam_appid.txt",
				"$(ProjectDir)/steam_appid.txt",
				StagedFileType.NonUFS);
		}

	}
}
