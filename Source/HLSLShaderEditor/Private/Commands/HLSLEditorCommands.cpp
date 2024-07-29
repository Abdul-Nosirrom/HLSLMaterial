// Copyright 2023 CoC All rights reserved


#include "HLSLEditorCommands.h"

#define LOCTEXT_NAMESPACE "FHLSLEditorCommands"

void FHLSLEditorCommands::RegisterCommands()
{
	UI_COMMAND(RecompileShader, "Recompile", "Recompiles Shader", EUserInterfaceActionType::Button, FInputChord(EKeys::C));
	UI_COMMAND(CreateMatInstance, "Create Mat Instance", "New Material Instance", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE