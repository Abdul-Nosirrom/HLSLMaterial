// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "IMaterialEditor.h"
#include "TickableEditorObject.h"
#include "Misc/NotifyHook.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class UHLSLShaderLibrary;

// Stolen from FMaterialInstanceEditor for the most part
class FHLSLShaderMaterialEditor : public IMaterialEditor, public FNotifyHook, public FGCObject, public FTickableEditorObject
{
public:
	FHLSLShaderMaterialEditor();
	virtual ~FHLSLShaderMaterialEditor();

	/// @brief	Initializes the editor to use a shader asset w/ a material dynamically generated. Should be used once
	void InitShaderEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UHLSLShaderLibrary* InHLSLAsset);
	
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	
	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FHLSLShaderEditor");
	}

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual void InitToolMenuContext(struct FToolMenuContext& MenuContext) override;

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** The material instance applied to the preview mesh. */
	virtual UMaterialInterface* GetMaterialInterface() const override;

	/** Pre edit change notify for properties. */
	virtual void NotifyPreChange( FProperty* PropertyAboutToChange ) override;

	/** Post edit change notify for properties. */
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged ) override;

	/** Rebuilds the editor when the original material changes */
	void RebuildMaterialInstanceEditor();

	/**
	 * Draws messages on the specified viewport and canvas.
	 */
	virtual void DrawMessages( FViewport* Viewport, FCanvas* Canvas ) override;

	/**
	 * Draws sampler/texture mismatch warning strings.
	 * @param Canvas - The canvas on which to draw.
	 * @param DrawPositionY - The Y position at which to draw. Upon return contains the Y value following the last line of text drawn.
	 */
	void DrawSamplerWarningStrings(FCanvas* Canvas, int32& DrawPositionY);

	/** Passes instructions to the preview viewport */
	bool SetPreviewAsset(UObject* InAsset);
	bool SetPreviewAssetByName(const TCHAR* InMeshName);
	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Gets the extensibility managers for outside entities to extend material editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() { return ToolBarExtensibilityManager; }
	
	/** call this to notify the editor that the edited material changed from outside */
	virtual void NotifyExternalMaterialChange() override;

protected:
	
	/** Updated the properties pane */
	void UpdatePropertyWindow();

private:
	/** Binds our UI commands to delegates */
	void BindCommands();

	/** Command for the apply button */
	void OnRecompile();
	
	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();
	
	/** Updates the 3D and UI preview viewport visibility based on material domain */
	void UpdatePreviewViewportsVisibility();

	void RegisterToolBar();
	/** Builds the toolbar widget for the material editor */
	void ExtendToolbar();
	
	//~ Begin IMaterialEditor Interface
	virtual bool ApproveSetPreviewAsset(UObject* InAsset) override { return true; }

	/**	Spawns the preview tab */
	TSharedRef<SDockTab> SpawnTab_Preview( const FSpawnTabArgs& Args );

	/**	Spawns the properties tab */
	TSharedRef<SDockTab> SpawnTab_HLSLProperties( const FSpawnTabArgs& Args );

	/**	Spawns the properties tab */
	TSharedRef<SDockTab> SpawnTab_MaterialDetails(const FSpawnTabArgs& Args);
	
	/** Spawns the advanced preview settings tab */
	TSharedRef<SDockTab> SpawnTab_PreviewSettings(const FSpawnTabArgs& Args);

	/**	Caches the specified tab for later retrieval */
	void AddToSpawnedToolPanels( const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab );

	/**	Refresh the viewport and property window */
	void Refresh();

	/** Refreshes the preview asset */
	void RefreshPreviewAsset();

	void RefreshOnScreenMessages();

private:
	struct FOnScreenMessage
	{
		FOnScreenMessage() = default;
		FOnScreenMessage(const FLinearColor& InColor, const FString& InMessage) : Message(InMessage), Color(InColor) {}

		FString Message;
		FLinearColor Color;
	};

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockTab> > SpawnedToolPanels;

	/** Preview Viewport widget */
	TSharedPtr<class SMaterialEditor3DPreviewViewport> PreviewVC;

	/** Preview viewport widget used for UI materials */
	TSharedPtr<class SMaterialEditorUIPreviewViewport> PreviewUIViewport;

	/** HLSL Shader Properties View */
	TSharedPtr<class IDetailsView> HLSLAssetDetails;

	/** Material Instance Properties View */
	TSharedPtr<class IDetailsView> MaterialInstanceDetails;
	
	/** List of all current on-screen messages to display */
	TArray<FOnScreenMessage> OnScreenMessages;

	/** Object that stores all of the possible parameters we can edit. */
	TObjectPtr<class UMaterialEditorInstanceConstant> MaterialEditorInstance;
	
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	class UHLSLShaderLibrary* HLSLAsset;


	/**	The ids for the tabs spawned by this toolkit */
	static const FName PreviewTabId;		
	static const FName HLSLAssetDetailsTabId;	
	static const FName MaterialInstanceDetailsTabId;
	static const FName PreviewSettingsTabId;

	/** Object used as material statistics manager */
	TSharedPtr<class FMaterialStats> MaterialStatsManager;
};
