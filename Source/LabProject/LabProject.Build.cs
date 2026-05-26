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
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"NetCore",
			"IrisCore",
			"ModularGameplay", 
			"GameFeatures",
			"GameplayTags", 
			"GameplayAbilities",
			"GameplayTasks",
			"StructUtils",
			"ModelViewViewModel",
			"UMG",
			"AnimGraphRuntime",
			"Niagara",
			"AssetRegistry"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore", "AIModule"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
