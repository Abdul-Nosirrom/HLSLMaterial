#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FAssetTypeActions_HLSLShaderLibrary;

class FHLSLShaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr<class FExtensibilityManager> GetEditorToolbarExtensibilityManager() { return ToolbarExtensibilityManager; }

private:
	TSharedPtr<FAssetTypeActions_HLSLShaderLibrary> HLSLAssetTypeActions;
	TSharedPtr<FExtensibilityManager> ToolbarExtensibilityManager;

};
