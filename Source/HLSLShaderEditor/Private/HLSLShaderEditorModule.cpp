// Copyright Phyronnaz

#include "CoreMinimal.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "Modules/ModuleInterface.h"
#include "HLSLMaterialSettings.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLShaderLibrary.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

class FAssetTypeActions_HLSLShaderLibrary : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_HLSLShaderLibrary() = default;

	//~ Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return INVTEXT("HLSL Shader Library"); }
	virtual uint32 GetCategories() override
	{
#if ENGINE_VERSION >= 500
		return EAssetTypeCategories::Materials;
#else
		return EAssetTypeCategories::MaterialsAndTextures;
#endif
	}
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 255); }
	virtual UClass* GetSupportedClass() const override { return UHLSLShaderLibrary::StaticClass(); }

	virtual bool HasActions(const TArray<UObject*>&InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>&InObjects, FMenuBuilder& MenuBuilder) override
	{
		MenuBuilder.AddMenuEntry(
			INVTEXT("Update from HLSL"),
			INVTEXT("Update all the generated material functions from the HLSL code"),
			{},
			FUIAction(FExecuteAction::CreateLambda([this, Assets = GetTypedWeakObjectPtrs<UHLSLShaderLibrary>(InObjects)]()
		{
			for (const TWeakObjectPtr<UHLSLShaderLibrary>& Asset : Assets)
			{
				if (ensure(Asset.IsValid()))
				{
					IHLSLShaderEditorInterface::Get()->Update(*Asset);
				}
			}
		})));
	}
	//~ End IAssetTypeActions Interface
};

class FHLSLShaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_HLSLShaderLibrary>());
		
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings(
			"Editor",
			"Plugins",
			"HLSL Material",
			INVTEXT("HLSL Material"),
			INVTEXT("Settings related to the HLSL Material plugin."),
			GetMutableDefault<UHLSLMaterialSettings>());
	}
};
IMPLEMENT_MODULE(FHLSLShaderEditorModule, HLSLShaderEditor);