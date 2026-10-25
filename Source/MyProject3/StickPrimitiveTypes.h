#pragma once

#include "CoreMinimal.h"
#include "StickPrimitiveTypes.generated.h"

UENUM(BlueprintType)
enum class EStickPrimitiveState : uint8
{
    Raw,
    Broken,
    Sharpened,
    DoubleEndedStake,
    PrimitiveTool,
    Spear,
    Bow,
    Arrow,
    Torch,
    BarrierPart,
    StoragePart,
    TrapComponent,
    AssembledStructure,
    Salvaged
};

UENUM(BlueprintType)
enum class EPrimitiveTrapState : uint8
{
    Incomplete,
    Unplaced,
    Grounded,
    Armed,
    Triggered,
    Disarmed
};

USTRUCT(BlueprintType)
struct FPrimitiveRecipeRequirement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Recipe")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Recipe", meta=(ClampMin="1"))
    int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct FPrimitiveAnchor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Anchors")
    FName AnchorId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Anchors")
    FName CompatibilityTag = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive|Anchors")
    FTransform LocalTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Primitive|Anchors")
    bool bOccupied = false;
};
