// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class LabProjectTarget : TargetRules
{
	public LabProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// The engine compiles Push Model into editor builds only. Built from the source engine,
		// the game gets its own build environment so the engine modules compile with it too.
		BuildEnvironment = TargetBuildEnvironment.Unique;
		bWithPushModel = true;

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			// OnlineSubsystemSteam does not read SteamDevAppId for Shipping builds.
			// It uses this compile-time value when validating/relaunching through Steam.
			GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=4972140");
		}

		ExtraModuleNames.AddRange( new string[] { "LabProject" } );
	}
}
