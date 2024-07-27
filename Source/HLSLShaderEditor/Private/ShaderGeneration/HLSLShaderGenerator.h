// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MaterialShader.h"

class UHLSLShaderLibrary;
class UMaterialExpressionParameter;
struct FHLSLShaderInput;
struct FHLSLMaterialShader;
class IMaterialEditor;

class FHLSLShaderGenerator
{
public:
	static FString GenerateShader(
		UHLSLShaderLibrary& Library,
		const TArray<FString>& IncludeFilePaths,
		FHLSLMaterialShader Shader,
		const TMap<FName, FGuid>& ParameterGuids);

	static FString GenerateFunctionCode(const UHLSLShaderLibrary& Library, const FHLSLMaterialShader& Function, const FString& Declarations);
	static IMaterialEditor* FindMaterialEditorForAsset(UObject* InAsset);
};
