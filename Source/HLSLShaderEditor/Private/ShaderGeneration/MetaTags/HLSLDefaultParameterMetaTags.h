#pragma once
#include "HLSLMetaTagHandler.h"
#include "CoreMinimal.h"

struct FHLSLMetaTag_Groups : FHLSLMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
		const FHLSLShaderInputMeta& MetaData) const override;
	virtual void SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) override;
};

struct FHLSLMetaTag_Channels : FHLSLMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
		const FHLSLShaderInputMeta& MetaData) const override;
	virtual void SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) override;
};

struct FHLSLMetaTag_PrimitiveData : FHLSLMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
		const FHLSLShaderInputMeta& MetaData) const override;
	virtual void SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) override;
};

struct FHLSLMetaTag_Range : FHLSLMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {2}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
		const FHLSLShaderInputMeta& MetaData) const override;
	virtual void SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) override;
};

struct FHLSLMetaTag_SamplerType : FHLSLMetaTagHandler
{
	virtual TArray<int> GetNumParameters() const override { return {1}; }
	
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType,
		const FHLSLShaderInputMeta& MetaData) const override;
	virtual void SetupExpressionMetaTag(UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) override;
};