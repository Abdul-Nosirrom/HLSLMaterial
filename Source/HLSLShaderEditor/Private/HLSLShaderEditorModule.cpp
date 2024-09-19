// Copyright Phyronnaz
#pragma once

#include "HLSLShaderEditorModule.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "HLSLMaterialSettings.h"
#include "AssetTypeActions/AssetTypeActions_HLSLShaderLibrary.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderGeneration/HLSLShaderGenerator.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "FHLSLShaderEditorModule"
#define SLATE_IMAGE_BRUSH( ImagePath, ImageSize ) new FSlateImageBrush( StyleSetInstance->RootToContentDir( TEXT(ImagePath), TEXT(".png") ), FVector2D(ImageSize, ImageSize ) )

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

	// Setup asset icons
	{
		// Create the new style set
		StyleSetInstance = MakeShareable(new FSlateStyleSet("HLSLShaderEditorStyle"));
		// Set root directory as resources folder
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("HLSLMaterial"))->GetBaseDir() / TEXT("Resources");
		StyleSetInstance->SetContentRoot(ContentDir);

		StyleSetInstance->Set("ClassIcon.HLSLShaderLibrary", SLATE_IMAGE_BRUSH("ShaderAssetIcon", 64.f));
		StyleSetInstance->Set("ClassThumbnail.HLSLShaderLibrary", SLATE_IMAGE_BRUSH("ShaderAssetIcon", 256.f));

		// Register the style set
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSetInstance);
	}

	// Initialize generator structures
	FHLSLShaderGenerator::Initialize();
}

void FHLSLShaderEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	if(!FModuleManager::Get().IsModuleLoaded("AssetTools")) return;
	FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(HLSLAssetTypeActions.ToSharedRef());

	// Unregister style set
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSetInstance.Get());
		StyleSetInstance.Reset();
	}
}

#undef SLATE_IMAGE_BRUSH
#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FHLSLShaderEditorModule, HLSLShaderEditor);