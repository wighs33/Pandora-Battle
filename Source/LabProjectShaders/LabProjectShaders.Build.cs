using UnrealBuildTool;

// 저장소의 Shaders 폴더를 머티리얼 Custom 노드에서 include할 수 있게 가상 경로로 연결하는 모듈.
public class LabProjectShaders : ModuleRules
{
	public LabProjectShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore" });
	}
}
