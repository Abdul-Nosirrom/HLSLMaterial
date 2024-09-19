// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UHLSLShaderLibrary;
class UMaterialExpressionParameter;
struct FHLSLShaderInput;
struct FHLSLMaterialShader;
class IMaterialEditor;

// NOTE: make this or libraryeditor an editor subsystem? libraryeditor makes sense since it deals with creating and managing the material asset...
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

	// All the modular pieces for generating materials
	static TMap<FString, TUniquePtr<struct FHLSLMetaTagHandler>> MetaTagStructMap;
	static TMap<FString, TUniquePtr<struct FHLSLUniqueMetaTagHandler>> UniqueMetaTagStructMap;
	static TArray<TUniquePtr<struct FHLSLDependencyHandler>> DependencyHandlers;
	static void Initialize();
};
