#pragma once

#include "CoreMinimal.h"
#include "LogBuildingTypes.generated.h"

UENUM(BlueprintType)
enum class ELogPieceType : uint8
{
    WholeLog        UMETA(DisplayName = "Whole Log"),
    ThreeQuarterLog UMETA(DisplayName = "3/4 Log"),
    HalfLengthLog   UMETA(DisplayName = "1/2 Length Log"),
    QuarterLog      UMETA(DisplayName = "1/4 Log"),
    SplitHalfLog    UMETA(DisplayName = "Split Half Log"),
    Plank           UMETA(DisplayName = "Plank"),
    HewnLog         UMETA(DisplayName = "Hewn Log"),
    GutterLog       UMETA(DisplayName = "Grooved / Gutter Log"),
    NotchedLog      UMETA(DisplayName = "Notched Log"),
    JointPiece      UMETA(DisplayName = "Joint Piece"),
    Other           UMETA(DisplayName = "Other")
};

UENUM(BlueprintType)
enum class ELogLongitudinalAxis : uint8
{
    X UMETA(DisplayName = "X Axis"),
    Y UMETA(DisplayName = "Y Axis"),
    Z UMETA(DisplayName = "Z Axis")
};

UENUM(BlueprintType)
enum class ELogPlacementMode : uint8
{
    Horizontal UMETA(DisplayName = "Horizontal"),
    Vertical   UMETA(DisplayName = "Vertical"),
    Diagonal   UMETA(DisplayName = "Diagonal")
};


UENUM(BlueprintType)
enum class ELogCutAction : uint8
{
    SplitHalf UMETA(DisplayName = "Split Into Half-Rounds"),
    Quarter   UMETA(DisplayName = "Split Into Quarters"),
    Hewn      UMETA(DisplayName = "Hewn / Flatten"),
    Gutter    UMETA(DisplayName = "Cut Groove / Gutter"),
    Trough    UMETA(DisplayName = "Hollow Trough"),
    Kalas     UMETA(DisplayName = "Kalas / Squared Beam"),
    Plank     UMETA(DisplayName = "Plank")
};

USTRUCT(BlueprintType)
struct FLogSnapPoint
{
    GENERATED_BODY()

    // Unique editor-facing name for this connection point.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    FName PointId = NAME_None;

    // Connection family, such as LogEnd, LogSide, CrossLap, or Gutter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    FName SnapTag = FName(TEXT("LogEnd"));

    // Transform relative to the log actor's root. Its forward axis points out
    // of the connection face. For a normal end-to-end joint, incoming rotation
    // is flipped 180 degrees so the two logs extend away from one another.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    FTransform LocalTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bCanSource = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bCanReceive = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bAcceptAnyPieceType = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap",
        meta = (EditCondition = "!bAcceptAnyPieceType"))
    TArray<ELogPieceType> AcceptedPieceTypes;

    // When non-empty, only sources whose SnapTag is in this list can connect.
    // When empty, source and target SnapTag values must match.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    TArray<FName> CompatibleSnapTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bFlipIncomingOrientation = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Log Snap")
    bool bOccupied = false;

    bool AcceptsPieceType(ELogPieceType Type) const
    {
        return bCanReceive && !bOccupied &&
            (bAcceptAnyPieceType || AcceptedPieceTypes.Contains(Type));
    }

    bool AcceptsSnapTag(FName SourceTag) const
    {
        if (CompatibleSnapTags.Num() > 0)
        {
            return CompatibleSnapTags.Contains(SourceTag);
        }
        return SnapTag == SourceTag;
    }
};
