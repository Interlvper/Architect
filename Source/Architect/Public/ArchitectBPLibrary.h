// Copyright Interlvper. All rights reserved.

#pragma once
#include "Structures.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArchitectBPLibrary.generated.h"

/**
 * Contains functions required to generate Normal, Metallic, Roughness, AO, and Heightmaps from an Albedo texture.
 */
UCLASS()
class UArchitectBPLibrary : public UBlueprintFunctionLibrary
{
	
	GENERATED_UCLASS_BODY()

public:
	
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Architect Plugin")
	static void GenerateMapsForSelectedTextures(FNormalSettings NormalSettings, FAOSettings AOSettings);

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Architect Plugin")
	static void BatchCreateMaterialInstances(const FString& MasterMaterialPath, const FString& OutputFolder);

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Architect Plugin")
	static void AssignMaterialsToMeshesBySlotName(const FString& MaterialFolder, const FString& MaterialPrefix);

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Architect Plugin")
	static void DisableRecomputeNormals();

private:
	
	static void GenerateMapsForTexture(UTexture2D* AlbedoTexture, FNormalSettings& NormalSettings, FAOSettings& AOSettings);
	
	static UTexture2D* CreateTextureAsset(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& Name, const FString& PackagePath, TEnumAsByte<enum TextureCompressionSettings> Compression);
};
