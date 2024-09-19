#include "HLSLMaterialUtilities.h"
#include "HLSLShader.h"
#include "HLSLShaderGenerator.h"
#include "HLSLShaderLibrary.h"
#include "HLSLShaderLibraryEditor.h"
#include "HLSLShaderMessages.h"
#include "Engine/Texture2DArray.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "MetaTags/HLSLMetaTagHandler.h"

UMaterialExpression* FHLSLShaderInput::GetInputExpression(UHLSLShaderLibrary& Library, const FGuid ParamGUID, int32 Index, const FString& BasePath) const
{
	UMaterial* Material = Library.Materials.Get();
	UMaterialExpression* TargetExpression = nullptr;

	// Unique parameters (particles, paramcollections, etc...) only have 1 meta tag for the specifier which is verified at this point, so we check if thats our case or a default parameter type
	{
		if (Meta.Num() > 0 && FHLSLShaderGenerator::UniqueMetaTagStructMap.Contains(Meta[0].Tag.ToLower()))
		{
			TargetExpression = FHLSLShaderGenerator::UniqueMetaTagStructMap[Meta[0].Tag]->GenerateUniqueExpression(Library, *this, Meta[0], Index);
			checkf(TargetExpression, TEXT("ERROR: Target expression not generated properly for input [%s]"), *Name);
			return TargetExpression;
		}
	}
	
	const auto SetupExpression = [&](auto* Expression)
	{
		FString ParameterName = Name;

		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->ExpressionGUID = ParamGUID;
		if (!Expression->ExpressionGUID.IsValid())
		{
			Expression->ExpressionGUID = FGuid::NewGuid();
		}
		Expression->SortPriority = Index;
		Expression->ParameterName = *ParameterName;
		Expression->bCollapsed = true;
		Expression->MaterialExpressionEditorX = 0;
		Expression->MaterialExpressionEditorY = 200 * Index;
		
		Material->FunctionExpressions.Add(Expression);
	};

	switch (InputType)
	{
	case FunctionInput_StaticBool:
	{
		UMaterialExpressionStaticBoolParameter* Expression = NewObject<UMaterialExpressionStaticBoolParameter>(Material);
		SetupExpression(Expression);
		SetupParameterMetaTags(Expression);

		if (!DefaultValue.IsEmpty())
		{
			Expression->DefaultValue = bDefaultValueBool;
		}

		TargetExpression = Expression;
	}
		break;
	case FunctionInput_Scalar:
	{
		UMaterialExpressionScalarParameter* Expression = NewObject<UMaterialExpressionScalarParameter>(Material);
		SetupExpression(Expression);
		SetupParameterMetaTags(Expression);

		if (!DefaultValue.IsEmpty())
		{
			Expression->DefaultValue = DefaultValueVector.X;
		}
		
		TargetExpression = Expression;
	}
		break;
	case FunctionInput_Vector2:
	case FunctionInput_Vector3:
	case FunctionInput_Vector4:
	{
		UMaterialExpressionVectorParameter* Expression = NewObject<UMaterialExpressionVectorParameter>(Material);
		SetupExpression(Expression);
		SetupParameterMetaTags(Expression);

		if (!DefaultValue.IsEmpty())
		{
			Expression->DefaultValue = FLinearColor(DefaultValueVector);
		}
		
		if (InputType == FunctionInput_Vector4)
		{
			UMaterialExpressionAppendVector* AppendVector = NewObject<UMaterialExpressionAppendVector>(Material);
			Material->FunctionExpressions.Add(AppendVector);
			AppendVector->MaterialExpressionEditorX = 150;
			AppendVector->MaterialExpressionEditorY = 200 * Index;

			AppendVector->A.Connect(0, Expression);
			AppendVector->B.Connect(4, Expression);
			
			TargetExpression = AppendVector;
		}
		else if (InputType == FunctionInput_Vector3)
		{
			TargetExpression = Expression;
		}
		else
		{
			UMaterialExpressionMaterialFunctionCall* MakeFloat2Call = NewObject<UMaterialExpressionMaterialFunctionCall>(Material);
			FString FuncPath = TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/MakeFloat2.MakeFloat2");
			UMaterialFunction* MakeFloat2Func = LoadObject<UMaterialFunction>(NULL, *FuncPath, NULL, 0, NULL);
			MakeFloat2Call->SetMaterialFunction(MakeFloat2Func);
			Material->FunctionExpressions.Add(MakeFloat2Call);

			Expression->ConnectExpression(MakeFloat2Call->GetInput(0), 1);
			Expression->ConnectExpression(MakeFloat2Call->GetInput(1), 2);
			TargetExpression = MakeFloat2Call;
		}
	}
		break;
	case FunctionInput_Texture2D:
	case FunctionInput_TextureCube:
	case FunctionInput_Texture2DArray:
	case FunctionInput_VolumeTexture:
	case FunctionInput_TextureExternal:
	{
		UMaterialExpressionTextureObjectParameter* Expression = NewObject<UMaterialExpressionTextureObjectParameter>(Material);
		SetupExpression(Expression);
		SetupParameterMetaTags(Expression);

		TargetExpression = Expression;

		switch (InputType)
		{
		case FunctionInput_Texture2D:
		{
			// Default is already a Texture2D
		}
			break;
		case FunctionInput_TextureCube:
		{
			Expression->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultTextureCube"));
		}
			break;
		case FunctionInput_Texture2DArray:
		{
			// Hacky

			UTexture2DArray* TextureArray = LoadObject<UTexture2DArray>(nullptr, *(BasePath / TEXT("DefaultTextureArray")));
			if (!TextureArray)
			{
				FString Error;
				TextureArray = FHLSLShaderLibraryEditor::CreateAsset<UTexture2DArray>("DefaultTextureArray", BasePath, Error);
				if (!Error.IsEmpty())
				{
					UE_LOG(LogHLSLMaterial, Error, TEXT("Failed to create %s/DefaultTextureArray: %s"), *BasePath, *Error);
				}

				if (TextureArray)
				{
					TextureArray->SourceTextures.Add(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture_Low.DefaultTexture")));
				}
			}
			Expression->Texture = TextureArray;
		}
			break;
		case FunctionInput_VolumeTexture:
		{
			Expression->Texture = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/DefaultVolumeTexture"));
		}
			break;
		case FunctionInput_TextureExternal:
		{
			// No idea what to do here
		}
			break;
		default: check(false);
		}
	}
		break;
	default: check(false);
	}

	return TargetExpression;
}


void FHLSLShaderInput::SetupParameterMetaTags(UMaterialExpression* Parameter) const
{
	for (const auto& MetaTag : Meta)
	{
		const FString Tag = MetaTag.Tag.ToLower();

		if (FHLSLShaderGenerator::MetaTagStructMap.Contains(Tag))
		{
			// Tag is guaranteed to be supported at this point so no reason to check, but ill check anyways ^.^
			FHLSLShaderGenerator::MetaTagStructMap[Tag]->SetupExpressionMetaTag(Parameter, MetaTag);
		}
		else
		{
			FHLSLShaderMessages::ShowError(TEXT("Unrecognized Meta-Tag [%s] On Parameter [%s]"), *MetaTag.Tag, *Name);
		}
	}
}

UClass* FHLSLShaderInput::GetBranchExpressionClass(bool& bRequiresBoolInput, int32& TrueIdx, int32& FalseIdx) const
{
	UClass* Class = UMaterialExpressionStaticSwitch::StaticClass();

	/*
	struct FSwitchParameterClassification
	{
		UClass* Class;
		bool bRequiresBoolInput;
		int32 TrueIdx, FalseIdx;
	};

	static const TMap<FString, FSwitchParameterClassification> SwitchParameterMap =
	{
		{"bShadowPass", {UMaterialExpressionShadowReplace::StaticClass(), false, 1, 0}},
	};
	
	if (SwitchParameterMap.Contains(Name))
	{
		Class = SwitchParameterMap[Name].Class;
		bRequiresBoolInput = SwitchParameterMap[Name].bRequiresBoolInput;
		TrueIdx = SwitchParameterMap[Name].TrueIdx;
		FalseIdx = SwitchParameterMap[Name].FalseIdx;
	}
	*/
	
	return Class;
}
