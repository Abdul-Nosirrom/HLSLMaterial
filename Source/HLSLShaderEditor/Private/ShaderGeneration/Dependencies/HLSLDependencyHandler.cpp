#include "HLSLDependencyHandler.h"

#include "HLSLMaterialUtilities.h"
#include "Internationalization/Regex.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"

#define SKYATMOSPHERE_ALLOWED 0 // Material expression for this is not exposed without an engine change, just need to add the minimalAPI flag to the UCLASS

#if SKYATMOSPHERE_ALLOWED
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
#endif

void FHLSLDependency_TexCoords::EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode,
                                                   const FString& ShaderBody) const
{
	// Create a dummy texture coordinate index to ensure NUM_TEX_COORD_INTERPOLATORS is correct
	// Detect used texture coordinates
	int32 MaxTexCoordinateUsed = -1;
	{
		FRegexPattern RegexPattern(R"_(Parameters.TexCoords\[([0-9]+)\])_");
		FRegexMatcher RegexMatcher(RegexPattern, ShaderBody);
		while (RegexMatcher.FindNext())
		{
			MaxTexCoordinateUsed = FMath::Max(MaxTexCoordinateUsed, FCString::Atoi(*RegexMatcher.GetCaptureGroup(1)));
		}
	}

	if (MaxTexCoordinateUsed == -1) return;
	
	UMaterialExpressionTextureCoordinate* TextureCoordinate = NewObject<UMaterialExpressionTextureCoordinate>(Material);
	TextureCoordinate->MaterialExpressionGuid = FGuid::NewGuid();
	TextureCoordinate->bCollapsed = true;
	TextureCoordinate->CoordinateIndex = MaxTexCoordinateUsed;
	TextureCoordinate->MaterialExpressionEditorX = HLSLNode->MaterialExpressionEditorX - 200;
	TextureCoordinate->MaterialExpressionEditorY = HLSLNode->MaterialExpressionEditorY;
	Material->FunctionExpressions.Add(TextureCoordinate);

	FCustomInput& CustomInput = HLSLNode->Inputs.Emplace_GetRef();
	CustomInput.InputName = "DUMMY_COORDINATE_INPUT";
	CustomInput.Input.Connect(0, TextureCoordinate);
}

void FHLSLDependency_SceneTexture::EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode,
	const FString& ShaderBody) const
{
	// Detect used scene texture look-up
	const bool bSceneTextureUsed = ShaderBody.Contains("SceneTextureLookup", ESearchCase::CaseSensitive);
	if (!bSceneTextureUsed) return;
	
	UMaterialExpressionSceneTexture* SceneTexture = NewObject<UMaterialExpressionSceneTexture>(Material);
	SceneTexture->MaterialExpressionGuid = FGuid::NewGuid();
	SceneTexture->bCollapsed = true;
	SceneTexture->SceneTextureId = ESceneTextureId::PPI_PostProcessInput0;
	SceneTexture->MaterialExpressionEditorX = HLSLNode->MaterialExpressionEditorX - 200;
	SceneTexture->MaterialExpressionEditorY = HLSLNode->MaterialExpressionEditorY;
	Material->FunctionExpressions.Add(SceneTexture);

	FCustomInput& CustomInput = HLSLNode->Inputs.Emplace_GetRef();
	CustomInput.InputName = "DUMMY_SCENETEX_INPUT";
	CustomInput.Input.Connect(0, SceneTexture);
}

void FHLSLDependency_VertexColors::EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode,
	const FString& ShaderBody) const
{
	// Create a dummy vertex color parameter to ensure INTERPOLATE_VERTEX_COLOR is correct
	// Detect used vertex colors
	const bool bVertexColorUsed = ShaderBody.Contains("Parameters.VertexColor", ESearchCase::CaseSensitive);
	if (!bVertexColorUsed) return;
	
	UMaterialExpressionVertexColor* Color = NewObject<UMaterialExpressionVertexColor>(Material);
	Color->MaterialExpressionGuid = FGuid::NewGuid();
	Color->bCollapsed = true;
	Color->MaterialExpressionEditorX = HLSLNode->MaterialExpressionEditorX - 200;
	Color->MaterialExpressionEditorY = HLSLNode->MaterialExpressionEditorY;
	Material->FunctionExpressions.Add(Color);

	FCustomInput& CustomInput = HLSLNode->Inputs.Emplace_GetRef();
	CustomInput.InputName = "DUMMY_COLOR_INPUT";
	CustomInput.Input.Connect(0, Color);
}

void FHLSLDependency_WPOExcludingOffsets::EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode,
	const FString& ShaderBody) const
{
	// Create a dummy world position node to ensure NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS is correct
	// Detect whether NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS is required
	const bool bNeedsWorldPositionExcludingShaderOffsets = ShaderBody.Contains("GetWorldPosition_NoMaterialOffsets", ESearchCase::CaseSensitive);
	if (!bNeedsWorldPositionExcludingShaderOffsets) return;
	
	UMaterialExpressionWorldPosition* WorldPosition = NewObject<UMaterialExpressionWorldPosition>(Material);
	WorldPosition->MaterialExpressionGuid = FGuid::NewGuid();
	WorldPosition->bCollapsed = true;
	WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
	WorldPosition->MaterialExpressionEditorX = HLSLNode->MaterialExpressionEditorX - 200;
	WorldPosition->MaterialExpressionEditorY = HLSLNode->MaterialExpressionEditorY;
	Material->FunctionExpressions.Add(WorldPosition);

	FCustomInput& CustomInput = HLSLNode->Inputs.Emplace_GetRef();
	CustomInput.InputName = "DUMMY_WORLD_POSITION_INPUT";
	CustomInput.Input.Connect(0, WorldPosition);
}

void FHLSLDependency_SkyAtmosphere::EvaluateDependency(UMaterial* Material, UMaterialExpressionCustom* HLSLNode,
	const FString& ShaderBody) const
{
#if SKYATMOSPHERE_ALLOWED
	// Detect if any sky atmosphere expressions are used
	const bool bSkyAtmosphereUsed = ShaderBody.Contains("MaterialExpressionSkyAtmosphere", ESearchCase::CaseSensitive);
	if (!bSkyAtmosphereUsed) return;
	
	UMaterialExpressionSkyAtmosphereLightIlluminance* SkyAtmosphere = NewObject<UMaterialExpressionSkyAtmosphereLightIlluminance>(Material);
	SkyAtmosphere->MaterialExpressionGuid = FGuid::NewGuid();
	SkyAtmosphere->bCollapsed = true;
	SkyAtmosphere->MaterialExpressionEditorX = HLSLNode->MaterialExpressionEditorX - 200;
	SkyAtmosphere->MaterialExpressionEditorY = HLSLNode->MaterialExpressionEditorY;
	Material->FunctionExpressions.Add(SkyAtmosphere);

	FCustomInput& CustomInput = HLSLNode->Inputs.Emplace_GetRef();
	CustomInput.InputName = "DUMMY_SKYATMOSPHERE_INPUT";
	CustomInput.Input.Connect(0, SkyAtmosphere);
#endif
}
