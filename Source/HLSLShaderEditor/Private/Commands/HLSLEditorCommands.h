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
	
	/** Focuses Viewport on Mesh */
	TSharedPtr<FUICommandInfo> FocusViewport;

	/** Recompiles Shader */
	TSharedPtr<FUICommandInfo> RecompileShader;
	
	/** Initialize commands */
	virtual void RegisterCommands() override;
};
