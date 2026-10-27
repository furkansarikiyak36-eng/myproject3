#pragma once

#include "CoreMinimal.h"
#include "LogBuildingTypes.generated.h"

class UStaticMesh;

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

/** Longitudinal amount of the original log represented by this piece. */
UENUM(BlueprintType)
enum class ELogLengthFraction : uint8
{
    FullLength UMETA(DisplayName = "Full Length"),
    ThreeQuarterLength UMETA(DisplayName = "3/4 Length"),
    HalfLength UMETA(DisplayName = "1/2 Length"),
    QuarterLength UMETA(DisplayName = "1/4 Length"),
    Other UMETA(DisplayName = "Other / Authored Length"),

    // Appended so serialized values stay stable. Matches the 75 cm mesh family
    // cut from a 150 cm quarter-length piece.
    EighthLength UMETA(DisplayName = "1/8 Length")
};

/** Cross-section/processing shape, independent of longitudinal length. */
UENUM(BlueprintType)
enum class ELogCrossSectionProfile : uint8
{
    Round UMETA(DisplayName = "Round"),
    HalfRound UMETA(DisplayName = "Half-Round"),
    QuarterRound UMETA(DisplayName = "Quarter-Round"),
    Hewn UMETA(DisplayName = "Hewn / Flattened"),
    SquareBeam UMETA(DisplayName = "Squared Beam"),
    GroovedHalfRound UMETA(DisplayName = "Grooved Half-Round"),
    Trough UMETA(DisplayName = "Hollow Trough"),
    Plank UMETA(DisplayName = "Plank"),
    Notched UMETA(DisplayName = "Notched"),
    Joint UMETA(DisplayName = "Joint Piece"),
    Other UMETA(DisplayName = "Other / Authored Profile"),

    // Appended so serialized values stay stable. Second hewn face (Hewn2 meshes);
    // a squared beam can only be made from this profile.
    HewnTwoFaces UMETA(DisplayName = "Hewn / Two Faces")
};


/**
 * Authored mesh mapping for a specific combination of length and cross-section.
 * Non-full-length processed pieces must provide an exact entry so the system
 * never reuses a full-length mesh for a shorter resource.
 */
USTRUCT(BlueprintType)
struct FLogGeometryVariantMesh
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    ELogLengthFraction LengthFraction = ELogLengthFraction::FullLength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    ELogCrossSectionProfile CrossSectionProfile = ELogCrossSectionProfile::Round;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    TObjectPtr<UStaticMesh> Mesh = nullptr;

    /** Required for a split output profile; center-to-center separation in local space. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    FVector SecondPieceCenterOffsetLocal = FVector::ZeroVector;

    /** Mesh-specific complementary orientation for the second split output. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Geometry")
    FRotator SecondPieceRotationOffsetLocal = FRotator::ZeroRotator;
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
    Diagonal   UMETA(DisplayName = "Diagonal"),
    // Appended so saved values stay valid.
    Floor      UMETA(DisplayName = "Floor / Roof / Bridge / Ramp"),
    Stairs     UMETA(DisplayName = "Stairs")
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
    Plank     UMETA(DisplayName = "Plank"),

    // Appended so existing serialized ELogCutAction values remain unchanged.
    SplitLengthHalf UMETA(DisplayName = "Cut Full Length into Two Half-Length Pieces"),
    SplitLengthThreeQuarter UMETA(DisplayName = "Cut Full Length into 3/4 + 1/4 Length"),
    SplitHalfLengthIntoQuarters UMETA(DisplayName = "Cut Half-Length into Two Quarter-Length Pieces"),
    SplitQuarterLengthIntoEighths UMETA(DisplayName = "Cut Quarter-Length into Two Eighth-Length Pieces"),
    HewSecondFace UMETA(DisplayName = "Hew the Second Face")
};

USTRUCT(BlueprintType)
struct FLogLengthCutVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    ELogCutAction Action = ELogCutAction::SplitLengthHalf;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    ELogLengthFraction SourceLengthFraction = ELogLengthFraction::FullLength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    ELogCrossSectionProfile CrossSectionProfile = ELogCrossSectionProfile::Round;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    ELogLengthFraction FirstOutputLengthFraction = ELogLengthFraction::HalfLength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    ELogLengthFraction SecondOutputLengthFraction = ELogLengthFraction::HalfLength;

    /** Output centers relative to the original actor pivot, in local space. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    FVector FirstOutputCenterOffsetLocal = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    FVector SecondOutputCenterOffsetLocal = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    FRotator FirstOutputRotationOffsetLocal = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Length Cutting")
    FRotator SecondOutputRotationOffsetLocal = FRotator::ZeroRotator;
};

/**
 * Authored asset bundle for one nominal log-mesh family.
 * VariantSetId is a stable selector; NominalLengthLabel is descriptive metadata,
 * not an assumption that filename digits are measured centimeters.
 * When an actor has any set configured, all geometry and length-cut data is
 * resolved from the selected set only; actor-level tables are not consulted.
 */
USTRUCT(BlueprintType)
struct FLogProcessingMeshSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FName VariantSetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    FName NominalLengthLabel = NAME_None;

    /** Full-length round mesh of this family; also identifies the set on an unprocessed actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TObjectPtr<UStaticMesh> BaseRoundMesh = nullptr;

    /** One row for every other exact length + cross-section combination this family supports. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TArray<FLogGeometryVariantMesh> MeshVariants;

    /** Mesh-specific longitudinal cut layouts for this family. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Piece|Mesh Set")
    TArray<FLogLengthCutVariant> LengthCutVariants;
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

    /**
     * This is a longitudinal station, not a pre-authored radial socket.
     * Resolve its contact frame from the hit direction at runtime.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bRuntimeRadialFrame = false;

    /** Only explicit hinge profiles may allow an unsupported one-end attachment to swing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Log Snap")
    bool bAllowsHinge = false;

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
