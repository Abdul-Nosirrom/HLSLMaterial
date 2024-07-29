// Copyright 2023 CoC All rights reserved

#pragma once

#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

class FHLSLEditorCommands : public TCommands<FHLSLEditorCommands>
{
public:
	/** Constructor */
	FHLSLEditorCommands() 
		: TCommands<FHLSLEditorCommands>("HLSLEditorCommands", NSLOCTEXT("Contexts", "HLSLShaderEditor", "HLSL Shader Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
		
	}
	
	/** Recompiles Shader */
	TSharedPtr<FUICommandInfo> RecompileShader;

	/** Creates A Material Instance */
	TSharedPtr<FUICommandInfo> CreateMatInstance;

	
	/** Initialize commands */
	virtual void RegisterCommands() override;
};
