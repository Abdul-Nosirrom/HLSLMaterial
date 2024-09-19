// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"

enum EMaterialProperty : int;
enum EFunctionInputType : int;
enum ECustomMaterialOutputType : int;
class UHLSLShaderLibrary;
class UMaterialExpression;
class UMaterialExpressionParameter;
class UMaterialExpressionTextureObjectParameter;
class UMaterial;

struct FHLSLShaderInputMetaParameter
{
	TArray<FString> StringValues;
	TArray<int> IntValues;
	TArray<float> FloatValues;
};

struct FHLSLShaderInputMeta
{
	FString Tag;
	TArray<FString> Parameters;

	static FString GetMetaDataFromString(const UHLSLShaderLibrary& Library, FString& MetaString, EFunctionInputType AssociatedInput, TArray<FHLSLShaderInputMeta>& MetaData);
	static FString VerifyMetaData(const UHLSLShaderLibrary& Library, int NumTags, EFunctionInputType AssociatedInput, const FHLSLShaderInputMeta& MetaData);
};


struct FHLSLShaderInput
{
	// Filled in based on the struct (PSInput,VSInput,NormInput)
	FString ShaderStage = "";

	FString MetaStringRaw;
	TArray<FHLSLShaderInputMeta> Meta;
	
	FString Type;
	FString Name;
	FString DefaultValue;

	bool bIsConst;

	EFunctionInputType InputType;
	bool bDefaultValueBool = false;
	FVector4 DefaultValueVector{ForceInit};

	static FString ParseMetaAndDefault(const UHLSLShaderLibrary& Library, FHLSLShaderInput& Input);
	static bool ParseDefaultValue(const FString& DefaultValue, int32 Dimension, FVector4& OutValue);
	
	UMaterialExpression* GetInputExpression(UHLSLShaderLibrary& Library, const FGuid ParamGUID, int32 Index, const FString& BasePath) const;
	void SetupParameterMetaTags(UMaterialExpression* Parameter) const;

	UClass* GetBranchExpressionClass(bool& bRequiresBoolInput, int32& TrueIdx, int32& FalseIdx) const;
};

struct FHLSLShaderOutput
{
	// Filled in based on the struct (PSOutput, VSOutput, NormOutput)
	FString ShaderStage = "";
	
	FString Type;
	FString Name;
	FString Semantic;

	EMaterialProperty OutputProperty;
	ECustomMaterialOutputType OutputType;

	static FString ParseTypeAndSemantic(FHLSLShaderOutput& Output);
};

// Container for generic structs before we process them into inputs/outputs. Could just be some code structs
struct FHLSLStruct
{
	FString ShaderStage = "";
	
	FString Name;
	FString Body;
};

struct FHLSLMaterialShader
{
	int32 StartLine = 0;

	FString ShaderStage = "";

	FString ReturnType;
	FString Name;
	TArray<FString> Arguments;
	TArray<FHLSLShaderInput> Inputs; // Processed post parsing from input struct
	TArray<FHLSLShaderOutput> Outputs; // Processed post parsing from output struct
	FString Body;

	FHLSLStruct InputStruct_Raw;
	FHLSLStruct OutputStruct_Raw;

	FString HashedString;

	FString GenerateHashedString(const FString& BaseHash) const;

	// Definitions
	inline static FString VERTEX_SHADER = "vert";
	inline static FString PIXEL_SHADER = "fragment";
	inline static FString NORMAL_SHADER = "normal";

	inline static FString VS_INPUT = "vsin";
	inline static FString PS_INPUT = "psin";
	inline static FString PS_OUTPUT = "psout";
	inline static FString NORM_INPUT = "nin";

	inline static TArray<FString> PRAGMA_DEFS = {VERTEX_SHADER, PIXEL_SHADER, NORMAL_SHADER, VS_INPUT, PS_INPUT, PS_OUTPUT, NORM_INPUT};
	inline static TArray<FString> FUNC_NAMES = {VERTEX_SHADER, PIXEL_SHADER, NORMAL_SHADER};
	inline static TArray<FString> INPUT_NAMES = {VS_INPUT, PS_INPUT, NORM_INPUT};
	inline static TArray<FString> OUTPUT_NAMES = {PS_OUTPUT};

	static FString GetShaderStageFromInputOrOutput(const FString& InOutTag)
	{
		if (InOutTag.Equals(VS_INPUT)) return VERTEX_SHADER;
		if (InOutTag.Equals(PS_INPUT)) return PIXEL_SHADER;
		if (InOutTag.Equals(NORM_INPUT)) return NORMAL_SHADER;
		
		if (InOutTag.Equals(PS_OUTPUT)) return PIXEL_SHADER;

		ensure(false);
		return "";
	}
};