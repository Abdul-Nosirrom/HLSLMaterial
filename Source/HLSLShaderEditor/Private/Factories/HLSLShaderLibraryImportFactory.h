// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "HLSLShaderLibraryImportFactory.generated.h"

UCLASS()
class UHLSLShaderLibraryImportFactory : public UFactory
{
	GENERATED_BODY()

public:
	UHLSLShaderLibraryImportFactory();

	//~ Begin UFactory Interface
	UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn);
	//~ End UFactory Interface
};
