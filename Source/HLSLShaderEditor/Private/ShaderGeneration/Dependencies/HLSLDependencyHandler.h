#pragma once

#include "CoreMinimal.h"

/// @brief	Modular element base used for adding dependency nodes to get the UE material to compile. An example of this would be
///			when using texture coordinates, we have to plug in a dummy TexCoord node to ensure NUM_TEX_COORD_INTERPOLATORS is set correctly by UE
struct FHLSLDependencyHandler
{
	virtual ~FHLSLDependencyHandler() = default;

	virtual void EvaluateDependency(class UMaterial* Material, class UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const = 0;
};

struct FHLSLDependency_TexCoords : FHLSLDependencyHandler
{
	virtual void EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const override;
};

struct FHLSLDependency_SceneTexture : FHLSLDependencyHandler
{
	virtual void EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const override;
};

struct FHLSLDependency_VertexColors : FHLSLDependencyHandler
{
	virtual void EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const override;
};

struct FHLSLDependency_WPOExcludingOffsets : FHLSLDependencyHandler
{
	virtual void EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const override;
};

struct FHLSLDependency_SkyAtmosphere : FHLSLDependencyHandler
{
	virtual void EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode, const FString& ShaderBody) const override;
};
