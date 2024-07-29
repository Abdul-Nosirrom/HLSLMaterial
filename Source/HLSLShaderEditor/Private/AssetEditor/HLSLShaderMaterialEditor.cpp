// Copyright 2023 CoC All rights reserved


#include "HLSLShaderMaterialEditor.h"

#include "AdvancedPreviewSceneModule.h"
#include "CanvasTypes.h"
#include "HLSLShaderEditorModule.h"
#include "HLSLShaderLibrary.h"
#include "MaterialEditorActions.h"
#include "MaterialEditorContext.h"
#include "MaterialEditorModule.h"
#include "MaterialEditorUtilities.h"
#include "MaterialStatsCommon.h"
#include "MaterialUtilities.h"
#include "UnrealEdGlobals.h"
#include "Commands/HLSLEditorCommands.h"
#include "Editor/UnrealEdEngine.h"
#include "MaterialEditor.h"
#include "MaterialEditor/DEditorRuntimeVirtualTextureParameterValue.h"
#include "MaterialEditor/DEditorSparseVolumeTextureParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/HLSLMaterialEditorInstanceDetailCustomization.h"
#include "MaterialEditor/SHLSLMaterialEditorViewport.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Materials/MaterialExpressionSparseVolumeTextureSample.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ShaderGeneration/HLSLShaderLibraryEditor.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "HLSLShaderMaterialEditor"


const FName FHLSLShaderMaterialEditor::PreviewTabId( TEXT( "HLSLShaderMaterialEditor_Preview" ) );
const FName FHLSLShaderMaterialEditor::HLSLAssetDetailsTabId( TEXT( "HLSLShaderMaterialEditor_HLSLAssetProperties" ) );
const FName FHLSLShaderMaterialEditor::MaterialInstanceDetailsTabId(TEXT("HLSLShaderMaterialEditor_MaterialProperties"));
const FName FHLSLShaderMaterialEditor::PreviewSettingsTabId(TEXT("HLSLShaderMaterialEditor_PreviewSettings"));


FHLSLShaderMaterialEditor::FHLSLShaderMaterialEditor()
	: MaterialEditorInstance(nullptr)
	, MenuExtensibilityManager(new FExtensibilityManager)
	, ToolBarExtensibilityManager(new FExtensibilityManager)
	, HLSLAsset(nullptr)
{
}

FHLSLShaderMaterialEditor::~FHLSLShaderMaterialEditor()
{
	OnMaterialEditorClosed().Broadcast();

	if (MaterialEditorInstance)
	{
		MaterialEditorInstance->SourceInstance = nullptr;
		MaterialEditorInstance->SourceFunction = nullptr;
		MaterialEditorInstance->MarkAsGarbage();
		MaterialEditorInstance = nullptr;
	}

	MaterialInstanceDetails.Reset();
}

void FHLSLShaderMaterialEditor::InitShaderEditor(const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost, UHLSLShaderLibrary* InHLSLAsset)
{
	HLSLAsset = InHLSLAsset;

	// We create a dummy instance of our material that'll be used in the preview and get rid of it later when the asset editor closes
	UMaterialInterface* GeneratedMaterialBase = HLSLAsset->Materials.Get();
	if (!GeneratedMaterialBase)
	{
		// Use the default world material
		GeneratedMaterialBase = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"), NULL, LOAD_None, NULL);
	}
	UMaterialInstanceConstant* MaterialInstanceProxy = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), FName("HLSL_Shader_Preview"), RF_Transactional);
	checkf(MaterialInstanceProxy, TEXT("Failed to create instanced material"));
	MaterialInstanceProxy->Parent = GeneratedMaterialBase;

	// Construct a temp holder for our instance parameters
	MaterialEditorInstance = NewObject<UMaterialEditorInstanceConstant>(GetTransientPackage(), NAME_None, RF_Transactional);
	MaterialEditorInstance->SetSourceInstance(MaterialInstanceProxy);

	// Setup stats manager
	//MaterialStatsManager = FMaterialStatsUtils::CreateMaterialStats(this, false, true);
	//MaterialStatsManager->SetMaterialsDisplayNames({MaterialEditorInstance->SourceInstance->GetName()});

	// Register our commands. This will only register them if not previously registered
	FMaterialEditorCommands::Register();

	CreateInternalWidgets();

	BindCommands();
	RegisterToolBar();

	// If UI material, show UI viewport, otherwise show 3D viewport
	UpdatePreviewViewportsVisibility();
	IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");

	// Specify the layout of the editor
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("HLSLShaderEditorLayout")
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()
			->SetSizeCoefficient(0.8f)
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->AddTab(PreviewTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(MaterialInstanceDetailsTabId, ETabState::OpenedTab)
				->AddTab(PreviewSettingsTabId, ETabState::OpenedTab)
			)
		)
		->Split
		(
			/*FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->AddTab("OutputLog", ETabState::OpenedTab)*/
		FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab(HLSLAssetDetailsTabId, ETabState::OpenedTab)
		)
	);

	// Initialize
	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, "HLSLShaderEditor", Layout, true, true, InHLSLAsset);

	// Add toolbars TODO:
	//AddMenuExtender(MaterialEditorModule->GetMenuExtensibilityManager())

	// Add toolbar buttons
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Set the preview mesh for the material.  This call must occur after the toolbar is initialized.
	
	if ( !SetPreviewAssetByName( *MaterialInstanceProxy->PreviewMesh.ToString() ) )
	{
		// If the preview mesh could not be found for this instance, attempt to use the preview mesh for the parent material if one exists,
		//	or use a default instead if the parent's preview mesh cannot be used

		if ( MaterialInstanceProxy->Parent == nullptr || !SetPreviewAssetByName( *MaterialInstanceProxy->Parent->PreviewMesh.ToString() ) )
		{
			USceneThumbnailInfoWithPrimitive* ThumbnailInfoWithPrim = Cast<USceneThumbnailInfoWithPrimitive>( MaterialInstanceProxy->ThumbnailInfo );

			if ( ThumbnailInfoWithPrim != nullptr )
			{
				SetPreviewAssetByName( *ThumbnailInfoWithPrim->PreviewMesh.ToString() );
			}
		}
	}

	Refresh();
}

void FHLSLShaderMaterialEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_HLSLShaderMaterialEditor", "HLSL Shader Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	////////////////// Spawn our layout elements ///////////////////////
	
	// Viewport Tab 
	InTabManager->RegisterTabSpawner(PreviewTabId, FOnSpawnTab::CreateSP(this, &FHLSLShaderMaterialEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon( FSlateIcon( FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports" ) );

	// HLSL Asset details tab
	InTabManager->RegisterTabSpawner(HLSLAssetDetailsTabId, FOnSpawnTab::CreateSP(this, &FHLSLShaderMaterialEditor::SpawnTab_HLSLProperties))
		.SetDisplayName(LOCTEXT("HLSLAssetDetails", "HLSL Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon( FSlateIcon( FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details" ) );

	// Material Instance Details Tab
	InTabManager->RegisterTabSpawner(MaterialInstanceDetailsTabId, FOnSpawnTab::CreateSP(this, &FHLSLShaderMaterialEditor::SpawnTab_MaterialDetails))
		.SetDisplayName(LOCTEXT("MatInstanceDetails", "Material Preview Parameters"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon( FSlateIcon( FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details" ) );

	// Preview Settings Tab
	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FHLSLShaderMaterialEditor::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon( FSlateIcon( FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details" ) );

	// Notify stats
	//MaterialStatsManager->RegisterTabs();

	OnRegisterTabSpawners().Broadcast(InTabManager);
}

void FHLSLShaderMaterialEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PreviewTabId);
	InTabManager->UnregisterTabSpawner(MaterialInstanceDetailsTabId);
	InTabManager->UnregisterTabSpawner(HLSLAssetDetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);

	//MaterialStatsManager->UnregisterTabs();

	OnUnregisterTabSpawners().Broadcast(InTabManager);
}


void FHLSLShaderMaterialEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MaterialEditorInstance);
}

FName FHLSLShaderMaterialEditor::GetToolkitFName() const
{
	return FName("HLSLShaderEditor");
}

FText FHLSLShaderMaterialEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "HLSL Shader Editor");
}

FText FHLSLShaderMaterialEditor::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObjects()[0];
	check(EditingObject);

	return GetLabelForObject(EditingObject);
}

FText FHLSLShaderMaterialEditor::GetToolkitToolTipText() const
{
	const UObject* EditingObject = GetEditingObjects()[0];

	// Overridden to accommodate editing of multiple objects (original and preview materials)
	return FAssetEditorToolkit::GetToolTipTextForObject(EditingObject);
}

FString FHLSLShaderMaterialEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "HLSL Shader ").ToString();
}

void FHLSLShaderMaterialEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UMaterialEditorMenuContext* Context = NewObject<UMaterialEditorMenuContext>();
	Context->MaterialEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

FLinearColor FHLSLShaderMaterialEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

UMaterialInterface* FHLSLShaderMaterialEditor::GetMaterialInterface() const
{
	return MaterialEditorInstance->SourceInstance;
}

void FHLSLShaderMaterialEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{}

void FHLSLShaderMaterialEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent,
	FProperty* PropertyThatChanged)
{
	// Change preview asset if needed for viewport. We then refresh the viewport later
	if (PropertyThatChanged->GetName() == TEXT("PreviewMesh"))
	{
		RefreshPreviewAsset();
	}

	// Rebuild the property window to account for the possibility that  
	// the item changed was a static switch or function call parameter
	UObject* PropertyClass = PropertyThatChanged->GetOwner<UObject>();
	if(PropertyClass && (PropertyClass->GetName() == TEXT("DEditorStaticSwitchParameterValue") || PropertyClass->GetName() == TEXT("EditorParameterGroup"))//DEditorMaterialLayerParameters"))
		&& MaterialEditorInstance->Parent && MaterialEditorInstance->SourceInstance )
	{
		// TODO: We need to hit this on MaterialLayerParam updates but only get notifications for their array elements changing, hence the overly generic test above
		MaterialEditorInstance->VisibleExpressions.Empty();
		FMaterialEditorUtilities::GetVisibleMaterialParameters(MaterialEditorInstance->Parent->GetMaterial(), MaterialEditorInstance->SourceInstance, MaterialEditorInstance->VisibleExpressions);

		UpdatePropertyWindow();
	}

	RefreshOnScreenMessages();

	// something was changed in the material so we need to reflect this in the stats
	//MaterialStatsManager->SignalMaterialChanged();

	// Update the preview window when the user changes a property.
	PreviewVC->RefreshViewport();
}

void FHLSLShaderMaterialEditor::RebuildMaterialInstanceEditor()
{
	if( MaterialEditorInstance )
	{
		MaterialEditorInstance->CopyBasePropertiesFromParent();
		MaterialEditorInstance->RegenerateArrays();
		UpdatePropertyWindow();
	}
}

void FHLSLShaderMaterialEditor::DrawMessages(FViewport* Viewport, FCanvas* Canvas)
{
	// Draw messages to the viewport (stats, warnings, etc...)
	Canvas->PushAbsoluteTransform(FMatrix::Identity);
	if ( MaterialEditorInstance->Parent && MaterialEditorInstance->SourceInstance )
	{
		const FMaterialResource* MaterialResource = MaterialEditorInstance->SourceInstance->GetMaterialResource(GMaxRHIFeatureLevel);
		UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
		int32 DrawPositionY = 50;
		if ( BaseMaterial && MaterialResource )
		{
			const bool bGeneratedNewShaders = MaterialEditorInstance->SourceInstance->bHasStaticPermutationResource;
			const bool bAllowOldMaterialStats = true;
			// copied from FMaterialEditor::DrawMaterialInfoStrings since that function isnt exported
			DrawMaterialInfoStrings( Canvas, BaseMaterial, MaterialResource, MaterialResource->GetCompileErrors(), DrawPositionY, bAllowOldMaterialStats, bGeneratedNewShaders );
		}

		DrawSamplerWarningStrings( Canvas, DrawPositionY );
	}
	Canvas->PopTransform();
}

void FHLSLShaderMaterialEditor::DrawSamplerWarningStrings(FCanvas* Canvas, int32& DrawPositionY)
{
	const int32 SpacingBetweenLines = 13;
	UFont* FontToUse = GEngine->GetTinyFont();
	for (const FOnScreenMessage& Message : OnScreenMessages)
	{
		Canvas->DrawShadowedString(
			5,
			DrawPositionY,
			*Message.Message,
			FontToUse,
			Message.Color);
		DrawPositionY += SpacingBetweenLines;
	}
}

bool FHLSLShaderMaterialEditor::SetPreviewAsset(UObject* InAsset)
{
	if (PreviewVC.IsValid())
	{
		return PreviewVC->SetPreviewAsset(InAsset);
	}
	return false;
}

bool FHLSLShaderMaterialEditor::SetPreviewAssetByName(const TCHAR* InMeshName)
{
	if (PreviewVC.IsValid())
	{
		return PreviewVC->SetPreviewAssetByName(InMeshName);
	}
	return false;
}

void FHLSLShaderMaterialEditor::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	if (PreviewVC.IsValid())
	{
		PreviewVC->SetPreviewMaterial(InMaterialInterface);
	}
}

void FHLSLShaderMaterialEditor::Tick(float DeltaTime)
{
	// Sometimes the material doesnt get set on start up, so make sure to refresh if its not set
	if (MaterialEditorInstance->Parent != HLSLAsset->Materials)
	{
		NotifyExternalMaterialChange(); // This does the refresh of everything and setting the right material
	}
}

TStatId FHLSLShaderMaterialEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FHLSLShaderMaterialEditor, STATGROUP_Tickables);
}

void FHLSLShaderMaterialEditor::NotifyExternalMaterialChange()
{
	// Called after we finish regenerating the shader/material from the HLSL file
	if (MaterialEditorInstance->Parent != HLSLAsset->Materials)
	{
		MaterialEditorInstance->SourceInstance = nullptr;
		UMaterialInstanceConstant* MaterialInstanceProxy = NewObject<UMaterialInstanceConstant>(GetTransientPackage(), FName("HLSL_Shader_Preview"), RF_Transactional);
		checkf(MaterialInstanceProxy, TEXT("Failed to create instanced material"));
		MaterialInstanceProxy->Parent = HLSLAsset->Materials.Get();
		MaterialEditorInstance->SetSourceInstance(MaterialInstanceProxy);
	}
	RebuildMaterialInstanceEditor();
	UpdatePreviewViewportsVisibility();
	Refresh();
	//MaterialStatsManager->SignalMaterialChanged();
}

void FHLSLShaderMaterialEditor::UpdatePropertyWindow()
{
	HLSLAssetDetails->SetObject(HLSLAsset, true);
	MaterialInstanceDetails->SetObject( MaterialEditorInstance, true );
}

void FHLSLShaderMaterialEditor::BindCommands()
{
	// Bind toolbar command actions
	FHLSLEditorCommands::Register();

	const FHLSLEditorCommands& Commands = FHLSLEditorCommands::Get();

	ToolkitCommands->MapAction(Commands.RecompileShader,
	FExecuteAction::CreateSP(this, &FHLSLShaderMaterialEditor::OnRecompile));

	ToolkitCommands->MapAction(Commands.CreateMatInstance, FExecuteAction::CreateLambda([this]()
	{
		FHLSLShaderLibraryEditor::GenerateMaterialInstanceForShader(*HLSLAsset);
	}));
}

void FHLSLShaderMaterialEditor::OnRecompile()
{
	if (HLSLAsset)
	{
		FHLSLShaderLibraryEditor::Generate(*HLSLAsset);
	}
	if (MaterialEditorInstance)
	{
		MaterialEditorInstance->bIsFunctionInstanceDirty = true;
		MaterialEditorInstance->ApplySourceFunctionChanges();
	}
}

void FHLSLShaderMaterialEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PreviewVC = SNew(SHLSLMaterialEditor3DPreviewViewport)
	.MaterialEditor(SharedThis(this));
	PreviewUIViewport = SNew(SHLSLMaterialEditorUIPreviewViewport, GetMaterialInterface());

	// Create HLSL asset details
	{
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	
		HLSLAssetDetails = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		HLSLAssetDetails->SetObjects(TArray<UObject*>{ HLSLAsset });
	}

	// Create material instance details
	{
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.NotifyHook = this;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowCustomFilterOption = true;
		MaterialInstanceDetails = PropertyEditorModule.CreateDetailView( DetailsViewArgs );
		// the sizes of the parameter lists are only based on the parent material and not changed out from under the details panel 
		// When a parameter is added open MI editors are refreshed
		// the tree should also refresh if one of the layer or blend assets is swapped

		auto ValidationLambda = ([](const FRootPropertyNodeList& PropertyNodeList) { return true; });
		MaterialInstanceDetails->SetCustomValidatePropertyNodesFunction(FOnValidateDetailsViewPropertyNodes::CreateLambda(MoveTemp(ValidationLambda)));

		FOnGetDetailCustomizationInstance LayoutMICDetails = FOnGetDetailCustomizationInstance::CreateStatic( 
			&FHLSLMaterialInstanceParameterDetails::MakeInstance, MaterialEditorInstance.Get(), FGetShowHiddenParameters::CreateLambda([](bool& bShowHidden) {bShowHidden = true; }) );
		MaterialInstanceDetails->RegisterInstancedCustomPropertyLayout( UMaterialEditorInstanceConstant::StaticClass(), LayoutMICDetails );
		//MaterialInstanceDetails->SetCustomFilterLabel(LOCTEXT("ShowOverriddenOnly", "Show Only Overridden Parameters"));
		//MaterialInstanceDetails->SetCustomFilterDelegate(FSimpleDelegate::CreateSP(this, &FMaterialInstanceEditor::FilterOverriddenProperties));
		MaterialEditorInstance->DetailsView = MaterialInstanceDetails;

	}
}


void FHLSLShaderMaterialEditor::UpdatePreviewViewportsVisibility()
{
	UMaterial* PreviewMaterial = GetMaterialInterface()->GetBaseMaterial();
	if( PreviewMaterial->IsUIMaterial() )
	{
		PreviewVC->SetVisibility(EVisibility::Collapsed);
		PreviewUIViewport->SetVisibility(EVisibility::Visible);
	}
	else
	{
		PreviewVC->SetVisibility(EVisibility::Visible);
		PreviewUIViewport->SetVisibility(EVisibility::Collapsed);
	}
}

void FHLSLShaderMaterialEditor::RegisterToolBar()
{
	FName MenuName = FAssetEditorToolkit::GetToolMenuToolbarName();
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, "AssetEditor.DefaultToolBar", EMultiBoxType::ToolBar);

		FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
		{
			FToolMenuSection& MaterialInstanceSection = ToolBar->AddSection("HLSLShaderTools", TAttribute<FText>(), InsertAfterAssetSection);

			MaterialInstanceSection.AddEntry(FToolMenuEntry::InitToolBarButton(FHLSLEditorCommands::Get().RecompileShader));
			MaterialInstanceSection.AddEntry(FToolMenuEntry::InitToolBarButton(FHLSLEditorCommands::Get().CreateMatInstance));
		}
	}
}

void FHLSLShaderMaterialEditor::ExtendToolbar()
{
	AddToolbarExtender(GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	FHLSLShaderEditorModule* HLSLEditorModule = &FModuleManager::LoadModuleChecked<FHLSLShaderEditorModule>(FName("HLSLShaderEditor"));
	AddToolbarExtender(HLSLEditorModule->GetEditorToolbarExtensibilityManager()->GetAllExtenders());
}

TSharedRef<SDockTab> FHLSLShaderMaterialEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId().TabType == PreviewTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabTitle", "Viewport"))
		[
			SNew( SOverlay )
			+ SOverlay::Slot()
			[
				PreviewVC.ToSharedRef()
			]
			+ SOverlay::Slot()
			[
				PreviewUIViewport.ToSharedRef()
			]
		];

	PreviewVC->OnAddedToTab( SpawnedTab );

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}

TSharedRef<SDockTab> FHLSLShaderMaterialEditor::SpawnTab_HLSLProperties(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId().TabType == HLSLAssetDetailsTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("HLSLShaderDetails", "Details"))
		[
			SNew(SBorder)
			.Padding(4)
			[
				HLSLAssetDetails.ToSharedRef()
			]
		];

	UpdatePropertyWindow();

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}

TSharedRef<SDockTab> FHLSLShaderMaterialEditor::SpawnTab_MaterialDetails(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId().TabType == MaterialInstanceDetailsTabId );

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("MaterialPropertiesTitle", "Material Properties"))
		[
			SNew(SBorder)
			.Padding(4)
			[
				MaterialInstanceDetails.ToSharedRef()
			]
		];

	UpdatePropertyWindow();

	AddToSpawnedToolPanels( Args.GetTabId().TabType, SpawnedTab );
	return SpawnedTab;
}

TSharedRef<SDockTab> FHLSLShaderMaterialEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	TSharedRef<SWidget> InWidget = SNullWidget::NullWidget;
	if (PreviewVC.IsValid())
	{
		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		InWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewVC->GetPreviewScene());
	}

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			SNew(SBox)
			[
				InWidget
			]
		];

	return SpawnedTab;
}

void FHLSLShaderMaterialEditor::AddToSpawnedToolPanels(const FName& TabIdentifier,
	const TSharedRef<SDockTab>& SpawnedTab)
{
	TWeakPtr<SDockTab>* TabSpot = SpawnedToolPanels.Find(TabIdentifier);
	if (!TabSpot)
	{
		SpawnedToolPanels.Add(TabIdentifier, SpawnedTab);
	}
	else
	{
		check(!TabSpot->IsValid());
		*TabSpot = SpawnedTab;
	}
}

void FHLSLShaderMaterialEditor::Refresh()
{
	PreviewVC->RefreshViewport();
	
	UpdatePropertyWindow();

	RefreshOnScreenMessages();
}

void FHLSLShaderMaterialEditor::RefreshPreviewAsset()
{
	UObject* PreviewAsset = MaterialEditorInstance->SourceInstance->PreviewMesh.TryLoad();
	if (!PreviewAsset)
	{
		// Attempt to use the parent material's preview mesh if the instance's preview mesh is invalid, and use a default
		//	sphere instead if the parent's mesh is also invalid
		UMaterialInterface* ParentMaterial = MaterialEditorInstance->SourceInstance->Parent;

		UObject* ParentPreview = ParentMaterial != nullptr ? ParentMaterial->PreviewMesh.TryLoad() : nullptr;
		PreviewAsset = ParentPreview != nullptr ? ParentPreview : ToRawPtr(GUnrealEd->GetThumbnailManager()->EditorSphere);

		USceneThumbnailInfoWithPrimitive* ThumbnailInfo = Cast<USceneThumbnailInfoWithPrimitive>(MaterialEditorInstance->SourceInstance->ThumbnailInfo);
		if (ThumbnailInfo)
		{
			ThumbnailInfo->PreviewMesh.Reset();
		}

	}
	PreviewVC->SetPreviewAsset(PreviewAsset);
}

void FHLSLShaderMaterialEditor::RefreshOnScreenMessages()
{
	OnScreenMessages.Reset();

	// Copied directly from MaterialInstanceEditor
	if (MaterialEditorInstance->SourceInstance)
	{
		UMaterial* BaseMaterial = MaterialEditorInstance->SourceInstance->GetMaterial();
		if (BaseMaterial)
		{
			UEnum* SamplerTypeEnum = StaticEnum<EMaterialSamplerType>();
			check(SamplerTypeEnum);
			UEnum* MaterialTypeEnum = StaticEnum<ERuntimeVirtualTextureMaterialType>();
			check(MaterialTypeEnum);

			const int32 GroupCount = MaterialEditorInstance->ParameterGroups.Num();
			for (int32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
			{
				const FEditorParameterGroup& Group = MaterialEditorInstance->ParameterGroups[GroupIndex];
				const int32 ParameterCount = Group.Parameters.Num();
				for (int32 ParameterIndex = 0; ParameterIndex < ParameterCount; ++ParameterIndex)
				{
					UDEditorTextureParameterValue* TextureParameterValue = Cast<UDEditorTextureParameterValue>(Group.Parameters[ParameterIndex]);
					if (TextureParameterValue && TextureParameterValue->ExpressionId.IsValid())
					{
						UTexture* Texture = NULL;
						MaterialEditorInstance->SourceInstance->GetTextureParameterValue(TextureParameterValue->ParameterInfo, Texture);
						if (Texture)
						{
							EMaterialSamplerType SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(Texture);
							UMaterialExpressionTextureSampleParameter* Expression = BaseMaterial->FindExpressionByGUID<UMaterialExpressionTextureSampleParameter>(TextureParameterValue->ExpressionId);

							FString ErrorMessage;
							if (Expression && !Expression->TextureIsValid(Texture, ErrorMessage))
							{
								OnScreenMessages.Emplace(FLinearColor(1, 0, 0),
									FString::Printf(TEXT("Error: %s has invalid texture %s: %s."),
									*TextureParameterValue->ParameterInfo.Name.ToString(),
									*Texture->GetPathName(),
									*ErrorMessage));
							}
							else
							{
								if (Expression && Expression->SamplerType != SamplerType)
								{
									FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(Expression->SamplerType).ToString();
									OnScreenMessages.Emplace(FLinearColor(1, 1, 0),
										FString::Printf(TEXT("Warning: %s samples %s as %s."),
										*TextureParameterValue->ParameterInfo.Name.ToString(),
										*Texture->GetPathName(),
										*SamplerTypeDisplayName));
								}
								if (Expression && ((Expression->SamplerType == (EMaterialSamplerType)TC_Normalmap || Expression->SamplerType == (EMaterialSamplerType)TC_Masks) && Texture->SRGB))
								{
									FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(Expression->SamplerType).ToString();
									OnScreenMessages.Emplace(FLinearColor(1, 1, 0),
										FString::Printf(TEXT("Warning: %s samples texture as '%s'. SRGB should be disabled for '%s'."),
											*TextureParameterValue->ParameterInfo.Name.ToString(),
											*SamplerTypeDisplayName,
											*Texture->GetPathName()));
								}
							}
						}
					}

					UDEditorRuntimeVirtualTextureParameterValue* RuntimeVirtualTextureParameterValue = Cast<UDEditorRuntimeVirtualTextureParameterValue>(Group.Parameters[ParameterIndex]);
					if (RuntimeVirtualTextureParameterValue && RuntimeVirtualTextureParameterValue->ExpressionId.IsValid())
					{
						URuntimeVirtualTexture* RuntimeVirtualTexture = NULL;
						MaterialEditorInstance->SourceInstance->GetRuntimeVirtualTextureParameterValue(RuntimeVirtualTextureParameterValue->ParameterInfo, RuntimeVirtualTexture);
						if (RuntimeVirtualTexture)
						{
							UMaterialExpressionRuntimeVirtualTextureSampleParameter* Expression = BaseMaterial->FindExpressionByGUID<UMaterialExpressionRuntimeVirtualTextureSampleParameter>(RuntimeVirtualTextureParameterValue->ExpressionId);
							if (!Expression)
							{
								const FText ExpressionNameText = FText::Format(LOCTEXT("MissingRVTExpression", "Warning: Runtime Virtual Texture Expression {0} not found."), FText::FromName(RuntimeVirtualTextureParameterValue->ParameterInfo.Name));
								OnScreenMessages.Emplace(FLinearColor(1, 1, 0), ExpressionNameText.ToString());
							}
							if (Expression && Expression->MaterialType != RuntimeVirtualTexture->GetMaterialType())
							{
								FString BaseMaterialTypeDisplayName = MaterialTypeEnum->GetDisplayNameTextByValue((int64)(Expression->MaterialType)).ToString();
								FString OverrideMaterialTypeDisplayName = MaterialTypeEnum->GetDisplayNameTextByValue((int64)(RuntimeVirtualTexture->GetMaterialType())).ToString();

								OnScreenMessages.Emplace(FLinearColor(1, 1, 0),
									FText::Format(LOCTEXT("MismatchedRVTType", "Warning: '{0}' interprets the virtual texture as '{1}' not '{2}', {3}"),
									FText::FromName(RuntimeVirtualTextureParameterValue->ParameterInfo.Name),
									FText::FromString(BaseMaterialTypeDisplayName),
									FText::FromString(OverrideMaterialTypeDisplayName),
									FText::FromString(RuntimeVirtualTexture->GetPathName())).ToString());
							}
							if (Expression && Expression->bSinglePhysicalSpace != RuntimeVirtualTexture->GetSinglePhysicalSpace())
							{
								OnScreenMessages.Emplace(FLinearColor(1, 1, 0),
									FText::Format(LOCTEXT("VirtualTexturePagePackingWarning", "Warning: '{0}' interprets the virtual texture page table packing as {1} not {2}, {3}"),
									FText::FromName(RuntimeVirtualTextureParameterValue->ParameterInfo.Name),
									FText::FromString(RuntimeVirtualTexture->GetSinglePhysicalSpace() ? TEXT("true") : TEXT("false")),
									FText::FromString(Expression->bSinglePhysicalSpace ? TEXT("true") : TEXT("false")),
									FText::FromString(RuntimeVirtualTexture->GetPathName())).ToString());
							}
							if (Expression && Expression->bAdaptive != RuntimeVirtualTexture->GetAdaptivePageTable())
							{
								OnScreenMessages.Emplace(FLinearColor(1, 1, 0),
									FText::Format(LOCTEXT("VirtualTextureAdaptiveWarning", "Warning: '{0}' interprets the adaptive page table setting as {1} not {2}, {3}"),
									FText::FromName(RuntimeVirtualTextureParameterValue->ParameterInfo.Name),
									FText::FromString(RuntimeVirtualTexture->GetAdaptivePageTable() ? TEXT("true") : TEXT("false")),
									FText::FromString(Expression->bAdaptive ? TEXT("true") : TEXT("false")),
									FText::FromString(RuntimeVirtualTexture->GetPathName())).ToString());
							}
						}
					}

					UDEditorSparseVolumeTextureParameterValue * SparseVolumeTextureParameterValue = Cast<UDEditorSparseVolumeTextureParameterValue>(Group.Parameters[ParameterIndex]);
					if (SparseVolumeTextureParameterValue && SparseVolumeTextureParameterValue->ExpressionId.IsValid())
					{
						USparseVolumeTexture* SparseVolumeTexture = NULL;
						MaterialEditorInstance->SourceInstance->GetSparseVolumeTextureParameterValue(SparseVolumeTextureParameterValue->ParameterInfo, SparseVolumeTexture);
						if (SparseVolumeTexture)
						{
							UMaterialExpressionSparseVolumeTextureSampleParameter* Expression = BaseMaterial->FindExpressionByGUID<UMaterialExpressionSparseVolumeTextureSampleParameter>(SparseVolumeTextureParameterValue->ExpressionId);
							if (!Expression)
							{
								const FText ExpressionNameText = FText::Format(LOCTEXT("MissingSVTExpression", "Warning: Sparse Volume Texture Expression {0} not found."), FText::FromName(SparseVolumeTextureParameterValue->ParameterInfo.Name));
								OnScreenMessages.Emplace(FLinearColor(1, 1, 0), ExpressionNameText.ToString());
							}
						}
					}
				}
			}
		}
	}
}

void FHLSLShaderMaterialEditor::DrawMaterialInfoStrings(FCanvas* Canvas, const UMaterial* Material,
	const FMaterialResource* MaterialResource, const TArray<FString>& CompileErrors, int32& DrawPositionY,
	bool bDrawInstructions, bool bGeneratedNewShaders)
{
		check(Material && MaterialResource);

	ERHIFeatureLevel::Type FeatureLevel = MaterialResource->GetFeatureLevel();
	FString FeatureLevelName;
	GetFeatureLevelName(FeatureLevel,FeatureLevelName);

	// The font to use when displaying info strings
	UFont* FontToUse = GEngine->GetTinyFont();
	const int32 SpacingBetweenLines = 13;

	if (bDrawInstructions)
	{
		// Display any errors and messages in the upper left corner of the viewport.
		TArray<FMaterialStatsUtils::FShaderInstructionsInfo> Descriptions;

		// Copied from FMaterialShaderUtils::GetRepresentativeInstructionCount since its not exported
		{
			auto GetShaderString = [](const FShader::FShaderStatisticMap& Statistics)
			{
				TStringBuilder<2048> StatisticsStrBuilder;
				for (const auto& Stat : Statistics)
				{
					StatisticsStrBuilder << Stat.Key << ": ";
					Visit([&StatisticsStrBuilder](auto& StoredValue)
					{
						StatisticsStrBuilder << StoredValue << "\n";
					}, Stat.Value);
				}

				return StatisticsStrBuilder.ToString();
			};
			
			const FMaterialShaderMap* MaterialShaderMap = MaterialResource->GetGameThreadShaderMap();

			if (MaterialShaderMap)
			{
				TMap<FName, TArray<FMaterialStatsUtils::FRepresentativeShaderInfo>> ShaderTypeNamesAndDescriptions;
				FMaterialStatsUtils::GetRepresentativeShaderTypesAndDescriptions(ShaderTypeNamesAndDescriptions, MaterialResource);
				TStaticArray<bool, (int32)ERepresentativeShader::Num> bShaderTypeAdded(InPlace, false);
		
				if (MaterialResource->IsUIMaterial())
				{
					//for (const TPair<FName, FRepresentativeShaderInfo>& ShaderTypePair : ShaderTypeNamesAndDescriptions)
					for (auto DescriptionPair : ShaderTypeNamesAndDescriptions)
					{
						auto& DescriptionArray = DescriptionPair.Value;
						for (int32 i = 0; i < DescriptionArray.Num(); ++i)
						{
							const FMaterialStatsUtils::FRepresentativeShaderInfo& ShaderInfo = DescriptionArray[i];
							if (!bShaderTypeAdded[(int32)ShaderInfo.ShaderType])
							{
								FShaderType* ShaderType = FindShaderTypeByName(ShaderInfo.ShaderName);
								check(ShaderType);
								const int32 NumInstructions = MaterialShaderMap->GetMaxNumInstructionsForShader(ShaderType);

								FMaterialStatsUtils::FShaderInstructionsInfo Info;
								Info.ShaderType = ShaderInfo.ShaderType;
								Info.ShaderDescription = ShaderInfo.ShaderDescription;
								Info.InstructionCount = NumInstructions;
								Info.ShaderStatisticsString = GetShaderString(MaterialShaderMap->GetShaderStatisticsMapForShader(ShaderType));
								if (Info.ShaderStatisticsString.Len() == 0)
								{
									Info.ShaderStatisticsString = TEXT("n/a");
								}

								Descriptions.Push(Info);

								bShaderTypeAdded[(int32)ShaderInfo.ShaderType] = true;
							}
						}
					}
				}
				else
				{
					for (auto DescriptionPair : ShaderTypeNamesAndDescriptions)
					{
						FVertexFactoryType* FactoryType = FindVertexFactoryType(DescriptionPair.Key);
						const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(FactoryType);
						if (MeshShaderMap)
						{
							TMap<FHashedName, TShaderRef<FShader>> ShaderMap;
							MeshShaderMap->GetShaderList(*MaterialShaderMap, ShaderMap);

							auto& DescriptionArray = DescriptionPair.Value;

							for (int32 i = 0; i < DescriptionArray.Num(); ++i)
							{
								const FMaterialStatsUtils::FRepresentativeShaderInfo& ShaderInfo = DescriptionArray[i];
								if (!bShaderTypeAdded[(int32)ShaderInfo.ShaderType])
								{
									TShaderRef<FShader>* ShaderEntry = ShaderMap.Find(ShaderInfo.ShaderName);
									if (ShaderEntry != nullptr)
									{
										FShaderType* ShaderType = (*ShaderEntry).GetType();
										{
											const int32 NumInstructions = MeshShaderMap->GetMaxNumInstructionsForShader(*MaterialShaderMap, ShaderType);

											FMaterialStatsUtils::FShaderInstructionsInfo Info;
											Info.ShaderType = ShaderInfo.ShaderType;
											Info.ShaderDescription = ShaderInfo.ShaderDescription;
											Info.InstructionCount = NumInstructions;
											Info.ShaderStatisticsString = GetShaderString(MeshShaderMap->GetShaderStatisticsMapForShader(*MaterialShaderMap, ShaderType));
											if (Info.ShaderStatisticsString.Len() == 0)
											{
												Info.ShaderStatisticsString = TEXT("n/a");
											}

											Descriptions.Push(Info);

											bShaderTypeAdded[(int32)ShaderInfo.ShaderType] = true;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		for (int32 InstructionIndex = 0; InstructionIndex < Descriptions.Num(); InstructionIndex++)
		{
			FString InstructionCountString = FString::Printf(TEXT("%s: %u instructions"), *Descriptions[InstructionIndex].ShaderDescription, Descriptions[InstructionIndex].InstructionCount);
			Canvas->DrawShadowedString(5, DrawPositionY, *InstructionCountString, FontToUse, FLinearColor(1, 1, 0));
			DrawPositionY += SpacingBetweenLines;
		}

		// Display the number of texture samplers and samplers used by the material.
		const int32 SamplersUsed = MaterialResource->GetSamplerUsage();

		if (SamplersUsed >= 0)
		{
			int32 MaxSamplers = GetExpectedFeatureLevelMaxTextureSamplers(MaterialResource->GetFeatureLevel());

			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("%s samplers: %u/%u"), FeatureLevel <= ERHIFeatureLevel::ES3_1 ? TEXT("Mobile texture") : TEXT("Texture"), SamplersUsed, MaxSamplers),
				FontToUse,
				SamplersUsed > MaxSamplers ? FLinearColor(1,0,0) : FLinearColor(1,1,0)
				);
			DrawPositionY += SpacingBetweenLines;
		}

		uint32 NumVSTextureSamples = 0, NumPSTextureSamples = 0;
		MaterialResource->GetEstimatedNumTextureSamples(NumVSTextureSamples, NumPSTextureSamples);

		if (NumVSTextureSamples > 0 || NumPSTextureSamples > 0)
		{
			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("Texture Lookups (Est.): VS(%u), PS(%u)"), NumVSTextureSamples, NumPSTextureSamples),
				FontToUse,
				FLinearColor(1,1,0)
				);
			DrawPositionY += SpacingBetweenLines;
		}

		uint32 NumVirtualTextureLookups = MaterialResource->GetEstimatedNumVirtualTextureLookups();
		if (NumVirtualTextureLookups > 0)
		{
			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("Virtual Texture Lookups (Est.): %u"), NumVirtualTextureLookups),
				FontToUse,
				FLinearColor(1, 1, 0)
			);
			DrawPositionY += SpacingBetweenLines;
		}

		const uint32 NumVirtualTextureStacks = MaterialResource->GetNumVirtualTextureStacks();
		if (NumVirtualTextureStacks > 0)
		{
			Canvas->DrawShadowedString(
				5,
				DrawPositionY,
				*FString::Printf(TEXT("Virtual Texture Stacks: %u"), NumVirtualTextureStacks),
				FontToUse,
				FLinearColor(1, 1, 0)
			);
			DrawPositionY += SpacingBetweenLines;
		}

		TStaticArray<uint16, (int)ELWCFunctionKind::Max> LWCFuncUsages = MaterialResource->GetEstimatedLWCFuncUsages();
		for (int KindIndex = 0; KindIndex < (int)ELWCFunctionKind::Max; ++KindIndex)
		{
			int Usages = LWCFuncUsages[KindIndex];
			if (LWCFuncUsages[KindIndex] > 0)
			{
				Canvas->DrawShadowedString(
					5,
					DrawPositionY,
					*FString::Printf(TEXT("LWC %s usages (Est.): %u"), *UEnum::GetDisplayValueAsText((ELWCFunctionKind)KindIndex).ToString(), Usages),
					FontToUse,
					FLinearColor(1,1,0)
				);
				DrawPositionY += SpacingBetweenLines;
			}
		}

		if (bGeneratedNewShaders)
		{
			int32 NumShaders = 0;
			int32 NumPipelines = 0;
			if(FMaterialShaderMap* ShaderMap = MaterialResource->GetGameThreadShaderMap())
			{
				ShaderMap->CountNumShaders(NumShaders, NumPipelines);
			}

			if (NumShaders)
			{
				FString ShaderCountString = FString::Printf(TEXT("Num shaders added: %i"), NumShaders);
				Canvas->DrawShadowedString(5, DrawPositionY, *ShaderCountString, FontToUse, FLinearColor(1, 0.8, 0));
				DrawPositionY += SpacingBetweenLines;
			}
		}
	}

	for(int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
	{
		Canvas->DrawShadowedString(5, DrawPositionY, *FString::Printf(TEXT("[%s] %s"), *FeatureLevelName, *CompileErrors[ErrorIndex]), FontToUse, FLinearColor(1, 0, 0));
		DrawPositionY += SpacingBetweenLines;
	}
}


#undef LOCTEXT_NAMESPACE
