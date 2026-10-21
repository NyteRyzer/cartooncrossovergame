using UnrealBuildTool;

public class MultiverseToonsEditorTarget : TargetRules
{
	public MultiverseToonsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("MultiverseToons");
	}
}
