// Copyright 2023 CoC All rights reserved


#include "HLSLShaderLibraryImportFactory.h"
#include "Editor.h"
#include "Subsystems/ImportSubsystem.h"
#include "HLSLShaderLibrary.h"

UHLSLShaderLibraryImportFactory::UHLSLShaderLibraryImportFactory()
{
	bCreateNew = false; // We'll create it from import so set this to false
	bEditorImport = true; // Allowing us to import from the editor
	bText = true; // Import from text files
	bEditAfterNew = true;
	Formats.Add(TEXT("hlsl;HLSL Shader File"));
	SupportedClass = UHLSLShaderLibrary::StaticClass();
}

UObject* UHLSLShaderLibraryImportFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName,
	EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd,
	FFeedbackContext* Warn)
{
	Flags |= RF_Transactional;

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

	const FString FileName = UFactory::GetCurrentFilename();

	UHLSLShaderLibrary* Result = NewObject<UHLSLShaderLibrary>(InParent, InName, Flags);
	if (Result)
	{
		Result->File = FFilePath(FileName);
	}
	
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, Result);

	return Result;
}
