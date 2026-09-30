// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

// 전용 서버 빌드 target이다. 소스로 빌드한 엔진에서만 사용할 수 있으며,
// 일반 플레이는 계속 LabProject(Game) target의 Listen Server를 사용한다.
public class LabProjectServerTarget : TargetRules
{
	public LabProjectServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.AddRange( new string[] { "LabProject" } );
	}
}
