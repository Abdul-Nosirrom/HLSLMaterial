#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "HLSLShaderLibrary.h"
#include "HLSLShaderLibraryFactory.generated.h"

UCLASS()
class UHLSLShaderLibraryFactory : public UFactory
{
	GENERATED_BODY()

public:
	UHLSLShaderLibraryFactory()
	{
		bCreateNew = true;
		bEditorImport = false; // Allowing us to import from the editor
		bEditAfterNew = true;
		SupportedClass = UHLSLShaderLibrary::StaticClass();
	}

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		return NewObject<UObject>(InParent, Class, Name, Flags);
	}
	//~ End UFactory Interface
};
