// Copyright Phyronnaz

#include "HLSLShaderLibraryEditor.h"

#include "AssetToolsModule.h"
#include "HLSLShader.h"
#include "HLSLShaderGenerator.h"
#include "HLSLShaderParser.h"
#include "HLSLMaterialUtilities.h"
#include "HLSLMaterialEditor/Private/HLSLMaterialFileWatcher.h"
#include "HLSLShaderMessages.h"
#include "HLSLShaderLibrary.h"
#include "IMaterialEditor.h"
#include "MaterialEditingLibrary.h"
#include "MaterialEditorActions.h"
#include "MaterialShared.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"

#include "Misc/FileHelper.h"
#include "AssetRegistry/AssetData.h"
#include "Materials/MaterialFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Editor/MaterialEditor/Private/MaterialEditor.h"
#include "Factories/MaterialFactoryNew.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialInstance.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"

class FHLSLShaderEditorInterfaceImpl : public IHLSLShaderEditorInterface
{
public:
	virtual TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLShaderLibrary& Library) override
	{
		return FHLSLShaderLibraryEditor::CreateWatcher(Library);
	}
	virtual void Update(UHLSLShaderLibrary& Library) override
	{
		FHLSLShaderLibraryEditor::Generate(Library);
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FHLSLShaderLibraryEditor::Register()
{
	IHLSLShaderEditorInterface::StaticInterface = new FHLSLShaderEditorInterfaceImpl();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.OnFilesLoaded().AddLambda([&AssetRegistry]
	{
		// Force load all libraries that have bGenerateOnFileChange, to start their watchers
		TArray<FAssetData> AssetDatas;
		FARFilter Filer;
		Filer.ClassPaths.Add(UHLSLShaderLibrary::StaticClass()->GetClassPathName());
		Filer.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UHLSLShaderLibrary, bUpdateOnFileChange), FString("true"));
		AssetRegistry.GetAssets(Filer, AssetDatas);

		for (const FAssetData& AssetData : AssetDatas)
		{
			ensure(AssetData.GetAsset());
		}
	});
}
HLSL_STARTUP_FUNCTION(EDelayedRegisterRunPhase::EndOfEngineInit, FHLSLShaderLibraryEditor::Register);

TSharedRef<FVirtualDestructor> FHLSLShaderLibraryEditor::CreateWatcher(UHLSLShaderLibrary& Library)
{
	FHLSLShaderMessages::FLibraryScope Scope(Library);

	const FString FullPath = Library.GetFilePath();

	// Create file watchers for main file as well as any includes by loading the string file and getting the include paths then creating a watcher for them
	TArray<FString> Files;
	Files.Add(FullPath);

	if (Library.bUpdateOnIncludeChange)
	{
		FString Text;
		if (TryLoadFileToString(Text, Library.GetFilePath()))
		{
			for (const FHLSLShaderParser::FInclude& Include : FHLSLShaderParser::GetIncludes(FullPath, Text))
			{
				if (!Include.DiskPath.IsEmpty())
				{
					Files.Add(Include.DiskPath);
				}
			}
		}
	}

	// Bind watcher to the generation of the material graph, so whenever a file change is saved, we regen the material
	const TSharedRef<FHLSLMaterialFileWatcher> Watcher = FHLSLMaterialFileWatcher::Create(Files);
	Watcher->OnFileChanged.AddWeakLambda(&Library, [&Library]
	{
		Generate(Library);
	});

	return Watcher;
}

void FHLSLShaderLibraryEditor::Generate(UHLSLShaderLibrary& Library)
{
	FHLSLShaderMessages::FLibraryScope Scope(Library);

	// Always recreate watcher in case includes changed
	Library.CreateWatcherIfNeeded();

	const FString FullPath = Library.GetFilePath();

	// Load the shader file as string
	FString Text;
	if (!TryLoadFileToString(Text, FullPath))
	{
		FHLSLShaderMessages::ShowError(TEXT("Failed to read %s"), *FullPath);
		return;
	}

	// Collect and validate all the #include "..."
	FString BaseHash;
	TArray<FString> IncludeFilePaths;
	for (const FHLSLShaderParser::FInclude& Include : FHLSLShaderParser::GetIncludes(FullPath, Text))
	{
		IncludeFilePaths.Add(Include.VirtualPath);

		FString IncludeText;
		if (TryLoadFileToString(IncludeText, Include.DiskPath))
		{
			BaseHash += FHLSLMaterialUtilities::HashString(IncludeText);
		}
		else
		{
			FHLSLShaderMessages::ShowError(TEXT("Invalid include: %s"), *Include.VirtualPath);
		}
	}

	// Collect and validate all the #define SETTING VALUE
	TArray<FHLSLShaderParser::FSetting> Settings = FHLSLShaderParser::GetSettings(Text); 
	for (const FHLSLShaderParser::FSetting& Setting : Settings)
	{
		BaseHash += FHLSLMaterialUtilities::HashString(Setting.Setting);
		BaseHash += FHLSLMaterialUtilities::HashString(Setting.Value);
	}

	// Collect and validate all the #pragma type name (functions & input/output structs declarations)
	TArray<FHLSLShaderParser::FPragmaDeclarations> PragmaDeclarations;
	TArray<FString> EncounteredPragmaTokens;
	for (const FHLSLShaderParser::FPragmaDeclarations& NameDefs : FHLSLShaderParser::GetPragmaDeclarations(Text))
	{
		if (EncounteredPragmaTokens.Contains(NameDefs.Type))
		{
			FHLSLShaderMessages::ShowError(TEXT("Encountered multiple Tokens: %s"), *NameDefs.Type);
			return;
		}
		else if (FHLSLMaterialShader::PRAGMA_DEFS.Contains(NameDefs.Type))
		{
			BaseHash += FHLSLMaterialUtilities::HashString(NameDefs.Type);
			BaseHash += FHLSLMaterialUtilities::HashString(NameDefs.Name);
		}
		else
		{
			FHLSLShaderMessages::ShowError(TEXT("Invalid Function/Struct Token: %s %s"), *NameDefs.Type, *NameDefs.Name);
			return;
		}

		PragmaDeclarations.Add(NameDefs);
		EncounteredPragmaTokens.Add(NameDefs.Type);
	}


	// Parse the function bodies & retrieve the structs and do some basic error checking + filling out each objects shader stage
	TArray<FHLSLMaterialShader> Shaders;
	TArray<FHLSLStruct> Structs;
	{
		const FString Error = FHLSLShaderParser::Parse(Library, Text, Shaders, Structs);
		if (!Error.IsEmpty())
		{
			FHLSLShaderMessages::ShowError(TEXT("Parsing failed: %s"), *Error);
			return;
		}

		if (Shaders.Num() > 3 || Structs.Num() > 4) // 3 input structs, 1 output max
		{
			FHLSLShaderMessages::ShowError(TEXT("Found more than 3 functions/6 structs, can only have functions corresponding to the 3 Shader Stage [Vertex/Normal/Pixel]"));
			return;
		}

		// Fill out the ShaderStage parameter in each shader & struct based on checking the function names against the pragma declarations, verifying they're correct
		int32 ExpectedStructCount = 0;
		{
			for (FHLSLMaterialShader& Shader : Shaders)
			{
				for (const FHLSLShaderParser::FPragmaDeclarations& Pragma : PragmaDeclarations)
				{
					// Remove whitespaces first
					Shader.Name.ReplaceInline(TEXT(" "), TEXT(""));
					if (Shader.Name.Equals(Pragma.Name))
					{
						Shader.ShaderStage = Pragma.Type;
						if (Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER) ExpectedStructCount += 2;
						else ExpectedStructCount++;
					}
				}

				if (Shader.ShaderStage.IsEmpty())
				{
					FHLSLShaderMessages::ShowError(TEXT("Illegal function not corresponding to a declared shader stage: %s"), *Shader.Name);
					return;
				}
			}

			for (FHLSLStruct& Struct : Structs)
			{
				Struct.Name.ReplaceInline(TEXT(" "), TEXT(""));
				Struct.Body.ReplaceInline(TEXT("\""), TEXT(""));
				for (const FHLSLShaderParser::FPragmaDeclarations& Pragma : PragmaDeclarations)
				{
					if (Struct.Name.Equals(Pragma.Name))
					{
						Struct.ShaderStage = Pragma.Type;
					}
				}

				if (Struct.ShaderStage.IsEmpty())
				{
					FHLSLShaderMessages::ShowError(TEXT("Illegal struct not corresponding to a declared shader stage: %s { \n %s \n };"), *Struct.Name, *Struct.Body);
					return;
				}
			}
		}

		// Number of structs should equal to some expected value depending on which shader is declared [1 input for normal/vertex and 1 input/1 output for pixel] 
		if (Structs.Num() != ExpectedStructCount)
		{
			FHLSLShaderMessages::ShowError(TEXT("Illegal functions/structs present. Pixel Shader requires 1 input & 1 output struct, vertex/normal require only 1 input struct [Structs: %d | Shaders: %d]"), Structs.Num(), Shaders.Num());
			return;
		}
	}

	// Create association between shaders & structs
	for (FHLSLMaterialShader& Shader : Shaders)
	{
		for (const FHLSLStruct& Struct : Structs)
		{
			if (Shader.ShaderStage.Equals(FHLSLMaterialShader::GetShaderStageFromInputOrOutput(Struct.ShaderStage)))
			{
				if (FHLSLMaterialShader::INPUT_NAMES.Contains(Struct.ShaderStage))			Shader.InputStruct_Raw = Struct;
				else if (FHLSLMaterialShader::OUTPUT_NAMES.Contains(Struct.ShaderStage))	Shader.OutputStruct_Raw = Struct;
			}
		}

		// Easy error check, make sure we have both of those structs filled out otherwise we have a problem
		{
			if ((Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER && Shader.OutputStruct_Raw.ShaderStage.IsEmpty()) || Shader.InputStruct_Raw.ShaderStage.IsEmpty())
			{
				FHLSLShaderMessages::ShowError(TEXT("Could not find associated input/output struct for: %s"), *Shader.Name);
				return;
			}
		}
	}
	
	// Verify that all functions return void or float3 depending on the stage and arguments match the pattern we're expecting and place structs in the appropriate function/shader
	for (FHLSLMaterialShader& Shader : Shaders)
	{
		// Vertex / Normals Shaders:
		// float3 FuncName(FMaterialPixel/VertexParameters Parameters, InputStruct struct); OR float3 FuncName(InputStruct struct);
		// Pixel Shaders
		// void FuncName(FMaterialPixel/VertexParameters Parameters, InputStruct struct, out OutputStruct output); OR void FuncName(InputStruct struct, out OutputStruct output);
		
		if (FHLSLMaterialShader::PIXEL_SHADER != Shader.ShaderStage && Shader.ReturnType != "float3")
		{
			// We disallow output structs for vertex/normals and instead use the direct custom node output for them. The reason being is the custom outputs of the node don't seem to work when plugged into WPO/Normal attributes
			FHLSLShaderMessages::ShowError(TEXT("%s Shader must return float3: %s %s"), *Shader.ShaderStage, *Shader.Name);
			return;
		}
		if (FHLSLMaterialShader::PIXEL_SHADER == Shader.ShaderStage && Shader.ReturnType != "void")
		{
			FHLSLShaderMessages::ShowError(TEXT("Pixel Shader must return void: %s"), *Shader.Name);
			return;
		}

		// Verify that each shader function is taking in the correct arguments
		const int MinNumParameters = Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER ? 2 : 1;
		const int MaxNumParameters = Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER ? 3 : 2;

		if (Shader.Arguments.Num() < MinNumParameters || Shader.Arguments.Num() > MaxNumParameters)
		{
			FHLSLShaderMessages::ShowError(TEXT("Error: Invalid shader function arguments: %s"), *Shader.Name);
			return;
		}

		// Verify that the arguments are valid
		int32 ArgOffset = 0;
		if (Shader.Arguments.Num() == MaxNumParameters)
		{
			const FString FirstArg = Shader.Arguments[0];
			if (!(FirstArg.Equals("FMaterialPixelParameters Parameters") || FirstArg.Equals("FMaterialVertexParameters Parameters")))
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse function parameters for shader [%s]"), *Shader.Name);
				return;
			}

			ArgOffset = 1;
		}

		if (Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER)
		{
			TArray<FString> ArgOneExplode, ArgTwoExplode;
			TArray<FString> InputStructArgs, OutputStructArgs;

			Shader.Arguments[0 + ArgOffset].ParseIntoArrayWS(ArgOneExplode);
			Shader.Arguments[1 + ArgOffset].ParseIntoArrayWS(ArgTwoExplode);

			if(ArgOneExplode.IsEmpty() || ArgTwoExplode.IsEmpty())
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for %s"), *Shader.Name);
				return;
			}

			// Retrieve which of the arguments is the input/output structs
			if (ArgOneExplode[0].Equals("out") && !ArgTwoExplode[0].Equals("out"))
			{
				InputStructArgs = ArgTwoExplode;
				OutputStructArgs = ArgOneExplode;
			}
			else if (ArgTwoExplode[0].Equals("out") && !ArgOneExplode[0].Equals("out"))
			{
				InputStructArgs = ArgOneExplode;
				OutputStructArgs = ArgTwoExplode;
			}
			else
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for %s"), *Shader.Name);
				return;
			}

			// Verify that the input/output args are using the correct struct types
			if (!InputStructArgs[0].Equals(Shader.InputStruct_Raw.Name) || !OutputStructArgs[1].Equals(Shader.OutputStruct_Raw.Name))
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for %s"), *Shader.Name);
				return;
			}

			// Remove struct explicit refs from function body
			// 'Input.Param' -> 'Param'
			// 'Output.Param' -> 'Param'
			const FString FuncBodyInputStruct = InputStructArgs[1] + ".";
			const FString FuncBodyOutputStruct = OutputStructArgs[2] + ".";
			Shader.Body.ReplaceInline(ToCStr(FuncBodyInputStruct), TEXT(""));
			Shader.Body.ReplaceInline(ToCStr(FuncBodyOutputStruct), TEXT(""));
		}
		else // Normals & Vertex shader need to explicitly return a value
		{
			TArray<FString> InputStructArgs;

			Shader.Arguments[0 + ArgOffset].ParseIntoArrayWS(InputStructArgs);

			if(InputStructArgs.IsEmpty())
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for %s"), *Shader.Name);
				return;
			}
			else if (InputStructArgs[0].Equals("out"))
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for [Out keyword illegal] %s"), *Shader.Name);
				return;
			}

			// Verify that the input/output args are using the correct struct types
			if (!InputStructArgs[0].Equals(Shader.InputStruct_Raw.Name))
			{
				FHLSLShaderMessages::ShowError(TEXT("Error: Failed to parse input arguments for %s"), *Shader.Name);
				return;
			}

			// Remove struct explicit refs from function body
			// 'Input.Param' -> 'Param'
			const FString FuncBodyInputStruct = InputStructArgs[1] + ".";
			Shader.Body.ReplaceInline(ToCStr(FuncBodyInputStruct), TEXT(""));
		}
		
	}
	
	// Generate input/output struct params from each struct
	Library.ShaderResults.Empty();
	for (FHLSLMaterialShader& Shader : Shaders)
	{
		FString InputErrors = FHLSLShaderParser::ParseInputStructs(Library, Shader.InputStruct_Raw, Shader.Inputs);
		FString OutputErrors = FHLSLShaderParser::ParseOutputStructs(Library, Shader.OutputStruct_Raw, Shader.Outputs);
		
		if (!InputErrors.IsEmpty() || !OutputErrors.IsEmpty())
		{
			FHLSLShaderMessages::ShowError(TEXT("%s: (%s) (%s)"), *Shader.Name, *InputErrors, *OutputErrors);
			return;
		}
		
		BaseHash += Shader.InputStruct_Raw.Name;
		BaseHash += Shader.InputStruct_Raw.Body;
		BaseHash += Shader.OutputStruct_Raw.Name;
		BaseHash += Shader.OutputStruct_Raw.Body;

		auto& Result = Library.ShaderResults.Emplace_GetRef();
		Result.Arguments = Shader.Arguments;
		Result.Body = Shader.Body;
		Result.Name = Shader.Name;
		Result.ReturnType = Shader.ReturnType;
		Result.ShaderStage = Shader.ShaderStage;

		for (auto Input : Shader.Inputs)
		{
			auto& InputDbg = Result.Inputs.Emplace_GetRef();
			InputDbg.Name = Input.Name;
			InputDbg.Type = Input.Type;
			InputDbg.bIsConst = Input.bIsConst;
			InputDbg.DefaultValue = Input.DefaultValue;
			InputDbg.DefaultValueVector = Input.DefaultValueVector;
			InputDbg.MetaStringRaw = Input.MetaStringRaw;
			InputDbg.ShaderStage = Input.ShaderStage;
			InputDbg.InputType = Input.InputType;
			
			for (auto Meta : Input.Meta)
			{
				auto& MetaDb = InputDbg.Meta.Emplace_GetRef();
				MetaDb.Parameters = Meta.Parameters;
				MetaDb.Tag = Meta.Tag;
			}
		}

		for (auto Output : Shader.Outputs)
		{
			auto& OutputDbg = Result.Outputs.Emplace_GetRef();
			OutputDbg.Name = Output.Name;
			OutputDbg.ShaderStage = Output.ShaderStage;
			OutputDbg.Type = Output.Type;
			OutputDbg.Semantic = Output.Semantic;
			OutputDbg.OutputProperty = Output.OutputProperty;
			OutputDbg.OutputType = Output.OutputType;
		}
	}


	
	// Create or retrieve the material asset and set up its base settings first
	{
		const FString Error = GenerateMaterialForShader(Library, Settings);
		if (!Error.IsEmpty())
		{
			FHLSLShaderMessages::ShowError(TEXT("Creating Material Error: %s"), *Error);
			return;
		}
	}

	// Compare hash to see if we need to regenerate or if the material is up to date
	for (UMaterialExpressionComment* Comment : Library.Materials->FunctionEditorComments)
	{
		if (Comment && Comment->Text.Contains(BaseHash))
		{
			UE_LOG(LogHLSLMaterial, Log, TEXT("%s already up to date"), *Library.GetName());
			return;
		}
	}

	// Generate the shaders
	//FMaterialUpdateContext UpdateContext;
	//UpdateContext.AddMaterial(Library.Materials.Get());
	
	// We're gonna be resetting everything, but to keep track of state, cache existing GUIDs and opt to add them as is back in instead of creating new ones if possible
	TMap<FName, FGuid> ParameterGuids;
	for (UMaterialExpression* Expression : Library.Materials->FunctionExpressions)
	{
		if (UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression))
		{
			ParameterGuids.Add(Parameter->ParameterName, Parameter->ExpressionGUID);
		}
	}

	// Setup transaction for material generation then start generating
	{
		const FScopedTransaction Transaction( NSLOCTEXT("HLSLShader", "MaterialShaderRegen", "HLSL SHader: Material Regeneration") );
    	Library.Materials->Modify();
	
		// Disconnect anything connected to MaterialProperties, as those wont be deleted if they're still connected
		Library.Materials->PreEditChange(nullptr);
	
		UMaterialEditingLibrary::DeleteAllMaterialExpressions(Library.Materials.Get());
		Library.Materials->FunctionExpressions.Empty();
		Library.Materials->FunctionEditorComments.Empty();
		
		// Generate the actual shader/material graph
		for (FHLSLMaterialShader Shader : Shaders)
		{
			Shader.HashedString = Shader.GenerateHashedString(BaseHash);

			// Add dummy output for loops to work
			if (Shader.ShaderStage != FHLSLMaterialShader::PIXEL_SHADER)
			{
				Shader.Outputs.Empty();
				auto& OutputDummy = Shader.Outputs.Emplace_GetRef();
				OutputDummy.Name = "result";
				OutputDummy.Semantic = Shader.ShaderStage == FHLSLMaterialShader::VERTEX_SHADER ? "vertexoffset" : "normal";
				OutputDummy.OutputProperty = Shader.ShaderStage == FHLSLMaterialShader::VERTEX_SHADER ? MP_WorldPositionOffset : MP_Normal;
			}

			const FString Error = FHLSLShaderGenerator::GenerateShader(
				Library,
				IncludeFilePaths,
				Shader,
				ParameterGuids);
		
			if (!Error.IsEmpty())
			{
				FHLSLShaderMessages::ShowError(TEXT("Shader %s: %s"), *Shader.Name, *Error);
			}
		}

		if (Library.Materials->MaterialGraph)
			Library.Materials->MaterialGraph->RebuildGraph();
		
		Library.Materials->PostEditChange();
	
		// Mark material as dirty and recompile its shader
		FMaterialUpdateContext UpdateContext;
		UpdateContext.AddMaterial(Library.Materials.Get());
		UMaterialEditingLibrary::RecompileMaterial(Library.Materials.Get());
	}
	
	// message
	FNotificationInfo Info(FText::Format(INVTEXT("{0} updated"), FText::FromString(Library.GetFilePath())));
	Info.ExpireDuration = 5.f;
	Info.CheckBoxState = ECheckBoxState::Checked;
	FSlateNotificationManager::Get().AddNotification(Info);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FString FHLSLShaderLibraryEditor::GenerateMaterialForShader(UHLSLShaderLibrary& Library, const TArray<FHLSLShaderParser::FSetting>& MaterialSettings)
{
	TSoftObjectPtr<UMaterial>* MaterialPtr = &Library.Materials;
	
	FString BasePath = FPackageName::ObjectPathToPackageName(Library.GetPathName());
	if (Library.bPutFunctionsInSubdirectory)
	{
		BasePath += "_Generated";
	}
	else
	{
		BasePath = FPaths::GetPath(BasePath);
	}
	
	UMaterial* Material = MaterialPtr->Get();
	if (!Material)
	{
		FString Error;
		{
			// create an unreal material asset
			FString FixedMaterialName = TEXT("M_") + Library.GetName();
			FString NewPackageName = BasePath + TEXT("/") + FixedMaterialName;

			UPackage* Package = UPackageTools::FindOrCreatePackageForAssetType(FName(*NewPackageName), UMaterial::StaticClass());
			auto MaterialFactory = NewObject<UMaterialFactoryNew>();
			Material = (UMaterial*)MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), Package, *FixedMaterialName, RF_Standalone|RF_Public, NULL, GWarn);
			FAssetRegistryModule::AssetCreated(Material);
			Package->SetDirtyFlag(true);
		}
		
		//Material = CreateAsset<UMaterial>(Library.GetName(), BasePath, Error);
		if (!Error.IsEmpty())
		{
			ensure(!Material);
			return Error;
		}
	}

	if (!Material)
	{
		return "Failed to create asset";
	}

	if (*MaterialPtr != Material)
	{
		Library.MarkPackageDirty();
	}
	*MaterialPtr = Material;

	// Setup material settings

	FString Error = SetupMaterialSettings(Library.Materials.Get(), MaterialSettings);
	if (!Error.IsEmpty()) return Error;

	
	Library.Materials = Material;

	return "";
}

FString FHLSLShaderLibraryEditor::SetupMaterialSettings(UMaterial* Material,
	const TArray<FHLSLShaderParser::FSetting>& Settings)
{
	static const TMap<FString, EMaterialDomain> DomainMap =
	{
		{"surface", MD_Surface},
		{"decal", MD_DeferredDecal},
		{"lightfunction", MD_LightFunction},
		{"volume", MD_Volume},
		{"postprocess", MD_PostProcess},
		{"ui", MD_UI}
	};
	static const TMap<FString, EMaterialShadingModel> ShadingModelMap =
	{
		{"unlit", MSM_Unlit},
		{"defaultlit", MSM_DefaultLit},
		{"subsurface", MSM_Subsurface},
		{"preintegratedskin", MSM_PreintegratedSkin},
		{"clearcoat", MSM_ClearCoat},
		{"subsurfaceprofile", MSM_SubsurfaceProfile},
		{"twosidedfoliage", MSM_TwoSidedFoliage},
		{"hair", MSM_Hair},
		{"cloth", MSM_Cloth},
		{"eye", MSM_Eye},
		{"singlelayerwater", MSM_SingleLayerWater},
		{"thintranslucent", MSM_ThinTranslucent},
		{"lightsmuggler", MSM_LightSmuggler}
	};
	static const TMap<FString, EBlendMode> BlendMap =
	{
		{"opaque", BLEND_Opaque},
		{"mask", BLEND_Masked},
		{"translucent", BLEND_Translucent},
		{"additive", BLEND_Additive},
		{"modulate", BLEND_Modulate},
		{"premultiplied", BLEND_AlphaComposite},
		{"alphaholdout", BLEND_AlphaHoldout}
	};
	static const TMap<FString, EMaterialDecalResponse> DecalResponseMap =
	{
		{"none", MDR_None},
		{"color_normal_roughness", MDR_ColorNormalRoughness},
		{"color", MDR_Color},
		{"color_normal", MDR_ColorNormal},
		{"color_roughness", MDR_ColorRoughness},
		{"normal", MDR_Normal},
		{"normal_roughness", MDR_NormalRoughness},
		{"roughness", MDR_Roughness}
	};
	static const TMap<FString, EDecalBlendMode> DecalBlendMap =
	{
		{"translucent", DBM_Translucent},
		{"stain", DBM_Stain},
		{"normal", DBM_Normal},
		{"emissive", DBM_Emissive},
		{"dbuffer_color_normal_roughness", DBM_DBuffer_ColorNormalRoughness},
		{"dbuffer_color", DBM_DBuffer_Color},
		{"dbuffer_color_normal", DBM_DBuffer_ColorNormal},
		{"dbuffer_color_roughness", DBM_DBuffer_ColorRoughness},
		{"dbuffer_normal", DBM_DBuffer_Normal},
		{"dbuffer_normal_roughness", DBM_DBuffer_NormalRoughness},
		{"dbuffer_roughness", DBM_DBuffer_Roughness}
	};
	static const TMap<FString, EBlendableLocation> BlendableLocationMap =
	{
		{"before_translucency", BL_SceneColorBeforeDOF},
		{"before_tonemapping", BL_SceneColorAfterDOF},
		{"translucency_after_dof", BL_TranslucencyAfterDOF},
		{"ssr_input", BL_SSRInput},
		{"before_bloom", BL_SceneColorBeforeBloom},
		{"tonemapper", BL_ReplacingTonemapper},
		{"after_tonemapping", BL_SceneColorAfterTonemapping}
	};
	static const TMap<FString, EMaterialStencilCompare> StencilCompareMap =
	{
		{"less", MSC_Less}, {"less_equal", MSC_LessEqual}, {"greater", MSC_Greater},
		{"greater_equal", MSC_GreaterEqual}, {"equal", MSC_Equal}, {"not_equal", MSC_NotEqual},
		{"never", MSC_Never}, {"always", MSC_Always}
	};
	static const TMap<FString, bool> BoolMap =
	{
		{"true", true},{"1", true},{"false", false}, {"0", false}
	};

	for (const auto& Setting : Settings)
	{
		const FString SettingTag = Setting.Setting.ToLower();

		if (SettingTag == "domain")
		{
			if (!DomainMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized material domain [%s]"), *Setting.Value);
			Material->MaterialDomain = DomainMap[Setting.Value];
		}
		else if (SettingTag == "shadingmodel")
		{
			if (!ShadingModelMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized shading model [%s]"), *Setting.Value);
			Material->SetShadingModel(ShadingModelMap[Setting.Value]);
		}
		else if (SettingTag == "blendmode")
		{
			if (!BlendMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized blend mode [%s]"), *Setting.Value);
			Material->BlendMode = BlendMap[Setting.Value];
		}
		else if (SettingTag == "decalresponse")
		{
			if (!DecalResponseMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized decal response [%s]"), *Setting.Value);
			Material->MaterialDecalResponse = DecalResponseMap[Setting.Value];
		}
		else if (SettingTag == "decalblendmode")
		{
			if (!DecalBlendMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized decal blend mode [%s]"), *Setting.Value);
			Material->DecalBlendMode = DecalBlendMap[Setting.Value];
		}
		else if (SettingTag == "blendablelocation")
		{
			if (!BlendableLocationMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized blendable location [%s]"), *Setting.Value);
			Material->BlendableLocation = BlendableLocationMap[Setting.Value];
		}
		else if (SettingTag == "outputalpha")
		{
			if (!BoolMap.Contains(Setting.Value)) return FString::Printf(TEXT("ERROR: Unrecognized boolean value [%s] for setting [%s]"), *Setting.Value, *SettingTag);
			Material->BlendableOutputAlpha = BoolMap[Setting.Value]; 
		}
		else if (SettingTag == "blendablepriority")
		{
			Material->BlendablePriority = FCString::Atoi(ToCStr(Setting.Value));
		}
		else if (SettingTag == "isblendable")
		{
			if (!BoolMap.Contains(Setting.Value)) return FString::Printf(TEXT("ERROR: Unrecognized boolean value [%s] for setting [%s]"), *Setting.Value, *SettingTag);
			Material->bIsBlendable = BoolMap[Setting.Value]; 
		}
		else if (SettingTag == "stenciltest")
		{
			if (!BoolMap.Contains(Setting.Value)) return FString::Printf(TEXT("ERROR: Unrecognized boolean value [%s] for setting [%s]"), *Setting.Value, *SettingTag);
			Material->bEnableStencilTest =  BoolMap[Setting.Value];
		}
		else if (SettingTag == "stencilcompare")
		{
			if (!StencilCompareMap.Contains(Setting.Value))
				return FString::Printf(TEXT("ERROR: Unrecognized stencil compare operation [%s]"), *Setting.Value);
			Material->StencilCompare = StencilCompareMap[Setting.Value];
		}
		else if (SettingTag == "stencilvalue")
		{
			Material->StencilRefValue = FCString::Strtoui64(*Setting.Value, NULL, 10);
		}
		else if (SettingTag == "twosided")
		{
			if (!BoolMap.Contains(Setting.Value)) return FString::Printf(TEXT("ERROR: Unrecognized boolean value [%s] for setting [%s]"), *Setting.Value, *SettingTag);
			Material->TwoSided = BoolMap[Setting.Value]; 
		}
		else
		{
			return "Error: Unrecognized setting";
		}
	}

	// Some minor checking
	if (Material->BlendMode != BLEND_Translucent && Material->MaterialDomain == MD_DeferredDecal)
	{
		return "ERROR: Decal shader requires blend mode to be translucent";
	}
	
	return "";
}


bool FHLSLShaderLibraryEditor::TryLoadFileToString(FString& Text, const FString& FullPath)
{
	if (!FPaths::FileExists(FullPath))
	{
		return false;
	}

	if (!FFileHelper::LoadFileToString(Text, *FullPath))
	{
		// Wait and retry in case the text editor has locked the file
		FPlatformProcess::Sleep(0.1f);

		if (!FFileHelper::LoadFileToString(Text, *FullPath))
		{
			UE_LOG(LogHLSLMaterial, Error, TEXT("Failed to read %s"), *FullPath);
			return false;
		}
	}

	return true;
}

UObject* FHLSLShaderLibraryEditor::CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString& OutError)
{
	const FString PackageName = FolderPath / AssetName;

	{
		FString NewPackageName;
		FString NewAssetName;

		const FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(PackageName, {}, NewPackageName, NewAssetName);

		if (NewAssetName != AssetName)
		{
			OutError = FString::Printf(
				TEXT("Asset %s already exists! Add it back to the HLSL library Materials if you want it to be updated"),
				*PackageName);
			return nullptr;
		}
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!ensure(Package))
	{
		return nullptr;
	}

	auto* Object = NewObject<UObject>(Package, Class, *AssetName, RF_Public | RF_Standalone);
	if (!ensure(Object))
	{
		return nullptr;
	}

	Object->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Object);

	return Object;
}