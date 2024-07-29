using UnrealBuildTool;
using System.IO;

public class HLSLShaderEditor : ModuleRules
{
    public HLSLShaderEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.NoPCHs;

        bLegacyPublicIncludePaths = false;
        bUseUnity = false;

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "RHI",
                "RenderCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "MessageLog",
                "DesktopPlatform",
                "MaterialEditor",
                "HLSLShaderRuntime",
                "HLSLMaterialEditor",
                "HLSLMaterialRuntime",
                "MaterialEditor",
                "UnrealEd",
                "AdvancedPreviewScene",
                "InputCore",
                "AssetTools",
                "DeveloperSettings",
                "PropertyEditor",
                "ApplicationCore",
                "Projects",
                "ToolMenus"
            });

        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Developer/MessageLog/Private/"));
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Editor/MaterialEditor/Private/"));
    }
}