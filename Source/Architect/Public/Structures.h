// Copyright Interlvper. All rights reserved.

#pragma once
#include "Structures.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct ARCHITECT_API FNormalSettings
{

	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Normal Settings")
	float Scale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Normal Settings")
	float High = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Normal Settings")
	float Medium = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Normal Settings")
	float Low = 1.0f;
};

USTRUCT(Blueprintable, BlueprintType)
struct ARCHITECT_API FAOSettings
{

	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AO Settings")
	float BlurScale = 4.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AO Settings")
	float Contrast = 5.0f;
};