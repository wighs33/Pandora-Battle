using UnrealBuildTool;

public class LabProjectEditor : ModuleRules
{
	public LabProjectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"StateTreeEditorModule"
		});
	}
}
