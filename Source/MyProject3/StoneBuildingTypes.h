#pragma once

#include "CoreMinimal.h"
#include "StoneBuildingTypes.generated.h"

class UStaticMesh;

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

UENUM(BlueprintType)
enum class EStoneShapeAction : uint8
{
    Block UMETA(DisplayName = "Shape Block"),
    Slab UMETA(DisplayName = "Shape Slab"),
    Column UMETA(DisplayName = "Shape Column"),
    Lintel UMETA(DisplayName = "Shape Lintel"),
    Corner UMETA(DisplayName = "Shape Corner")
};

/** Exact-source-mesh outputs; configured sets are authoritative and fail closed. */
USTRUCT(BlueprintType)
struct FStoneProcessingMeshSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> SourceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> BlockMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> SlabMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> ColumnMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> LintelMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stone|Processing")
    TObjectPtr<UStaticMesh> CornerMesh = nullptr;
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
