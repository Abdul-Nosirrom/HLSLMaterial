
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FHLSLShaderRuntimeModule : public IModuleInterface
{
};
IMPLEMENT_MODULE(FHLSLShaderRuntimeModule, HLSLMaterialRuntime);