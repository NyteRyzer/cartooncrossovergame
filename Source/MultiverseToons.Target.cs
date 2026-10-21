using UnrealBuildTool;

public class MultiverseToonsTarget : TargetRules
{
	public MultiverseToonsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("MultiverseToons");
	}
}
