#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
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
    SquaredBeam     UMETA(DisplayName = "Squared Beam / Kalas"),
    GutterLog       UMETA(DisplayName = "Grooved / Gutter Log"),
    TroughLog       UMETA(DisplayName = "Hollow Trough"),
    NotchedLog      UMETA(DisplayName = "Notched Log"),
    JointPiece      UMETA(DisplayName = "Joint Piece"),
    Other           UMETA(DisplayName = "Other"),

    // Added at the end to preserve existing serialized enum values. Unlike the
    // legacy QuarterLog name, this explicitly describes a cross-section shape.
    QuarterSectionLog UMETA(DisplayName = "Quarter-Round Cross Section")
};


/** Length fraction is independent of the cross-section profile of a log piece. */
UENUM(BlueprintType)
enum class ELogLengthFraction : uint8
{
    FullLength UMETA(DisplayName = "Full Length"),
    ThreeQuarterLength UMETA(DisplayName = "3/4 Length"),
    HalfLength UMETA(DisplayName = "1/2 Length"),
    QuarterLength UMETA(DisplayName = "1/4 Length")
};

/** Cross-section/processing shape is independent of the log's length fraction. */
UENUM(BlueprintType)
enum class ELogCrossSectionShape : uint8
{
    FullRound UMETA(DisplayName = "Full Round"),
    HalfRound UMETA(DisplayName = "Half Round"),
    QuarterRound UMETA(DisplayName = "Quarter Round"),
    HewnFlat UMETA(DisplayName = "Hewn / Flattened"),
    SquareBeam UMETA(DisplayName = "Squared Beam"),
    Grooved UMETA(DisplayName = "Grooved"),
    Trough UMETA(DisplayName = "Hollow Trough"),
    Plank UMETA(DisplayName = "Plank"),
    Notched UMETA(DisplayName = "Notched"),
    Joint UMETA(DisplayName = "Joint Piece"),
    Other UMETA(DisplayName = "Other")
};

/** Orthogonal geometry metadata used alongside the legacy combined ELogPieceType. */
USTRUCT(BlueprintType)
struct FLogPieceGeometryState
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Geometry")
    ELogLengthFraction LengthFraction = ELogLengthFraction::FullLength;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Log Piece|Geometry")
    ELogCrossSectionShape CrossSectionShape = ELogCrossSectionShape::FullRound;
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

/** Authored asset bundle for one nominal log-mesh family. Set IDs are explicit; filename length is not guessed. */
USTRUCT(BlueprintType)
struct FLogProcessingMeshSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FName VariantSetId = NAME_None;

    /** Descriptive label only (e.g. an asset family name); not treated as measured centimeters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FName NominalLengthLabel = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> FullRoundMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> SplitHalfMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> QuarterSectionMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> HewnMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> GutterMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> TroughMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> SquaredBeamMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> PlankMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FVector SplitHalfCenterOffsetLocal = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FRotator SplitHalfChildRotationOffsetLocal = FRotator(0.0f, 0.0f, 180.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FVector QuarterCenterOffsetLocal = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FRotator QuarterChildRotationOffsetLocal = FRotator(0.0f, 180.0f, 0.0f);
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
