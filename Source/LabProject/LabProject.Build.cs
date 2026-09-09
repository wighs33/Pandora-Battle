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
			"RenderCore",
			"AssetRegistry",
			"CableComponent",
			"Slate",
			"NavigationSystem"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"EngineSettings",
				"UnrealEd"
			});
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");

	}
}
