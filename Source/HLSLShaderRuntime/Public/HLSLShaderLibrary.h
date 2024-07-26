// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HLSLShaderLibrary.generated.h"

enum EMaterialProperty : int;
enum ECustomMaterialOutputType : int;
enum EFunctionInputType : int;
class UMaterial;
class UHLSLShaderLibrary;
class UMaterialParameterCollection;
class UHLSLMaterialFunctionLibrary;

#if WITH_EDITOR
class HLSLSHADERRUNTIME_API IHLSLShaderEditorInterface
{
public:
	virtual ~IHLSLShaderEditorInterface() = default;

	virtual TSharedRef<FVirtualDestructor> CreateWatcher(UHLSLShaderLibrary& Library) = 0;
	virtual void Update(UHLSLShaderLibrary& Library) = 0;

public:
	static IHLSLShaderEditorInterface* Get()
	{
		return StaticInterface;
	}

private:
	static IHLSLShaderEditorInterface* StaticInterface;

	friend class FHLSLShaderLibraryEditor;
};
#endif

USTRUCT()
struct FHLSLShaderInputMeta_Debug
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FString Tag;
	UPROPERTY(VisibleAnywhere)
	TArray<FString> Parameters;
};

USTRUCT()
struct FHLSLShaderInput_Debug
{
	GENERATED_BODY()
	
	// Filled in based on the struct (PSInput,VSInput,NormInput)
	UPROPERTY(VisibleAnywhere)
	FString ShaderStage = "";

	UPROPERTY(VisibleAnywhere)
	FString MetaStringRaw;
	UPROPERTY(VisibleAnywhere)
	TArray<FHLSLShaderInputMeta_Debug> Meta;

	UPROPERTY(VisibleAnywhere)
	FString Type;
	UPROPERTY(VisibleAnywhere)
	FString Name;
	UPROPERTY(VisibleAnywhere)
	FString DefaultValue;

	UPROPERTY(VisibleAnywhere)
	bool bIsConst;

	UPROPERTY(VisibleAnywhere)
	TEnumAsByte<EFunctionInputType> InputType;
	UPROPERTY(VisibleAnywhere)
	bool bDefaultValueBool = false;
	UPROPERTY(VisibleAnywhere)
	FVector4 DefaultValueVector{ForceInit};
};

USTRUCT()
struct FHLSLShaderOutput_Debug
{
	GENERATED_BODY()
	
	// Filled in based on the struct (PSOutput, VSOutput, NormOutput)
	UPROPERTY(VisibleAnywhere)
	FString ShaderStage = "";

	UPROPERTY(VisibleAnywhere)
	TEnumAsByte<EMaterialProperty> OutputProperty;
	UPROPERTY(VisibleAnywhere)
	TEnumAsByte<ECustomMaterialOutputType> OutputType;
	
	UPROPERTY(VisibleAnywhere)
	FString Type;
	UPROPERTY(VisibleAnywhere)
	FString Name;
	UPROPERTY(VisibleAnywhere)
	FString Semantic;
};

USTRUCT()
struct FHLSLShaderResults
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FString ShaderStage;

	UPROPERTY(VisibleAnywhere)
	FString Name;
	UPROPERTY(VisibleAnywhere)
	FString ReturnType;

	UPROPERTY(VisibleAnywhere)
	TArray<FString> Arguments;

	UPROPERTY(VisibleAnywhere, meta=(MultiLine=true))
	FString Body;

	UPROPERTY(VisibleAnywhere)
	TArray<FHLSLShaderInput_Debug> Inputs;
	UPROPERTY(VisibleAnywhere)
	TArray<FHLSLShaderOutput_Debug> Outputs;
};

UCLASS()
class HLSLSHADERRUNTIME_API UHLSLShaderLibrary : public UObject
{
	GENERATED_BODY()

	public:
#if WITH_EDITORONLY_DATA
	// HLSL file containing functions
	UPROPERTY(EditAnywhere, Category = "Config")
	FFilePath File;

	// If true assets will automatically be updated when the file is modified on disk by an external editor
	UPROPERTY(EditAnywhere, Category = "Config", AssetRegistrySearchable)
	bool bUpdateOnFileChange = true;

	// Update the assets when any of the included files are updated
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bUpdateOnIncludeChange = false;

	// If true, functions will be put in a folder named "AssetName_GeneratedFunctions"
	// If false they'll be generated next to this asset
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bPutFunctionsInSubdirectory = true;

	// If true, will insert preprocessor directive so that compilation errors are relative to your hlsl file
	// instead of the huge generated material file
	//
	// ie, errors will look like MyFile.hlsl:9 instead of /Generated/Material.usf:2330
	// 
	// The downside is that whenever you add or remove a line to your file, all the functions below it will have to be recompiled
	// If compilation is taking forever for you, consider turning this off
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAccurateErrors = true;

	UPROPERTY(EditAnywhere, Category = "Config")
	bool bAutomaticallyApply = true;

	UPROPERTY(EditAnywhere, Category = "Config")
	TArray<UMaterialParameterCollection*> ParameterCollections;
	
	UPROPERTY(EditAnywhere, Category = "Generated")
	TSoftObjectPtr<UMaterial> Materials;

	UPROPERTY(VisibleAnywhere, Category="Debug")
	TArray<FHLSLShaderResults> ShaderResults;
	
#endif

#if WITH_EDITOR
public:
	FString GetFilePath() const;
	static FString GetFilePath(const FString& InFilePath);

	void CreateWatcherIfNeeded();

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	//~ End UObject Interface

private:
	TSharedPtr<FVirtualDestructor> Watcher;

	static void MakeRelativePath(FString& Path);

public:
	static bool TryConvertShaderPathToFilename(const FString& ShaderPath, FString& OutFilename);
	static bool TryConvertFilenameToShaderPath(const FString& Filename, FString& OutShaderPath);

private:
	static bool TryConvertPathImpl(const TMap<FString, FString>& DirectoryMappings, const FString& InPath, FString& OutPath);
#endif
};
