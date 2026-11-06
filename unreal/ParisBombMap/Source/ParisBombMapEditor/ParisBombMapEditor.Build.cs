using UnrealBuildTool;

public class ParisBombMapEditor : ModuleRules
{
	public ParisBombMapEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"NavigationSystem",
			"ParisBombMap"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
