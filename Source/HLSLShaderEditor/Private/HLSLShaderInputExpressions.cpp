#include "HLSLMaterialUtilities.h"
#include "HLSLShader.h"
#include "HLSLShaderLibrary.h"
#include "HLSLShaderLibraryEditor.h"
#include "HLSLShaderMessages.h"
#include "Engine/Texture2DArray.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialParameterCollection.h"

UMaterialExpression* FHLSLShaderInput::GetInputExpression(UHLSLShaderLibrary& Library, const FGuid ParamGUID, int32 Index, const FString& BasePath) const
{
	UMaterial* Material = Library.Materials.Get();
	UMaterialExpression* TargetExpression = nullptr;

	// Unique parameters (particles, paramcollections, etc...) only have 1 meta tag for the specifier which is verified at this point, so we check if thats our case or a default parameter type
	{
		static const TArray<FString> UniqueExpressionsSupported =
		{
			"particles", "parametercollection",
		};
		if (Meta.Num() == 1 && UniqueExpressionsSupported.Contains(Meta[0].Tag.ToLower()))
		{
			TargetExpression = GetUniqueInputExpression(Library, Meta[0], Index);
			if (TargetExpression != nullptr)
				return TargetExpression;

			FHLSLShaderMessages::ShowError(TEXT("ERROR: Failed to parse unique input [%s]"), *Name);
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
		SetupParameterTextureMetaTags(Expression);

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


void FHLSLShaderInput::SetupParameterMetaTags(UMaterialExpressionParameter* Parameter) const
{
	for (const auto& MetaTag : Meta)
	{
		const FString Tag = MetaTag.Tag.ToLower();

		if (Tag == "group")
		{
			Parameter->Group = FName(MetaTag.Parameters[0]);
		}
		else if (Tag == "channels")
		{
			if (auto* VectorParam = Cast<UMaterialExpressionVectorParameter>(Parameter))
			{
				TArray<FString> ChannelNames;
				const int32 NumNames = MetaTag.Parameters[0].ParseIntoArrayWS(ChannelNames, TEXT(","));
				FParameterChannelNames InputChannelNames;
				InputChannelNames.R = NumNames > 0 ? FText::FromString(ChannelNames[0]) : FText();
				InputChannelNames.G = NumNames > 1 ? FText::FromString(ChannelNames[1]) : FText();
				InputChannelNames.B = NumNames > 2 ? FText::FromString(ChannelNames[2]) : FText();
				InputChannelNames.A = NumNames > 3 ? FText::FromString(ChannelNames[3]) : FText();
				VectorParam->ChannelNames = InputChannelNames;
			}
		}
		else if (Tag == "primitivedata")
		{
			const uint8 PrimitiveIdx = FCString::Strtoui64(*MetaTag.Parameters[0], NULL, 10);
			if (auto* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Parameter))
			{
				ScalarParam->bUseCustomPrimitiveData = true;
				ScalarParam->PrimitiveDataIndex = PrimitiveIdx;
			}
			else if (auto* VectorParam = Cast<UMaterialExpressionVectorParameter>(Parameter))
			{
				VectorParam->bUseCustomPrimitiveData = true;
				VectorParam->PrimitiveDataIndex = PrimitiveIdx;
			}
		}
		else if (Tag == "range")
		{
			if (auto* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Parameter))
			{
				ScalarParam->SliderMin = FCString::Atof(ToCStr(MetaTag.Parameters[0]));
				ScalarParam->SliderMax = FCString::Atof(ToCStr(MetaTag.Parameters[1]));
			}
		}
		else
		{
			FHLSLShaderMessages::ShowError(TEXT("Unrecognized Meta-Tag [%s] On Parameter [%s]"), *MetaTag.Tag, *Name);
		}
	}
}

void FHLSLShaderInput::SetupParameterTextureMetaTags(UMaterialExpressionTextureObjectParameter* Parameter) const
{
	for (const auto& MetaTag : Meta)
	{
		const FString Tag = MetaTag.Tag.ToLower();

		if (Tag == "group")
		{
			Parameter->Group = FName(MetaTag.Parameters[0]);
		}
		else if (Tag == "samplertype")
		{
			static const TMap<FString, EMaterialSamplerType> SamplerTypeMap =
			{
				{"color", SAMPLERTYPE_Color}, {"grayscale", SAMPLERTYPE_Grayscale}, {"alpha", SAMPLERTYPE_Alpha},
				{"normal", SAMPLERTYPE_Normal}, {"masks", SAMPLERTYPE_Masks}, {"linear_color", SAMPLERTYPE_LinearColor},
				{"linear_grayscale", SAMPLERTYPE_LinearGrayscale}
			};
			if (SamplerTypeMap.Contains(MetaTag.Parameters[0]))
				Parameter->SamplerType = SamplerTypeMap[MetaTag.Parameters[0]];
			else FHLSLShaderMessages::ShowError(TEXT("ERROR: Unregonized Sampler Type [%s] for texture parameter [%s]"), *MetaTag.Parameters[0], *Name);

		}
		else
		{
			FHLSLShaderMessages::ShowError(TEXT("Unrecognized Meta-Tag [%s] On Parameter [%s]"), *MetaTag.Tag, *Name);
		}
	}
}

UMaterialExpression* FHLSLShaderInput::GetUniqueInputExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInputMeta& MetaTag, int32 Index) const
{
	UMaterial* Material = Library.Materials.Get();
	const auto SetupExpression = [&](auto* Expression)
	{
		FString ParameterName = Name;

		Expression->MaterialExpressionGuid = FGuid::NewGuid();
		Expression->bCollapsed = true;
		Expression->MaterialExpressionEditorX = 0;
		Expression->MaterialExpressionEditorY = 200 * Index;
		
		Material->FunctionExpressions.Add(Expression);
	};
	
	const FString Tag = MetaTag.Tag.ToLower();
	if (Tag == "particles")
	{
		UMaterialExpressionDynamicParameter* Expression = NewObject<UMaterialExpressionDynamicParameter>(Material);
		SetupExpression(Expression);

		if (!DefaultValue.IsEmpty())
			Expression->DefaultValue = FLinearColor(DefaultValueVector);
		Expression->ParameterIndex = FCString::Strtoui64(*MetaTag.Parameters[0], NULL, 10);
		if (MetaTag.Parameters.Num() == 2)
		{
			TArray<FString> ChannelNames;
			const int32 NumNames = MetaTag.Parameters[1].ParseIntoArrayWS(ChannelNames, TEXT(","));
			Expression->ParamNames = ChannelNames;

			// Have to fill out the rest since it should always contain 4 elements
			for (int idx = NumNames; idx < 4; idx++)
			{
				Expression->ParamNames.Add(FString::Printf(TEXT("Parameter %d"), idx+1));
			}
		}

		// Have to do an append so we can plug in a float4
		UMaterialExpressionAppendVector* AppendVector = NewObject<UMaterialExpressionAppendVector>(Material);
		Material->FunctionExpressions.Add(AppendVector);
		AppendVector->MaterialExpressionEditorX = 150;
		AppendVector->MaterialExpressionEditorY = 200 * Index;

		AppendVector->A.Connect(4, Expression); // RGB
		AppendVector->B.Connect(3, Expression); // A
		
		return AppendVector;
	}
	if (Tag == "parametercollection")
	{
		// We know that we have the associated correct collection by the time we get here, just have to retrieve it
		UMaterialExpressionCollectionParameter* Expression = NewObject<UMaterialExpressionCollectionParameter>(Material);
		SetupExpression(Expression);
		
		UMaterialParameterCollection* TargetCollection = nullptr;;
		FName ParamName = NAME_None;
		for (UMaterialParameterCollection* Collection : Library.ParameterCollections)
		{
			if (!Collection) continue;

			if (InputType == EFunctionInputType::FunctionInput_Scalar)
			{
				for (const FCollectionScalarParameter& ScalarParam : Collection->ScalarParameters)
				{
					// Break upon success
					if (MetaTag.Parameters[0].ToLower().Equals(ScalarParam.ParameterName.ToString().ToLower()))
					{
						TargetCollection = Collection;
						ParamName = ScalarParam.ParameterName;
						break;
					}
				}
			}
			else if (InputType == EFunctionInputType::FunctionInput_Vector4)
			{
				for (const FCollectionVectorParameter& VectorParams : Collection->VectorParameters)
				{
					// Break upon success
					if (MetaTag.Parameters[0].ToLower().Equals(VectorParams.ParameterName.ToString().ToLower()))
					{
						TargetCollection = Collection;
						ParamName = VectorParams.ParameterName;
						break;
					}
				}
			}
		}
		Expression->Collection = TargetCollection;
		Expression->SetParameterName(ParamName);
		Expression->ParameterId = TargetCollection->GetParameterId(ParamName);
		return Expression;
	}
	return nullptr;
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
