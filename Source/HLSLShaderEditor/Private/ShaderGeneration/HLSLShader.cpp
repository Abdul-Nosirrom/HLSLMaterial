// Fill out your copyright notice in the Description page of Project Settings.


#include "HLSLShader.h"

#include "HLSLMaterialUtilities.h"
#include "HLSLShaderGenerator.h"
#include "SceneTypes.h"
#include "Internationalization/Regex.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "HLSLShaderLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "MetaTags/HLSLMetaTagHandler.h"

FString FHLSLShaderOutput::ParseTypeAndSemantic(FHLSLShaderOutput& Output)
{
	Output.OutputType = CMOT_Float1;
	const FString Semantic = Output.Semantic.ToLower();
	
	{
		if (Semantic == "basecolor")
		{
			Output.OutputProperty = MP_BaseColor;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "metallic")
		{
			Output.OutputProperty = MP_Metallic;
		}
		else if (Semantic == "specular")
		{
			Output.OutputProperty = MP_Specular;
		}
		else if (Semantic == "roughness")
		{
			Output.OutputProperty = MP_Roughness;
		}
		else if (Semantic == "anisotropy")
		{
			Output.OutputProperty = MP_Anisotropy;
		}
		else if (Semantic == "emissive")
		{
			Output.OutputProperty = MP_EmissiveColor;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "opacity")
		{
			Output.OutputProperty = MP_Opacity;
		}
		else if (Semantic == "opacitymask")
		{
			Output.OutputProperty = MP_OpacityMask;
		}
		else if (Semantic == "normal")
		{
			Output.OutputProperty = MP_Normal;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "tangent")
		{
			Output.OutputProperty = MP_Tangent;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "vertexoffset")
		{
			Output.OutputProperty = MP_WorldPositionOffset;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "subsurface")
		{
			Output.OutputProperty = MP_SubsurfaceColor;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "refraction")
		{
			Output.OutputProperty = MP_Refraction;
			Output.OutputType = CMOT_Float3;
		}
		else if (Semantic == "pixeldepthoffset")
		{
			Output.OutputProperty = MP_PixelDepthOffset;
		}
		else
			return "Failed to parse semantic: " + Output.Semantic;
	}

	if ((Output.Type != "float" || Output.Type != "float3") || (Output.Type == "float3" && Output.OutputType != CMOT_Float3))
	{
		//return "Invalid types on output parameters: " + Output.Type; 
	}

	return "";
}

FString FHLSLMaterialShader::GenerateHashedString(const FString& BaseHash) const
{
	FString StringToHash =
	BaseHash + " " +
	// Changes too often
	//FString::FromInt(StartLine) + " " +
	InputStruct_Raw.Body + " " +
	OutputStruct_Raw.Body + " " +
	ReturnType + " " +
	Name + "(" +
	FString::Join(Arguments, TEXT(",")) + ")" +
	Body;

	StringToHash.ReplaceInline(TEXT("\t"), TEXT(" "));
	StringToHash.ReplaceInline(TEXT("\n"), TEXT(" "));

	while (StringToHash.ReplaceInline(TEXT("  "), TEXT(" "))) {}

	return "HLSL Hash: " + FHLSLMaterialUtilities::HashString(StringToHash);
}


FString FHLSLShaderInput::ParseMetaAndDefault(const UHLSLShaderLibrary& Library, FHLSLShaderInput& Input)
{
	const FString DefaultValueError = Input.Name + ": invalid default value for type " + Input.Type + ": " + Input.DefaultValue;

	{
		if (Input.Type == "bool")
		{
			Input.InputType = FunctionInput_StaticBool;

			if (!Input.DefaultValue.IsEmpty())
			{
				if (Input.DefaultValue == "true")
				{
					Input.bDefaultValueBool = true;
				}
				else if (Input.DefaultValue == "false")
				{
					Input.bDefaultValueBool = false;
				}
				else
				{
					return DefaultValueError;
				}
			}
		}
		else if (Input.Type == "int" || Input.Type == "uint")
		{
			Input.InputType = FunctionInput_Scalar;

			if (!Input.DefaultValue.IsEmpty() && !ParseDefaultValue(Input.DefaultValue, 1, Input.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "float")
		{
			Input.InputType = FunctionInput_Scalar;

			if (!Input.DefaultValue.IsEmpty() && !ParseDefaultValue(Input.DefaultValue, 1, Input.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "float2")
		{
			Input.InputType = FunctionInput_Vector2;

			if (!Input.DefaultValue.IsEmpty() && !ParseDefaultValue(Input.DefaultValue, 2, Input.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "float3")
		{
			Input.InputType = FunctionInput_Vector3;

			if (!Input.DefaultValue.IsEmpty() && !ParseDefaultValue(Input.DefaultValue, 3, Input.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "float4")
		{
			Input.InputType = FunctionInput_Vector4;

			if (!Input.DefaultValue.IsEmpty() && !ParseDefaultValue(Input.DefaultValue, 4, Input.DefaultValueVector))
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "Texture2D")
		{
			Input.InputType = FunctionInput_Texture2D;

			if (!Input.DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "TextureCube")
		{
			Input.InputType = FunctionInput_TextureCube;

			if (!Input.DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "Texture2DArray")
		{
			Input.InputType = FunctionInput_Texture2DArray;

			if (!Input.DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "TextureExternal")
		{
			Input.InputType = FunctionInput_TextureExternal;

			if (!Input.DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else if (Input.Type == "Texture3D")
		{
			Input.InputType = FunctionInput_VolumeTexture;

			if (!Input.DefaultValue.IsEmpty())
			{
				return DefaultValueError;
			}
		}
		else
		{
			return "Invalid argument type: " + Input.Type;
		}
	}

	FString Error = FHLSLShaderInputMeta::GetMetaDataFromString(Library, Input.MetaStringRaw, Input.InputType, Input.Meta);
	return Error;
}

bool FHLSLShaderInput::ParseDefaultValue(const FString& DefaultValue, int32 Dimension, FVector4& OutValue)
{
	const FString FloatPattern = R"_(\s*(-?\s*\+?\s*(?:[0-9]*[.])?[0-9]*)f?\s*)_";

	const auto TryParseFloat = [&](const FString& String, auto& OutFloatValue)
	{
		FRegexPattern RegexPattern("^" + FloatPattern + "$");
		FRegexMatcher RegexMatcher(RegexPattern, String);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutFloatValue = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		return true;
	};

	if (Dimension == 1)
	{
		return TryParseFloat(DefaultValue, OutValue.X);
	}
	else if (Dimension == 2)
	{
		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue = FVector4(SingleValue);
			return true;
		}

		FRegexPattern RegexPattern("^float2\\("+ FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));

		return true;
	}
	else if (Dimension == 3)
	{
		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue = FVector4(SingleValue);
			return true;
		}

		FRegexPattern RegexPattern("^float3\\("+ FloatPattern + "," + FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));
		OutValue.Z = FCString::Atof(*RegexMatcher.GetCaptureGroup(3));

		return true;
	}
	else
	{
		check(Dimension == 4);

		float SingleValue;
		if (TryParseFloat(DefaultValue, SingleValue))
		{
			OutValue = FVector4(SingleValue);
			return true;
		}

		FRegexPattern RegexPattern("^float4\\("+ FloatPattern + "," + FloatPattern + "," + FloatPattern + "," + FloatPattern + "\\)$");
		FRegexMatcher RegexMatcher(RegexPattern, DefaultValue);
		if (!RegexMatcher.FindNext())
		{
			return false;
		}

		OutValue.X = FCString::Atof(*RegexMatcher.GetCaptureGroup(1));
		OutValue.Y = FCString::Atof(*RegexMatcher.GetCaptureGroup(2));
		OutValue.Z = FCString::Atof(*RegexMatcher.GetCaptureGroup(3));
		OutValue.W = FCString::Atof(*RegexMatcher.GetCaptureGroup(4));

		return true;
	}
}


FString FHLSLShaderInputMeta::GetMetaDataFromString(const UHLSLShaderLibrary& Library, FString& MetaString, EFunctionInputType InputType, TArray<FHLSLShaderInputMeta>& MetaData)
{
	if (MetaString.IsEmpty()) return "";

	enum class EScope
	{
		Tag,
		Parameter
	};

	EScope Scope = EScope::Tag;
	int32 ParenthesisDepth = 0;
	int32 Index = 0;

	// Simplify line breaks handling
	MetaString.ReplaceInline(TEXT("\r\n"), TEXT(""));
	MetaString.ReplaceInline(TEXT("\n"), TEXT(""));

	while (Index < MetaString.Len())
	{
		const TCHAR Char = MetaString[Index++];

		switch (Scope)
		{
		case EScope::Tag:
		{
			ensure(ParenthesisDepth == 0);

			Index--;

			FString Token;
			for (; Index < MetaString.Len(); Index++)
			{
				const TCHAR TokenChar = MetaString[Index];
				if (FChar::IsWhitespace(TokenChar) || FChar::IsLinebreak(TokenChar) || TokenChar == TEXT(',')) continue;
				if (TokenChar == TEXT('('))
				{
					ParenthesisDepth++;
					Scope = EScope::Parameter;
					Index++;
					break;
				}
				Token += MetaString[Index];
			}

			MetaData.Emplace();
			MetaData.Last().Tag = Token;
			
			break;
		}
		case EScope::Parameter:
		{
			// Check if parameters are initialized to empty
			if (MetaData.Last().Parameters.IsEmpty())
			{
				MetaData.Last().Parameters.Emplace();
			}

			// Commas at scope == 1 imply a new parameter to the current meta tag
			if (Char == TEXT(',') && ParenthesisDepth == 1)
			{
				MetaData.Last().Parameters.Emplace();
				continue;
			}

			// Nested scopes can exist within parameters, so be sure to increment the depth so we know when a comma means a new parameter or just part of the current one
			if (Char == TEXT('(')) ParenthesisDepth++;
			if (Char == TEXT(')')) ParenthesisDepth--;

			// We finished up, meaning we encountered a closing brace so go back to other state
			if (ParenthesisDepth == 0)
			{
				Scope = EScope::Tag;
				continue;
			}

			// Dont add parenthesis for convenience later
			if (Char != TEXT('(') && Char != TEXT(')'))
				MetaData.Last().Parameters.Last() += Char;

			break;
		}
		}
	}

	if (Scope == EScope::Parameter)
	{
		return "Failed to parse meta tag [" + MetaString + "]";
	}
	
	for (const auto& Meta : MetaData)
	{
		FString Error =	VerifyMetaData(Library, MetaData.Num(), InputType, Meta);
		if (!Error.IsEmpty()) return Error;
	}

	return ""; 
}

FString FHLSLShaderInputMeta::VerifyMetaData(const UHLSLShaderLibrary& Library, int NumTags, EFunctionInputType AssociatedInput,
	const FHLSLShaderInputMeta& MetaData)
{
	const FString Tag = MetaData.Tag.ToLower();

	const bool bIsRegularTag = FHLSLShaderGenerator::MetaTagStructMap.Contains(Tag);
	const bool bIsUniqueTag = FHLSLShaderGenerator::UniqueMetaTagStructMap.Contains(Tag);
	// Do we support this meta tag?
	if (!bIsUniqueTag && !bIsRegularTag) return FString::Printf(TEXT("%s: Given meta tag not supported!"), *Tag);
	// If its a unique tag, are there other tags with this parameter
	if (NumTags > 1 && bIsUniqueTag) return FString::Printf(TEXT("%s: Unique expression tag should be the only tag on a parameter!"), *Tag);
	// Verify that the number of inputs to this tag are valid
	const TArray<int> ParameterCount = bIsRegularTag ? FHLSLShaderGenerator::MetaTagStructMap[Tag]->GetNumParameters() : FHLSLShaderGenerator::UniqueMetaTagStructMap[Tag]->GetNumParameters();
	if (!ParameterCount.Contains(MetaData.Parameters.Num())) return FString::Printf(TEXT("%s: Invalid parameters count"), *Tag);

	// Now we can run each specific validation script
	if (bIsRegularTag) return FHLSLShaderGenerator::MetaTagStructMap[Tag]->Validate(Library, AssociatedInput, MetaData);
	if (bIsUniqueTag) return FHLSLShaderGenerator::UniqueMetaTagStructMap[Tag]->Validate(Library, AssociatedInput, MetaData);

	checkf(false, TEXT("Something went terribly wrong..."));

	return "";
}
