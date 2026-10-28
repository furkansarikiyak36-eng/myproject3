#pragma once

#include "CoreMinimal.h"
#include "StickBuildingTypes.generated.h"

class UStaticMesh;

/** Typed primitive-stick states; this is not an inventory item taxonomy. */
UENUM(BlueprintType)
enum class EStickPieceType : uint8
{
    RawStick UMETA(DisplayName = "Raw Stick"),
    ShortStick UMETA(DisplayName = "Short Stick"),
    SharpenedStake UMETA(DisplayName = "Sharpened Stake"),
    DoubleEndedStake UMETA(DisplayName = "Double-Ended Stake"),
    TorchBody UMETA(DisplayName = "Torch Body"),
    SpearShaft UMETA(DisplayName = "Spear Shaft"),
    ArrowShaft UMETA(DisplayName = "Arrow Shaft"),
    FenceStake UMETA(DisplayName = "Fence Stake"),
    TrapComponent UMETA(DisplayName = "Trap Component"),
    Other UMETA(DisplayName = "Other")
};

UENUM(BlueprintType)
enum class EStickProcessingAction : uint8
{
    BreakShort UMETA(DisplayName = "Break Into Short Stick"),
    SharpenOneEnd UMETA(DisplayName = "Sharpen One End"),
    SharpenBothEnds UMETA(DisplayName = "Sharpen Both Ends")
};

/**
 * Exact source-mesh mapping. Once any mappings are configured, unmatched and
 * duplicate source meshes fail closed rather than selecting a guessed output.
 */
USTRUCT(BlueprintType)
struct FStickProcessingMeshSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick Processing|Mesh Set")
    TObjectPtr<UStaticMesh> SourceMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick Processing|Mesh Set")
    TObjectPtr<UStaticMesh> ShortStickMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick Processing|Mesh Set")
    TObjectPtr<UStaticMesh> SharpenedStakeMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stick Processing|Mesh Set")
    TObjectPtr<UStaticMesh> DoubleEndedStakeMesh = nullptr;
};
