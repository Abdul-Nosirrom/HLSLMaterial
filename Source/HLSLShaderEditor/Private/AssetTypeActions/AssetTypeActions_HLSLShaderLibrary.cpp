// Copyright 2023 CoC All rights reserved


#include "AssetTypeActions_HLSLShaderLibrary.h"

#include "HLSLShaderLibrary.h"
#include "AssetEditor/HLSLShaderMaterialEditor.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ShaderGeneration/HLSLShaderLibraryEditor.h"

UClass* FAssetTypeActions_HLSLShaderLibrary::GetSupportedClass() const
{
	return UHLSLShaderLibrary::StaticClass();
}


void FAssetTypeActions_HLSLShaderLibrary::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
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

	MenuBuilder.AddMenuEntry(
		INVTEXT("Create Material Instance"),
		INVTEXT("Creates a material instance from the generated material of the HLSL code"),
		{},
		FUIAction(FExecuteAction::CreateLambda([this, Assets = GetTypedWeakObjectPtrs<UHLSLShaderLibrary>(InObjects)]
		{
			for (auto Asset : Assets)
			{
				if (Asset.IsValid())
				{
					FHLSLShaderLibraryEditor::GenerateMaterialInstanceForShader(*Asset);
				}
			}
		})));
}


void FAssetTypeActions_HLSLShaderLibrary::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode =
		EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UHLSLShaderLibrary* PoseSearchDb = Cast<UHLSLShaderLibrary>(*ObjIt))
		{
			//const TSharedRef<FHLSLEditorToolkit> NewEditor(new FHLSLEditorToolkit());
			//NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, PoseSearchDb);
			const TSharedRef<FHLSLShaderMaterialEditor> NewEditor(new FHLSLShaderMaterialEditor());
			NewEditor->InitShaderEditor(Mode, EditWithinLevelEditor, PoseSearchDb);
			//NewEditor->Ini

		}
	}}
