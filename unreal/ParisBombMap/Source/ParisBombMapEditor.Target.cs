using UnrealBuildTool;
using System.Collections.Generic;

public class ParisBombMapEditorTarget : TargetRules
{
	public ParisBombMapEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("ParisBombMap");
		ExtraModuleNames.Add("ParisBombMapEditor");
	}
}
