// Copyright 2023 CoC All rights reserved


#include "HLSLUniqueParameterMetaTags.h"

#include "HLSLMaterialUtilities.h"
#include "HLSLShaderLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"
#include "Materials/MaterialParameterCollection.h"
#include "ShaderGeneration/HLSLShader.h"

void FHLSLUniqueMetaTagHandler::SetupExpression(UMaterial* Material, int32 Index, auto* Expression)
{
	Expression->MaterialExpressionGuid = FGuid::NewGuid();
	Expression->bCollapsed = true;
	Expression->MaterialExpressionEditorX = 0;
	Expression->MaterialExpressionEditorY = 200 * Index;
		
	Material->FunctionExpressions.Add(Expression);
}

FString FHLSLUniqueMetaTag_Particles::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != EFunctionInputType::FunctionInput_Vector4)
	{
		return "Particles input requires a float4 type";
	}

	return "";
}

UMaterialExpression* FHLSLUniqueMetaTag_Particles::GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input,
                                                                            const FHLSLShaderInputMeta& MetaTag, int32 Index)
{
	UMaterial* Material = Library.Materials.Get();

	UMaterialExpressionDynamicParameter* Expression = NewObject<UMaterialExpressionDynamicParameter>(Material);
	SetupExpression(Material, Index, Expression);

	if (!Input.DefaultValue.IsEmpty())
		Expression->DefaultValue = FLinearColor(Input.DefaultValueVector);
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

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLUniqueMetaTag_ParameterCollection::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != FunctionInput_Scalar && InputType != FunctionInput_Vector4)
	{
		return "Parameter collection expects a float or float4, neither is provided";
	}

	// Verify that we have the parameter collection
	bool bSuccess = false;
	for (const UMaterialParameterCollection* Collection : Library.ParameterCollections)
	{
		if (bSuccess) break;
			
		if (!Collection) continue;

		if (InputType == EFunctionInputType::FunctionInput_Scalar)
		{
			for (const FCollectionScalarParameter& ScalarParam : Collection->ScalarParameters)
			{
				// Break upon success
				if (MetaData.Parameters[0].ToLower().Equals(ScalarParam.ParameterName.ToString().ToLower())) { bSuccess = true; break;}
			}
		}
		else if (InputType == EFunctionInputType::FunctionInput_Vector4)
		{
			for (const FCollectionVectorParameter& VectorParams : Collection->VectorParameters)
			{
				// Break upon success
				if (MetaData.Parameters[0].ToLower().Equals(VectorParams.ParameterName.ToString().ToLower())) { bSuccess = true; break;}
			}
		}
	}
		
	if (!bSuccess)
	{
		return FString::Printf(TEXT("ERROR: Parameter collection for type [%s] not found"), *MetaData.Parameters[0]);
	}

	return "";
}

UMaterialExpression* FHLSLUniqueMetaTag_ParameterCollection::GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input,
	const FHLSLShaderInputMeta& MetaTag, int32 Index)
{
	// We've already run the validation so we know the parameter collection we want exists, just gotta find it
	UMaterial* Material = Library.Materials.Get();

	UMaterialExpressionCollectionParameter* Expression = NewObject<UMaterialExpressionCollectionParameter>(Material);
	SetupExpression(Material, Index, Expression);
		
	UMaterialParameterCollection* TargetCollection = nullptr;;
	FName ParamName = NAME_None;
	for (UMaterialParameterCollection* Collection : Library.ParameterCollections)
	{
		if (!Collection) continue;

		if (Input.InputType == EFunctionInputType::FunctionInput_Scalar)
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
		else if (Input.InputType == EFunctionInputType::FunctionInput_Vector4)
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

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLUniqueMetaTag_LandscapeVisibility::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != EFunctionInputType::FunctionInput_Scalar)
	{
		return "Landscape visibility requires a float type";
	}
	return "";
}

UMaterialExpression* FHLSLUniqueMetaTag_LandscapeVisibility::GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input,
	const FHLSLShaderInputMeta& MetaTag, int32 Index)
{
	UMaterial* Material = Library.Materials.Get();
	UMaterialExpressionLandscapeVisibilityMask* Expression = NewObject<UMaterialExpressionLandscapeVisibilityMask>(Material);
	SetupExpression(Material, Index, Expression);
	return Expression;
}
