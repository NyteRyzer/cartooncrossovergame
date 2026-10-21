using UnrealBuildTool;

public class MultiverseToons : ModuleRules
{
	public MultiverseToons(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",
			"ToonRender"
		});

		PublicIncludePaths.AddRange(new string[]
		{
			ModuleDirectory + "/Public"
		});
	}
}
