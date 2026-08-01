// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class LabProjectTarget : TargetRules
{
	public LabProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;

		if (Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			// OnlineSubsystemSteam does not read SteamDevAppId for Shipping builds.
			// It uses this compile-time value when validating/relaunching through Steam.
			bOverrideBuildEnvironment = true;
			GlobalDefinitions.Add("UE_PROJECT_STEAMSHIPPINGID=4972140");
		}

		ExtraModuleNames.AddRange( new string[] { "LabProject" } );
	}
}
