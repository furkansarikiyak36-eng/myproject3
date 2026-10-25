#pragma once

#include "CoreMinimal.h"
#include "StoneBuildingTypes.generated.h"

UENUM(BlueprintType)
enum class EStonePieceType : uint8
{
    RawStone UMETA(DisplayName = "Raw Stone"),
    Block UMETA(DisplayName = "Stone Block"),
    Slab UMETA(DisplayName = "Stone Slab"),
    Column UMETA(DisplayName = "Stone Column"),
    Lintel UMETA(DisplayName = "Stone Lintel"),
    Corner UMETA(DisplayName = "Stone Corner"),
    Other UMETA(DisplayName = "Other")
};

USTRUCT(BlueprintType)
struct FStoneSnapAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    FName AnchorId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    FName CompatibilityTag = FName(TEXT("Stone"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    FTransform LocalTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    bool bCanReceive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Anchors")
    bool bCanConnect = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stone|Anchors")
    bool bOccupied = false;
};
