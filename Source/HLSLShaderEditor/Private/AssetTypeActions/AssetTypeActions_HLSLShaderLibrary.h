// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_HLSLShaderLibrary : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_HLSLShaderLibrary() = default;

	//~ Begin IAssetTypeActions Interface
	virtual FText GetName() const override { return INVTEXT("HLSL Shader Library"); }
	virtual uint32 GetCategories() override
	{
		return EAssetTypeCategories::Materials;
	}
	virtual FColor GetTypeColor() const override { return FColor(100, 0, 175); }
	virtual UClass* GetSupportedClass() const override;

	virtual bool HasActions(const TArray<UObject*>&InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>&InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;
	//~ End IAssetTypeActions Interface
};