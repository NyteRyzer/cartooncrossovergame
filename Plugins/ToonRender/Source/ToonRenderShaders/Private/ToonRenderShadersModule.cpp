#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

// Maps Plugins/ToonRender/Shaders to the virtual path /Plugin/ToonRender so
// material Custom nodes can #include "/Plugin/ToonRender/Private/ToonCommon.ush".
class FToonRenderShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ToonRender"));
		if (Plugin.IsValid())
		{
			const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/ToonRender"), ShaderDir);
		}
	}
};

IMPLEMENT_MODULE(FToonRenderShadersModule, ToonRenderShaders)
