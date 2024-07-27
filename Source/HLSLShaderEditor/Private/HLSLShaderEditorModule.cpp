// Copyright Phyronnaz
#pragma once

#include "HLSLShaderEditorModule.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "HLSLMaterialSettings.h"
#include "AssetTypeActions/AssetTypeActions_HLSLShaderLibrary.h"

#define LOCTEXT_NAMESPACE "FHLSLShaderEditorModule"

void FHLSLShaderEditorModule::StartupModule()
{
	HLSLAssetTypeActions = MakeShared<FAssetTypeActions_HLSLShaderLibrary>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(HLSLAssetTypeActions.ToSharedRef());
		
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.RegisterSettings(
		"Editor",
		"Plugins",
		"HLSL Material",
		INVTEXT("HLSL Material"),
		INVTEXT("Settings related to the HLSL Material plugin."),
		GetMutableDefault<UHLSLMaterialSettings>());

	ToolbarExtensibilityManager = MakeShareable(new FExtensibilityManager);
}

void FHLSLShaderEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	if(!FModuleManager::Get().IsModuleLoaded("AssetTools")) return;
	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(HLSLAssetTypeActions.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FHLSLShaderEditorModule, HLSLShaderEditor);