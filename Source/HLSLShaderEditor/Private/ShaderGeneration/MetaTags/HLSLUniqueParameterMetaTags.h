// Copyright 2023 CoC All rights reserved

#pragma once
#include "CoreMinimal.h"
#include "HLSLMetaTagHandler.h"

struct FHLSLUniqueMetaTag_Particles : FHLSLUniqueMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1, 2}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType, const FHLSLShaderInputMeta& MetaData) const override;
	virtual UMaterialExpression* GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input,
		const FHLSLShaderInputMeta& MetaTag, int32 Index) override;
};

struct FHLSLUniqueMetaTag_ParameterCollection : FHLSLUniqueMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType, const FHLSLShaderInputMeta& MetaData) const override;
	virtual UMaterialExpression* GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input, 
		const FHLSLShaderInputMeta& MetaTag, int32 Index) override;
};

struct FHLSLUniqueMetaTag_LandscapeVisibility : FHLSLUniqueMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType, const FHLSLShaderInputMeta& MetaData) const override;
	virtual UMaterialExpression* GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input,
		const FHLSLShaderInputMeta& MetaTag, int32 Index) override;
};
