#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UHLSLShaderLibrary;
struct FHLSLShaderInputMeta;
struct FHLSLShaderInput;
enum EFunctionInputType : int;

struct FHLSLMetaTagHandler
{
	FHLSLMetaTagHandler() = default;
	virtual ~FHLSLMetaTagHandler() = default;

	/// @brief	Number of parameters this meta tag supports [E.g Range(0, 5) tag supports 2 parameters, min/max, some may support 1 or 2 optional parameters]
	virtual TArray<int> GetNumParameters() const = 0;

	/// @brief	Validate whether everything is looking good [e.g Input Type works out for this]. Return an error string.
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType, const FHLSLShaderInputMeta& MetaData) const = 0;
	
	/// @brief	Given a material expression, based on what this meta tag should do, handles setting it up. We CANNOT fail at this point lest we break the material.
	virtual void SetupExpressionMetaTag(class UMaterialExpression* Expression, const FHLSLShaderInputMeta& MetaTag) = 0;
};

struct FHLSLUniqueMetaTagHandler
{
	FHLSLUniqueMetaTagHandler() = default;
	virtual ~FHLSLUniqueMetaTagHandler() = default;

	static void SetupExpression(UMaterial* Material, int32 Index, auto* Expression);

	/// @brief	Number of parameters this meta tag supports [E.g Range(0, 5) tag supports 2 parameters, min/max, some may support 1 or 2 optional parameters]
	virtual TArray<int> GetNumParameters() const = 0;

	/// @brief	Validate whether everything is looking good [e.g Input Type works out for this]. Return an error string.
	virtual FString Validate(const UHLSLShaderLibrary& Library, EFunctionInputType InputType, const FHLSLShaderInputMeta& MetaData) const = 0;
	
	/// @brief	Generate the unique expression associated with this meta tag [Particles/ParameterCollections/etc...]
	virtual class UMaterialExpression* GenerateUniqueExpression(UHLSLShaderLibrary& Library, const FHLSLShaderInput& Input, const FHLSLShaderInputMeta& MetaTag, int32 Index) = 0;
};
