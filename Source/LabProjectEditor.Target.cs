// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class LabProjectEditorTarget : TargetRules
{
	public LabProjectEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// GameLift 플러그인 안내: 수정된 파일만 따로 컴파일하는 적응형 유니티 빌드에서 플러그인 소스가 충돌한다.
		bUseAdaptiveUnityBuild = false;

		ExtraModuleNames.AddRange( new string[] { "LabProject" } );
	}
}
