// Copyright 2023 CoC All rights reserved

#include "HLSLDefaultParameterMetaTags.h"

#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "ShaderGeneration/HLSLShader.h"
#include "ShaderGeneration/HLSLShaderMessages.h"

FString FHLSLMetaTag_Groups::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
                                      const FHLSLShaderInputMeta& MetaData) const
{
	return "";
}

void FHLSLMetaTag_Groups::SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag)
{
	// We're expecting either a ParameterExpression or TextureParameterExpression
	if (auto RegParam = Cast<UMaterialExpressionParameter>(Expression))
	{
		RegParam->Group = FName(MetaTag.Parameters[0]);
	}
	else if (auto TexParam = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
	{
		TexParam->Group = FName(MetaTag.Parameters[0]);
	}
	else checkf(false, TEXT("Invalid expression passed to group meta tag!"));
}

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLMetaTag_Channels::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != FunctionInput_Vector2 && InputType != FunctionInput_Vector3 && InputType != FunctionInput_Vector4)
	{
		return "Invalid parameter type with the use of Channels meta tag! Require a vector2/3/4.";
	}
	return "";
}

void FHLSLMetaTag_Channels::SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag)
{
	auto ParamExpression = Cast<UMaterialExpressionVectorParameter>(Expression);
	checkf(ParamExpression, TEXT("Invalid expression passed to channels meta tag!"));

	if (ParamExpression)
	{
		TArray<FString> ChannelNames;
		const int32 NumNames = MetaTag.Parameters[0].ParseIntoArrayWS(ChannelNames, TEXT(","));
		FParameterChannelNames InputChannelNames;
		InputChannelNames.R = NumNames > 0 ? FText::FromString(ChannelNames[0]) : FText();
		InputChannelNames.G = NumNames > 1 ? FText::FromString(ChannelNames[1]) : FText();
		InputChannelNames.B = NumNames > 2 ? FText::FromString(ChannelNames[2]) : FText();
		InputChannelNames.A = NumNames > 3 ? FText::FromString(ChannelNames[3]) : FText();
		ParamExpression->ChannelNames = InputChannelNames;
	}
}

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLMetaTag_PrimitiveData::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != FunctionInput_Scalar && InputType != FunctionInput_Vector2 && InputType != FunctionInput_Vector3 && InputType != FunctionInput_Vector4)
	{
		return "Invalid parameter type for parameter that is meant to be used as custom primitive data";
	}
	return "";
}

void FHLSLMetaTag_PrimitiveData::SetupExpressionMetaTag(UMaterialExpression* Expression,
	const FHLSLShaderInputMeta& MetaTag)
{
	const uint8 PrimitiveIdx = FCString::Strtoui64(*MetaTag.Parameters[0], NULL, 10);
	if (auto* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		ScalarParam->bUseCustomPrimitiveData = true;
		ScalarParam->PrimitiveDataIndex = PrimitiveIdx;
	}
	else if (auto* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		VectorParam->bUseCustomPrimitiveData = true;
		VectorParam->PrimitiveDataIndex = PrimitiveIdx;
	}
	else checkf(false, TEXT("Invalid expression passed to primitive data meta tag!"));
}

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLMetaTag_Range::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != FunctionInput_Scalar)
	{
		return "Range metatag expects a scalar parameter";
	}
	return "";
}

void FHLSLMetaTag_Range::SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag)
{
	if (auto* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		ScalarParam->SliderMin = FCString::Atof(ToCStr(MetaTag.Parameters[0]));
		ScalarParam->SliderMax = FCString::Atof(ToCStr(MetaTag.Parameters[1]));
	}
	else checkf(false, TEXT("Invalid expression passed to range meta tag!"));
}

////////////////////////////////////////////////////
////////////////////////////////////////////////////
////////////////////////////////////////////////////

FString FHLSLMetaTag_SamplerType::Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
	const FHLSLShaderInputMeta& MetaData) const
{
	if (InputType != FunctionInput_Texture2D)
	{
		return "Sampler type meta tag is only valid on texture inputs";
	}
	return "";
}

void FHLSLMetaTag_SamplerType::SetupExpressionMetaTag(UMaterialExpression* Expression,
	const FHLSLShaderInputMeta& MetaTag)
{
	static const TMap<FString, EMaterialSamplerType> SamplerTypeMap =
	{
		{"color", SAMPLERTYPE_Color}, {"grayscale", SAMPLERTYPE_Grayscale}, {"alpha", SAMPLERTYPE_Alpha},
		{"normal", SAMPLERTYPE_Normal}, {"masks", SAMPLERTYPE_Masks}, {"linear_color", SAMPLERTYPE_LinearColor},
		{"linear_grayscale", SAMPLERTYPE_LinearGrayscale}
	};

	auto TexParam = Cast<UMaterialExpressionTextureObjectParameter>(Expression);

	if (TexParam)
	{
		if (SamplerTypeMap.Contains(MetaTag.Parameters[0]))
			TexParam->SamplerType = SamplerTypeMap[MetaTag.Parameters[0]];
		else FHLSLShaderMessages::ShowError(TEXT("ERROR: Unregonized Sampler Type [%s] for texture parameter"), *MetaTag.Parameters[0]);
	}
	else checkf(false, TEXT("Invalid expression passed to sampler type meta tag!"));
}
