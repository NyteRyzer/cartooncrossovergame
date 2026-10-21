using UnrealBuildTool;

// Loads at PostConfigInit (before shaders compile) to register the virtual
// shader directory. Must not contain UObject classes.
public class ToonRenderShaders : ModuleRules
{
	public ToonRenderShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "RenderCore" });
	}
}
