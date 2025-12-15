// Copyright Interlvper. All rights reserved.

#include "ArchitectBPLibrary.h"
#include "EditorAssetLibrary.h"
#include "EditorUtilityLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/SavePackage.h"

UArchitectBPLibrary::UArchitectBPLibrary(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

void UArchitectBPLibrary::GenerateMapsForSelectedTextures(FNormalSettings NormalSettings, FAOSettings AOSettings)
{
	TArray<FAssetData> Assets;
	GEditor->GetContentBrowserSelections(Assets);

	for (const FAssetData& Asset : Assets)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset.GetAsset()))
		{
			GenerateMapsForTexture(Texture, NormalSettings, AOSettings);
		}
	}
}

void UArchitectBPLibrary::GenerateMapsForTexture(UTexture2D* AlbedoTexture, FNormalSettings& NormalSettings, FAOSettings& AOSettings)
{
    if (!AlbedoTexture || !AlbedoTexture->Source.IsValid())
    {
        return;
    }

    const int32 Height = AlbedoTexture->Source.GetSizeY();
    const int32 Width = AlbedoTexture->Source.GetSizeX();

    TArray64<uint8> RawData;
    AlbedoTexture->Source.GetMipData(RawData, 0);

    ETextureSourceFormat Format = AlbedoTexture->Source.GetFormat();
    int32 BytesPerPixel = 4;

    switch (Format)
    {
        case TSF_G8:
            BytesPerPixel = 1;
            break;
        case TSF_BGRA8:
            BytesPerPixel = 4;
            break;
        case TSF_RGBA16:
            BytesPerPixel = 8;
            break;
        case TSF_RGBA16F:
            BytesPerPixel = 8;
            break;
        default:
            UE_LOG(LogTemp, Error, TEXT("Unsupported format: %d"), (int32)Format);
            return;
    }

    if (RawData.Num() < Width * Height * BytesPerPixel)
    {
        UE_LOG(LogTemp, Error, TEXT("RawData size mismatch for %s"), *AlbedoTexture->GetName());
        return;
    }

    TArray<float> GrayscaleMap;
    GrayscaleMap.SetNum(Width * Height);

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            int32 Index = (Y * Width + X) * BytesPerPixel;

            if (Index + 2 >= RawData.Num())
            {
                continue;
            }

            uint8 R = 0, G = 0, B = 0;

            if (Format == TSF_G8)
            {
                R = G = B = RawData[Index];
            }
            else
            {
                B = RawData[Index + 0];
                G = RawData[Index + 1];
                R = RawData[Index + 2];
            }

            GrayscaleMap[Y * Width + X] = (0.299f * R + 0.587f * G + 0.114f * B) / 255.0f;
        }
    }

    // Blur Lambda
    auto Blur = [&](TArray<float>& Input, const int32 Radius) -> TArray<float>
    {
        TArray<float> Output = Input;

        for (int32 Y = 0; Y < Height; ++Y)
        {
            for (int32 X = 0; X < Width; ++X)
            {
                float Sum = 0.0f;
                int32 Count = 0;

                for (int32 DY = -Radius; DY <= Radius; ++DY)
                {
                    for (int32 DX = -Radius; DX <= Radius; ++DX)
                    {
                        const int32 SX = FMath::Clamp(X + DX, 0, Width - 1);
                        const int32 SY = FMath::Clamp(Y + DY, 0, Height - 1);
                        Sum += Input[SY * Width + SX];
                        Count++;
                    }
                }

                Output[Y * Width + X] = Sum / static_cast<float>(Count);
            }
        }

        return Output;
    };

    TArray<float> HighLayer = GrayscaleMap;
    TArray<float> MediumLayer = Blur(GrayscaleMap, 2);
    TArray<float> LowLayer = Blur(GrayscaleMap, 6);

    TArray<float> Composite;
    Composite.SetNum(Width * Height);

    for (int32 Index = 0; Index < GrayscaleMap.Num(); ++Index)
    {
        Composite[Index] = HighLayer[Index] * NormalSettings.High + MediumLayer[Index] * NormalSettings.Medium + LowLayer[Index] * NormalSettings.Low;
    }

    // Ambient Occlusion
    TArray<float> AOPixels;
    AOPixels.SetNum(Width * Height);
    TArray<float> AOBlur = Blur(GrayscaleMap, AOSettings.BlurScale);

    for (int32 Index = 0; Index < GrayscaleMap.Num(); ++Index)
    {
        float Occlusion = GrayscaleMap[Index] - AOBlur[Index];
        Occlusion = 1.0f - FMath::Clamp(Occlusion * AOSettings.Contrast + 0.3f, 0.0f, 1.0f);
        AOPixels[Index] = Occlusion;
    }

    // Normal Map
    TArray<FColor> NormalPixels;
    NormalPixels.SetNum(Width * Height);

    for (int32 Y = 1; Y < Height - 1; ++Y)
    {
        for (int32 X = 1; X < Width - 1; ++X)
        {
            float DX = Composite[Y * Width + (X + 1)] - Composite[Y * Width + (X - 1)];
            float DY = Composite[(Y + 1) * Width + X] - Composite[(Y - 1) * Width + X];

            FVector Normal(-DX * NormalSettings.Scale, -DY * NormalSettings.Scale, 1.0f);
            Normal.Normalize();

            FColor Encoded;
            Encoded.R = FMath::Clamp(static_cast<int32>((Normal.X * 0.5f + 0.5f) * 255.0f), 0, 255);
            Encoded.G = FMath::Clamp(static_cast<int32>((Normal.Y * 0.5f + 0.5f) * 255.0f), 0, 255);
            Encoded.B = FMath::Clamp(static_cast<int32>((Normal.Z * 0.5f + 0.5f) * 255.0f), 0, 255);
            Encoded.A = 255;

            NormalPixels[Y * Width + X] = Encoded;
        }
    }

    for (int32 X = 0; X < Width; ++X)
    {
        NormalPixels[0 * Width + X] = NormalPixels[1 * Width + X];
        NormalPixels[(Height - 1) * Width + X] = NormalPixels[(Height - 2) * Width + X];
    }
    for (int32 Y = 0; Y < Height; ++Y)
    {
        NormalPixels[Y * Width + 0] = NormalPixels[Y * Width + 1];
        NormalPixels[Y * Width + (Width - 1)] = NormalPixels[Y * Width + (Width - 2)];
    }

    // Normal Texture
    FString AlbedoName = AlbedoTexture->GetName();
    FString NormalName = AlbedoName.Replace(TEXT("_Albedo"), TEXT("_Normal"));
    FString PackagePath = FPackageName::GetLongPackagePath(AlbedoTexture->GetOutermost()->GetName());
    
    CreateTextureAsset(NormalPixels, Width, Height, NormalName, PackagePath, TextureCompressionSettings::TC_Normalmap);
    
    // ORDM Texture
    AlbedoName = AlbedoTexture->GetName();
    FString HeightName = AlbedoName.Replace(TEXT("_Albedo"), TEXT("_ORDM"));
    PackagePath = FPackageName::GetLongPackagePath(AlbedoTexture->GetOutermost()->GetName());
    FString FullPackageName = PackagePath + "/" + HeightName;

    TArray<FColor> HeightMapPixels;
    HeightMapPixels.Reserve(Width * Height);
    
    for (int32 Index = 0; Index < Composite.Num(); Index++)
    {
        float Pixel = Composite[Index];
        uint8 AOValue = FMath::Clamp(static_cast<int32>(AOPixels[Index] * 255.0f), 0, 255);
        uint8 HeightValue = FMath::Clamp(static_cast<int32>(Pixel * 255.0f), 0, 255);
        uint8 Roughness = 255 - HeightValue;
        HeightMapPixels.Add(FColor(AOValue, Roughness, HeightValue, 255));
    }
    
    CreateTextureAsset(HeightMapPixels, Width, Height, HeightName, PackagePath, TextureCompressionSettings::TC_Default);
}

UTexture2D* UArchitectBPLibrary::CreateTextureAsset(const TArray<FColor>& Pixels, const int32 Width, const int32 Height, const FString& Name, const FString& PackagePath, TEnumAsByte<enum TextureCompressionSettings> Compression)
{
    const FString FullPackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*FullPackageName);
    
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create package for %s"), *FullPackageName);
        return nullptr;
    }

    Package->FullyLoad();

    UTexture2D* ExistingTexture = FindObject<UTexture2D>(Package, *Name);
    UTexture2D* Texture;

    if (ExistingTexture)
    {
        Texture = ExistingTexture;
        Texture->Modify();
    }
    else
    {
        Texture = NewObject<UTexture2D>(Package, *Name, RF_Public | RF_Standalone);
        Texture->AddToRoot();
    }

    TArray<uint8> RawBytes;
    RawBytes.SetNum(Pixels.Num() * 4);

    for (int32 Index = 0; Index < Pixels.Num(); Index++)
    {
        const FColor& Color = Pixels[Index];
        RawBytes[Index * 4 + 0] = Color.B;
        RawBytes[Index * 4 + 1] = Color.G;
        RawBytes[Index * 4 + 2] = Color.R;
        RawBytes[Index * 4 + 3] = Color.A;
    }

    Texture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, RawBytes.GetData());
    Texture->SRGB = false;
    Texture->CompressionSettings = Compression;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->UpdateResource();

    if (!ExistingTexture)
    {
        FAssetRegistryModule::AssetCreated(Texture);
    }

    Package->MarkPackageDirty();

    const FString FilePath = FPackageName::LongPackageNameToFilename(FullPackageName, FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    if (!UPackage::SavePackage(Package, Texture, *FilePath, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save texture package: %s"), *FilePath);
    }

    return Texture;
}

void UArchitectBPLibrary::BatchCreateMaterialInstances(const FString& MasterMaterialPath, const FString& OutputFolder)
{
    UMaterialInterface* MasterMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MasterMaterialPath));

    if (!MasterMaterial)
    {
        UE_LOG(LogTemp, Error, TEXT("Master material not found at: %s"), *MasterMaterialPath);
        return;
    }

    TArray<FAssetData> SelectedAssets;
    GEditor->GetContentBrowserSelections(SelectedAssets);

    for (const FAssetData& Asset : SelectedAssets)
    {
        UTexture2D* Albedo = Cast<UTexture2D>(Asset.GetAsset());
        if (!Albedo)
        {
            continue;
        }

        FString AlbedoName = Albedo->GetName();
        if (!AlbedoName.EndsWith(TEXT("_Albedo"))) continue;

        FString BaseName = AlbedoName.LeftChop(7);
        FString NormalName = BaseName + TEXT("_Normal");
        FString ORDMName = BaseName + TEXT("_ORDM");
        BaseName = BaseName.RightChop(2);

        FString PackagePath = FPackageName::GetLongPackagePath(Albedo->GetOutermost()->GetName());
        FString NormalPath = PackagePath / NormalName + TEXT(".") + NormalName;
        FString ORDMPath = PackagePath / ORDMName + TEXT(".") + ORDMName;

        UTexture2D* Normal = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *NormalPath));
        UTexture2D* ORDM = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *ORDMPath));

        if (!Normal)
        {
            UE_LOG(LogTemp, Warning, TEXT("Normal map not found for: %s"), *AlbedoName);
            continue;
        }

        FString MIName = TEXT("MI_") + BaseName;
        FString MIPackagePath = OutputFolder / MIName;
        UPackage* Package = CreatePackage(*MIPackagePath);
        Package->FullyLoad();

        UMaterialInstanceConstant* MaterialInstance = FindObject<UMaterialInstanceConstant>(Package, *MIName);

        if (!MaterialInstance)
        {
            MaterialInstance = NewObject<UMaterialInstanceConstant>(Package, *MIName, RF_Public | RF_Standalone | RF_MarkAsRootSet);
        }
        else
        {
            MaterialInstance->SetFlags(RF_Public | RF_Standalone);
        }

        MaterialInstance->SetParentEditorOnly(MasterMaterial);
        MaterialInstance->SetTextureParameterValueEditorOnly(TEXT("Albedo"), Albedo);
        MaterialInstance->SetTextureParameterValueEditorOnly(TEXT("Normal"), Normal);
        MaterialInstance->SetTextureParameterValueEditorOnly(TEXT("ORDM"), ORDM);
        MaterialInstance->PostEditChange();
        MaterialInstance->MarkPackageDirty();
        Package->MarkPackageDirty();

        const FString FilePath = FPackageName::LongPackageNameToFilename(MIPackagePath, FPackageName::GetAssetPackageExtension());

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        SaveArgs.SaveFlags = SAVE_NoError;

        UPackage::SavePackage(Package, MaterialInstance, *FilePath, SaveArgs);

        UE_LOG(LogTemp, Log, TEXT("Material Instance created: %s"), *MIName);
    }
}

void UArchitectBPLibrary::AssignMaterialsToMeshesBySlotName(const FString& MaterialFolder, const FString& MaterialPrefix)
{
    TArray<FAssetData> SelectedAssets;
    GEditor->GetContentBrowserSelections(SelectedAssets);

    for (const FAssetData& Asset : SelectedAssets)
    {
        UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
        if (!Mesh)
        {
            continue;
        }

        bool bAssignedAny = false;

        for (int32 SlotIndex = 0; SlotIndex < Mesh->GetStaticMaterials().Num(); ++SlotIndex)
        {
            const FName& SlotName = Mesh->GetStaticMaterials()[SlotIndex].MaterialSlotName;

            FString MIName = SlotName.ToString();
            FString MIFullPath = MaterialFolder + TEXT("/") + MaterialPrefix + MIName;

            UMaterialInterface* MaterialInstance = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MIFullPath));

            if (!MaterialInstance)
            {
                UE_LOG(LogTemp, Warning, TEXT("Material Instance not found for slot '%s' on mesh '%s'"), *MIName, *Mesh->GetName());
                continue;
            }

            Mesh->SetMaterial(SlotIndex, MaterialInstance);
            bAssignedAny = true;

            UE_LOG(LogTemp, Log, TEXT("Assigned Material Instance '%s' to Slot '%s' on Mesh '%s'"), *MIName, *SlotName.ToString(), *Mesh->GetName());
        }

        if (bAssignedAny)
        {
            Mesh->Modify();
            Mesh->MarkPackageDirty();
            Mesh->PostEditChange();
        }
    }
}

void UArchitectBPLibrary::DisableRecomputeNormals()
{
#if WITH_EDITOR
    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

    for (UObject* Asset : SelectedAssets)
    {
        UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset);
        
        if (!StaticMesh)
        {
            continue;
        }

        FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
        FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;

        BuildSettings.bRecomputeNormals = false;

        StaticMesh->Build(false);
        StaticMesh->MarkPackageDirty();

        FString Path = StaticMesh->GetPathName();
        
        if (!UEditorAssetLibrary::SaveAsset(Path, false))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to save asset: %s"), *Path);
        }
    }
#endif
}