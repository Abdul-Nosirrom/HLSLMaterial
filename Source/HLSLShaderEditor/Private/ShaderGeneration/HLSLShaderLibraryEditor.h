// Copyright Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "HLSLShaderParser.h"
#include "MaterialShared.h"

class UHLSLShaderLibrary;

class FHLSLShaderLibraryEditor
{
	friend class FAssetTypeActions_HLSLShaderLibrary;
	
public:
	static void Register();

	static TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLShaderLibrary& Library);
	static void Generate(UHLSLShaderLibrary& Library);

private:
	static FString GenerateMaterialForShader(UHLSLShaderLibrary& Library, const TArray<FHLSLShaderParser::FSetting>& MaterialSettings);
	static void GenerateMaterialInstanceForShader(UHLSLShaderLibrary& Library);

	static bool TryLoadFileToString(FString& Text, const FString& FullPath);

	static UObject* CreateAsset(FString AssetName, FString FolderPath, UClass* Class, FString& OutError);

	// Helpers
	static FString SetupMaterialSettings(UMaterial* Material, const TArray<FHLSLShaderParser::FSetting>& Settings);
	// ~

public:
	template<typename T>
	static T* CreateAsset(FString AssetName, FString FolderPath, FString& OutError)
	{
		return CastChecked<T>(CreateAsset(AssetName, FolderPath, T::StaticClass(), OutError), ECastCheckedType::NullAllowed);
	}
};