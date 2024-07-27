// Copyright 2023 CoC All rights reserved


#include "HLSLEditorCommands.h"

#define LOCTEXT_NAMESPACE "FHLSLEditorCommands"

void FHLSLEditorCommands::RegisterCommands()
{
	UI_COMMAND(FocusViewport, "Focus Viewport", "Focus Viewport on Mesh", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
	UI_COMMAND(RecompileShader, "Recompile", "Recompiles Shader", EUserInterfaceActionType::Button, FInputChord(EKeys::C));
}

#undef LOCTEXT_NAMESPACE