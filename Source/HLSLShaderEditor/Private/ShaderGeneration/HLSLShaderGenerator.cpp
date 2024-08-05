// Fill out your copyright notice in the Description page of Project Settings.


#include "HLSLShaderGenerator.h"

#include "HLSLMaterialUtilities.h"
#include "HLSLShader.h"
#include "HLSLShaderLibrary.h"
#include "HLSLMaterialEditor/Private/HLSLMaterialErrorHook.h"
#include "IMaterialEditor.h"
#include "MaterialEditorActions.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Internationalization/Regex.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"
#include "MaterialEditor.h"
#include "Materials/MaterialExpressionSceneTexture.h"


FString FHLSLShaderGenerator::GenerateShader(UHLSLShaderLibrary& Library, const TArray<FString>& IncludeFilePaths,
                                             FHLSLMaterialShader Shader, const TMap<FName, FGuid>& ParameterGuids)
{
	///////////////////////////////////////////////////////////////////////////////////
	//// Past this point, try to never error out as it'll break existing functions ////
	///////////////////////////////////////////////////////////////////////////////////

	// Figure out any parameters we need to inject into the material so UEs compiler is happy (TexCoords, SceneSamplers, etc...)

#pragma region Figure Out Used Params
	// Detect used texture coordinates
	int32 MaxTexCoordinateUsed = -1;
	{
		FRegexPattern RegexPattern(R"_(Parameters.TexCoords\[([0-9]+)\])_");
		FRegexMatcher RegexMatcher(RegexPattern, Shader.Body);
		while (RegexMatcher.FindNext())
		{
			MaxTexCoordinateUsed = FMath::Max(MaxTexCoordinateUsed, FCString::Atoi(*RegexMatcher.GetCaptureGroup(1)));
		}
	}

	// Detect used scene texture look-up
	const bool bSceneTextureUsed = Shader.Body.Contains("SceneTextureLookup", ESearchCase::CaseSensitive);
	// Detect used vertex colors
	const bool bVertexColorUsed = Shader.Body.Contains("Parameters.VertexColor", ESearchCase::CaseSensitive);
	// Detect whether NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS is required
	const bool bNeedsWorldPositionExcludingShaderOffsets = Shader.Body.Contains("GetWorldPosition_NoMaterialOffsets", ESearchCase::CaseSensitive);
#pragma endregion

	
	// Mark material as dirty once we're done
	ON_SCOPE_EXIT
	{
		Library.Materials->StateId = FGuid::NewGuid();
		Library.Materials->MarkPackageDirty();
	};

#pragma region Create Input Parameters Expressions
	// Keep track of indices to Static Bools since we'll be generating permutations and connecting them through static switches
	TArray<int32> StaticBoolParameters;
	for (int32 Index = 0; Index < Shader.Inputs.Num(); Index++)
	{
		if (Shader.Inputs[Index].InputType == FunctionInput_StaticBool)
		{
			StaticBoolParameters.Add(Index);
		}
	}

	// Create input Parameter material expressions and store them in an array which we'll connect later
	TArray<UMaterialExpression*> ShaderInputs;
	//GenerateShaderParameterExpressions(Shader, ShaderInputs);
	for (int32 Index = 0; Index < Shader.Inputs.Num(); Index++)
	{
		const FHLSLShaderInput& Input = Shader.Inputs[Index];

		ShaderInputs.Add(Input.GetInputExpression(Library, ParameterGuids.FindRef(*Input.Name), Index, ""));
	}
#pragma endregion

#pragma region Create Output Identifiers
	TArray<EMaterialProperty> OutputProperties;
	for (const auto& Output : Shader.Outputs)
	{
		OutputProperties.Add(Output.OutputProperty);
	}
#pragma endregion

#pragma region Create HLSL Expressions For All Permutations

	// Create output permutations
	struct FOutputPin
	{
		UMaterialExpression* Expression = nullptr;
		int32 Index = 0;
	};

	// Create the necessary expressions based on the static switches
	TArray<TArray<FOutputPin>> AllOutputPins;
	
	for (int32 Width = 0; Width < 1 << StaticBoolParameters.Num(); Width++)
	{
		FString LocalVariableDeclarations = "";
		for (int32 Index = 0; Index < StaticBoolParameters.Num(); Index++)
		{
			// Each permutation covers a seperate value of the bools
			bool bValue = Width & (1 << Index);
			// Invert the value, as switches take True as first pin
			bValue = !bValue;
			LocalVariableDeclarations += "const bool INTERNAL_IN_" + Shader.Inputs[StaticBoolParameters[Index]].Name + " = " + (bValue ? "true" : "false") + ";\n";
		}

		// Add other inputs
		for (const FHLSLShaderInput& Input : Shader.Inputs)
		{
			FString Cast;
			switch (Input.InputType)
			{
			case FunctionInput_Scalar:
			case FunctionInput_Vector2:
			case FunctionInput_Vector3:
			case FunctionInput_Vector4:
			{
				// Cast float to int if needed
				Cast = Input.Type;
			}
				break;
			case FunctionInput_Texture2D:
			case FunctionInput_TextureCube:
			case FunctionInput_Texture2DArray:
			case FunctionInput_VolumeTexture:
			case FunctionInput_TextureExternal:
			{
				LocalVariableDeclarations += (Input.bIsConst ? "const SamplerState " : "SamplerState ") + Input.Name + "Sampler" + " = INTERNAL_IN_" + Input.Name + "Sampler;\n";
			}
				break;
			case FunctionInput_StaticBool:
			case FunctionInput_MaterialAttributes:
			{
				// Nothing to fixup
			}
				break;
			case FunctionInput_MAX:
			default:
				ensure(false);
			}
			LocalVariableDeclarations += (Input.bIsConst ? "const " : "") + Input.Type + " " + Input.Name + " = " + Cast + "(INTERNAL_IN_" + Input.Name + ");\n";
		}

		// Create the custom HLSL node and start hooking things up
		UMaterialExpressionCustom* MaterialExpressionCustom = NewObject<UMaterialExpressionCustom>(Library.Materials.Get());
		MaterialExpressionCustom->MaterialExpressionGuid = FGuid::NewGuid();
		MaterialExpressionCustom->bCollapsed = true;
		MaterialExpressionCustom->OutputType = Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER ? CMOT_Float1 : CMOT_Float3;
		MaterialExpressionCustom->Code = GenerateFunctionCode(Library, Shader, LocalVariableDeclarations);
		MaterialExpressionCustom->MaterialExpressionEditorX = 500;
		MaterialExpressionCustom->MaterialExpressionEditorY = 200 * Width;
		MaterialExpressionCustom->IncludeFilePaths = IncludeFilePaths;
		Library.Materials->FunctionExpressions.Add(MaterialExpressionCustom);
		
		// Start hooking up inputs for this node
		MaterialExpressionCustom->Inputs.Reset();
		for (int32 Index = 0; Index < Shader.Inputs.Num(); Index++)
		{
			FHLSLShaderInput& Input = Shader.Inputs[Index];
			if (Input.InputType == FunctionInput_StaticBool)
			{
				continue;
			}

			FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
			CustomInput.InputName = *("INTERNAL_IN_" + Input.Name);
			CustomInput.Input.Connect(0, ShaderInputs[Index]);
		}
		
		// Add outputs to HLSL node (we'll connect them to the final material attributes later, indices indicate where)
		if (Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER)
		{
			for (int32 Index = 0; Index < Shader.Outputs.Num(); Index++)
			{
				const FHLSLShaderOutput& Output = Shader.Outputs[Index];
				MaterialExpressionCustom->AdditionalOutputs.Add({ *Output.Name, Output.OutputType });
			}
		}
		
		// Create the dummies necessary for certain things to compile for UE 
		{
			if (MaxTexCoordinateUsed != -1)
			{
				// Create a dummy texture coordinate index to ensure NUM_TEX_COORD_INTERPOLATORS is correct

				UMaterialExpressionTextureCoordinate* TextureCoordinate = NewObject<UMaterialExpressionTextureCoordinate>(Library.Materials.Get());
				TextureCoordinate->MaterialExpressionGuid = FGuid::NewGuid();
				TextureCoordinate->bCollapsed = true;
				TextureCoordinate->CoordinateIndex = MaxTexCoordinateUsed;
				TextureCoordinate->MaterialExpressionEditorX = MaterialExpressionCustom->MaterialExpressionEditorX - 200;
				TextureCoordinate->MaterialExpressionEditorY = MaterialExpressionCustom->MaterialExpressionEditorY;
				Library.Materials->FunctionExpressions.Add(TextureCoordinate);

				FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
				CustomInput.InputName = "DUMMY_COORDINATE_INPUT";
				CustomInput.Input.Connect(0, TextureCoordinate);
			}

			if (bSceneTextureUsed)
			{
				UMaterialExpressionSceneTexture* SceneTexture = NewObject<UMaterialExpressionSceneTexture>(Library.Materials.Get());
				SceneTexture->MaterialExpressionGuid = FGuid::NewGuid();
				SceneTexture->bCollapsed = true;
				SceneTexture->SceneTextureId = ESceneTextureId::PPI_PostProcessInput0;
				SceneTexture->MaterialExpressionEditorX = MaterialExpressionCustom->MaterialExpressionEditorX - 200;
				SceneTexture->MaterialExpressionEditorY = MaterialExpressionCustom->MaterialExpressionEditorY;
				Library.Materials->FunctionExpressions.Add(SceneTexture);

				FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
				CustomInput.InputName = "DUMMY_SCENETEX_INPUT";
				CustomInput.Input.Connect(0, SceneTexture);

			}

			if (bVertexColorUsed)
			{
				// Create a dummy vertex color parameter to ensure INTERPOLATE_VERTEX_COLOR is correct

				UMaterialExpressionVertexColor* Color = NewObject<UMaterialExpressionVertexColor>(Library.Materials.Get());
				Color->MaterialExpressionGuid = FGuid::NewGuid();
				Color->bCollapsed = true;
				Color->MaterialExpressionEditorX = MaterialExpressionCustom->MaterialExpressionEditorX - 200;
				Color->MaterialExpressionEditorY = MaterialExpressionCustom->MaterialExpressionEditorY;
				Library.Materials->FunctionExpressions.Add(Color);

				FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
				CustomInput.InputName = "DUMMY_COLOR_INPUT";
				CustomInput.Input.Connect(0, Color);
			}

			if (bNeedsWorldPositionExcludingShaderOffsets)
			{
				// Create a dummy world position node to ensure NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS is correct

				UMaterialExpressionWorldPosition* WorldPosition = NewObject<UMaterialExpressionWorldPosition>(Library.Materials.Get());
				WorldPosition->MaterialExpressionGuid = FGuid::NewGuid();
				WorldPosition->bCollapsed = true;
				WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
				WorldPosition->MaterialExpressionEditorX = MaterialExpressionCustom->MaterialExpressionEditorX - 200;
				WorldPosition->MaterialExpressionEditorY = MaterialExpressionCustom->MaterialExpressionEditorY;
				Library.Materials->FunctionExpressions.Add(WorldPosition);

				FCustomInput& CustomInput = MaterialExpressionCustom->Inputs.Emplace_GetRef();
				CustomInput.InputName = "DUMMY_WORLD_POSITION_INPUT";
				CustomInput.Input.Connect(0, WorldPosition);
			}
		}

		MaterialExpressionCustom->PostEditChange();

		TArray<FOutputPin>& OutputPins = AllOutputPins.Emplace_GetRef();
		for (int32 Index = 0; Index < Shader.Outputs.Num(); Index++)
		{
			// +1 as default output pin is result in HLSL node
			const int OutputOffset = Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER ? 1 : 0;
			OutputPins.Add({MaterialExpressionCustom, Index + OutputOffset});
		}
	}
#pragma endregion

#pragma region Create All Static Switches And Start Connecting Shit

	// We've created everything, so start creating the static switches and hooking them up
	for (int32 Layer = 0; Layer < StaticBoolParameters.Num(); Layer++)
	{
		const int32 InputIndex = StaticBoolParameters[Layer];
		const FHLSLShaderInput& Input = Shader.Inputs[InputIndex];

		const TArray<TArray<FOutputPin>> PreviousAllOutputPins = MoveTemp(AllOutputPins);

		for (int32 Width = 0; Width < 1 << (StaticBoolParameters.Num() - Layer - 1); Width++)
		{
			TArray<FOutputPin>& OutputPins = AllOutputPins.Emplace_GetRef();
			for (int32 Index = 0; Index < Shader.Outputs.Num(); Index++)
			{
				bool bRequiresBoolInput = true; int32 TrueIdx = 0, FalseIdx = 1; // e.g ShadowPass the order is flipped where True is the second input
				UClass* Class = Input.GetBranchExpressionClass(bRequiresBoolInput, TrueIdx, FalseIdx);

				UMaterialExpression* StaticSwitch = NewObject<UMaterialExpression>(Library.Materials.Get(), Class);
				StaticSwitch->MaterialExpressionGuid = FGuid::NewGuid();
				StaticSwitch->MaterialExpressionEditorX = (Layer + 2) * 500;
				StaticSwitch->MaterialExpressionEditorY = 200 * Width;
				Library.Materials->FunctionExpressions.Add(StaticSwitch);

				const FOutputPin& OutputPinA = PreviousAllOutputPins[2 * Width + 0][Index];
				const FOutputPin& OutputPinB = PreviousAllOutputPins[2 * Width + 1][Index];

				StaticSwitch->GetInput(TrueIdx)->Connect(OutputPinA.Index, OutputPinA.Expression);
				StaticSwitch->GetInput(FalseIdx)->Connect(OutputPinB.Index, OutputPinB.Expression);

				if (bRequiresBoolInput)
					StaticSwitch->GetInput(2)->Connect(0, ShaderInputs[InputIndex]);


				OutputPins.Add({ StaticSwitch, 0 });
			}
		}
	}
#pragma endregion

#pragma region Connect To Output/Material Attributes
	// Start connecting outputs to material attributes
	ensure(AllOutputPins.Num() == 1);
	for (int32 Index = 0; Index < Shader.Outputs.Num(); Index++)
	{
		const FOutputPin& Pin = AllOutputPins[0][Index];
		Library.Materials->GetExpressionInputForProperty(Shader.Outputs[Index].OutputProperty)->Connect(Pin.Index, Pin.Expression);
	}
#pragma endregion

#pragma region Finalize Material And Add Comment 
	{
		UMaterialExpressionComment* Comment = NewObject<UMaterialExpressionComment>(Library.Materials.Get());
		Comment->MaterialExpressionGuid = FGuid::NewGuid();
		Comment->MaterialExpressionEditorX = 0;
		Comment->MaterialExpressionEditorY = -200;
		Comment->SizeX = 1000;
		Comment->SizeY = 100;
		Comment->Text = "DO NOT MODIFY THIS\nAutogenerated from " + Library.File.FilePath + "\nLibrary " + Library.GetPathName() + "\n" + Shader.HashedString;
		Library.Materials->FunctionEditorComments.Add(Comment);
	}
#pragma endregion

	return "";
}

FString FHLSLShaderGenerator::GenerateFunctionCode(const UHLSLShaderLibrary& Library, const FHLSLMaterialShader& Shader, const FString& Declarations)
{
	FString Code;

	// In the pixel shader, replace any instances of return; with return 0.f; to make the custom HLSL expression happy (it requires an actual return value)
	if (Shader.ShaderStage == FHLSLMaterialShader::PIXEL_SHADER)
		Code += Shader.Body.Replace(TEXT("return"), TEXT("return 0.f"));
	else Code += Shader.Body;
	
	if (Library.bAccurateErrors)
	{
		Code = FString::Printf(TEXT(
			"#line %d \"%s%s%s\"\n%s\n#line 10000 \"Error occured outside of Custom HLSL node, line number will be inaccurate. "
			"Untick bAccurateErrors on your HLSL library to fix this (%s)\""),
			Shader.StartLine + 1,
			FHLSLMaterialErrorHook::PathPrefix,
			*Library.File.FilePath,
			FHLSLMaterialErrorHook::PathSuffix,
			*Code,
			*Library.GetPathName());
	}

	return FString::Printf(TEXT("// START %s\n\n%s\n%s\n\n// END %s\n\nreturn 0.f;\n//%s\n"), *Shader.Name, *Declarations, *Code, *Shader.Name, *Shader.HashedString);
}

IMaterialEditor* FHLSLShaderGenerator::FindMaterialEditorForAsset(UObject* InAsset)
{
	// From MaterialEditor\Private\MaterialEditingLibrary.cpp

	if (IAssetEditorInstance* AssetEditorInstance = (InAsset != nullptr) ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(InAsset, false) : nullptr)
	{
		// Ensure this is not a UMaterialInstanceDynamic, as that doesn't use IMaterialEditor as its editor
		if (!InAsset->IsA(UMaterialInstanceDynamic::StaticClass()))
		{
			return static_cast<IMaterialEditor*>(AssetEditorInstance);
		}
	}

	return nullptr;
}
