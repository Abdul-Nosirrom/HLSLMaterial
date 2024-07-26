// Fill out your copyright notice in the Description page of Project Settings.

#include "HLSLShaderLibrary.h"
#include "ShaderCore.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
IHLSLShaderEditorInterface* IHLSLShaderEditorInterface::StaticInterface = nullptr;

FString UHLSLShaderLibrary::GetFilePath() const
{
	return GetFilePath(File.FilePath);
}

FString UHLSLShaderLibrary::GetFilePath(const FString& InFilePath)
{
	FString FullPath;

	if (
		// Try to convert from a virtual shader path, eg /Plugin/Smthg
		// Give priority to shader paths
		!TryConvertShaderPathToFilename(InFilePath, FullPath) &&
		// Try to convert from a virtual content path, eg /Game/Smthg
		!FPackageName::TryConvertLongPackageNameToFilename(InFilePath, FullPath))
	{
		FullPath = InFilePath;
	}

	// Always return the full path
	return FPaths::ConvertRelativePathToFull(FullPath);
}

void UHLSLShaderLibrary::CreateWatcherIfNeeded()
{
	if (bUpdateOnFileChange && IHLSLShaderEditorInterface::Get())
	{
		Watcher = IHLSLShaderEditorInterface::Get()->CreateWatcher(*this);
	}
}

void UHLSLShaderLibrary::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// This is overkill, but better be safe with keeping things up to date

	FString NewPath = File.FilePath;
	MakeRelativePath(NewPath);

	if (GetFilePath(File.FilePath) == GetFilePath(NewPath))
	{
		// Conversion is safe
		File.FilePath = NewPath;
	}

	Watcher.Reset();
	CreateWatcherIfNeeded();
}

void UHLSLShaderLibrary::BeginDestroy()
{
	Super::BeginDestroy();

	Watcher.Reset();
}

void UHLSLShaderLibrary::PostLoad()
{
	Super::PostLoad();
	
	CreateWatcherIfNeeded();
}

void UHLSLShaderLibrary::MakeRelativePath(FString& Path)
{
	const FString AbsolutePickedPath = FPaths::ConvertRelativePathToFull(Path);

	if (!FPaths::FileExists(AbsolutePickedPath))
	{
		// This is either a manual entry or already a relative entry, don't do anything to it
		return;
	}

	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(AbsolutePickedPath, PackageName))
	{
		Path = PackageName + "." + FPaths::GetExtension(Path);
		return;
	}

	FString ShaderPath;
	if (TryConvertFilenameToShaderPath(AbsolutePickedPath, ShaderPath))
	{
		Path = ShaderPath;
		return;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool UHLSLShaderLibrary::TryConvertShaderPathToFilename(const FString& ShaderPath, FString& OutFilename)
{
	return TryConvertPathImpl(AllShaderSourceDirectoryMappings(), ShaderPath, OutFilename);
}

bool UHLSLShaderLibrary::TryConvertFilenameToShaderPath(const FString& Filename, FString& OutShaderPath)
{
	TMap<FString, FString> ShaderInverseDirectoryMappings;
	for (auto& It : AllShaderSourceDirectoryMappings())
	{
		ShaderInverseDirectoryMappings.Add(It.Value, It.Key);
	}

	return TryConvertPathImpl(ShaderInverseDirectoryMappings, Filename, OutShaderPath);
}

bool UHLSLShaderLibrary::TryConvertPathImpl(const TMap<FString, FString>& DirectoryMappings, const FString& InPath, FString& OutPath)
{
	FString ParentDirectoryPath = FPaths::GetPath(InPath);
	FString RelativeDirectoryPath = FPaths::GetCleanFilename(InPath);
	while (!ParentDirectoryPath.IsEmpty())
	{
		if (const FString* Mapping = DirectoryMappings.Find(ParentDirectoryPath))
		{
			OutPath = FPaths::Combine(*Mapping, RelativeDirectoryPath);
			return true;
		}

		RelativeDirectoryPath = FPaths::GetCleanFilename(ParentDirectoryPath) / RelativeDirectoryPath;
		ParentDirectoryPath = FPaths::GetPath(ParentDirectoryPath);
	}

	return false;
}
#endif