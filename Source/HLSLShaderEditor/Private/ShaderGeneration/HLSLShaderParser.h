// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FHLSLMaterialShader;
struct FHLSLStruct;
struct FHLSLShaderInput;
struct FHLSLShaderOutput;
class UHLSLShaderLibrary;

class FHLSLShaderParser
{
public:
	// Aim of this parser is to return all function bodies with the function names, and all struct names and struct bodies
	static FString Parse(
		const UHLSLShaderLibrary& Library, 
		FString Text, 
		TArray<FHLSLMaterialShader>& OutFunctions,
		TArray<FHLSLStruct>& OutStructs);

	struct FInclude
	{
		FString VirtualPath;
		FString DiskPath;
	};
	
	struct FSetting
	{
		FString Setting;
		FString Value;
	};

	struct FPragmaDeclarations
	{
		FString Type;
		FString Name;
	};
	
	static TArray<FInclude> GetIncludes(const FString& FilePath, const FString& Text);
	static TArray<FSetting> GetSettings(const FString& Text);
	static TArray<FPragmaDeclarations> GetPragmaDeclarations(const FString& Text);

	static FString ParseInputStructs(const UHLSLShaderLibrary& Library, const FHLSLStruct& Struct, TArray<FHLSLShaderInput>& Inputs);
	static FString ParseOutputStructs(const UHLSLShaderLibrary& Library, const FHLSLStruct& Struct, TArray<FHLSLShaderOutput>& Outputs);
};
