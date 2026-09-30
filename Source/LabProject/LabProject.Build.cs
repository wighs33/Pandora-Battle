using System.IO;
using System.Linq;
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
			"CommonUI",
			"CommonInput",
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
			"NavigationSystem",
			"HTTP",
			"Json"
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

		// GameLift 서버 SDK는 플러그인이 설치된 프로젝트의 Server target에서만 사용한다(플러그인 모듈이 Server 전용).
		// 설치 전이나 Editor·Game target에서는 PD_WITH_GAMELIFT=0으로 빌드되어 GameLift 호출이 모두 빠진다.
		bool bWithGameLift = Target.Type == TargetType.Server && HasGameLiftServerSdk(Target);
		if (bWithGameLift)
		{
			PrivateDependencyModuleNames.Add("GameLiftServerSDK");
			bEnableExceptions = true;
		}
		PrivateDefinitions.Add("PD_WITH_GAMELIFT=" + (bWithGameLift ? "1" : "0"));
	}

	private static bool HasGameLiftServerSdk(ReadOnlyTargetRules Target)
	{
		if (Target.ProjectFile == null)
		{
			return false;
		}

		string PluginsDirectory = Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins");
		return Directory.Exists(PluginsDirectory)
			&& Directory.EnumerateFiles(PluginsDirectory, "GameLiftServerSDK.Build.cs", SearchOption.AllDirectories).Any();
	}
}
