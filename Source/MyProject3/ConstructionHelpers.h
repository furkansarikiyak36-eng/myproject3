#pragma once

#include "CoreMinimal.h"
#include "LogBuildingTypes.h"

class AActor;
class APawn;
class UObject;
class ALogBuildPiece;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;
enum class EWorldItemOwnerKind : uint8;

/** Shared geometry, ledger and spawning helpers for construction features. */
namespace MyProject3Construction
{
    /** World-space shape of a log: centre line, half length and radius (cm). */
    struct FLogGeometry
    {
        FVector Centre = FVector::ZeroVector;
        FVector Axis = FVector::XAxisVector;
        float HalfLength = 0.0f;
        float Radius = 0.0f;

        FVector EndA() const { return Centre - Axis * HalfLength; }
        FVector EndB() const { return Centre + Axis * HalfLength; }
        float Length() const { return 2.0f * HalfLength; }
    };

    MYPROJECT3_API FVector AxisVector(ELogLongitudinalAxis Axis);
    MYPROJECT3_API bool GetLogGeometry(const ALogBuildPiece* Log, FLogGeometry& Out);
    MYPROJECT3_API bool IsVerticalLog(const ALogBuildPiece* Log, float MinUp = 0.8f);
    MYPROJECT3_API bool IsHorizontalLog(const ALogBuildPiece* Log, float MaxUp = 0.3f);
    /** Closest point to Point on the segment From..To. */
    MYPROJECT3_API FVector ClosestPointOnSegment(const FVector& Point, const FVector& From, const FVector& To);

    /** Rotation that lays the log's own long axis along WorldAxis with UpHint as its up. */
    MYPROJECT3_API FQuat MakeLogRotation(const ALogBuildPiece* Log, const FVector& WorldAxis, const FVector& UpHint);
    /** Actor transform that puts the log's mesh centre at WorldCentre with the given rotation. */
    MYPROJECT3_API FTransform MakeLogTransform(const ALogBuildPiece* Log, const FVector& WorldCentre, const FQuat& Rotation);

    MYPROJECT3_API bool GetPieceOwnerKind(const AActor* Piece, EWorldItemOwnerKind& OutKind);
    /** Moves a physical piece through the world-item ledger from whatever owner it has now. */
    MYPROJECT3_API bool TransferPiece(AActor* Piece, EWorldItemOwnerKind To, const FGuid& OwnerId, const FVector& Location);
    /** Records the piece as used up in the ledger and lifecycle, then removes the actor. */
    MYPROJECT3_API bool ConsumePiece(AActor* Piece);

    /** Spawns a loose, world-owned log with the same mesh family and scale as Source. */
    MYPROJECT3_API ALogBuildPiece* SpawnLogCopy(ALogBuildPiece* Source, const FTransform& Transform);
    /**
     * Shortens a log to the part between FromAlong and ToAlong (cm, measured along its
     * axis from the current centre). The mesh is scaled along its length.
     */
    MYPROJECT3_API bool TrimLogToRange(ALogBuildPiece* Log, float FromAlong, float ToAlong);
    /**
     * Cuts Log to KeepLength from its EndA side and returns the rest as a new loose
     * piece, or nullptr when nothing is cut off.
     */
    MYPROJECT3_API ALogBuildPiece* CutOffRemainder(ALogBuildPiece* Log, float KeepLength, const FVector& DropLocation);

    /** Shared bounds-volume mass model; density is kg/m^3 and fill factor corrects hollow/rounded bounds. */
    MYPROJECT3_API float EstimateMassKg(const AActor* Piece, float BoundsFillFactor, float DensityKgPerCubicMetre);
    /** Canonical default density by structural kind, using the shared bounds-volume model. */
    MYPROJECT3_API float EstimateMassKg(const AActor* Piece);
    /** A quarter-length or shorter piece: firewood or a board offcut. */
    MYPROJECT3_API bool IsSmallWoodPiece(const ALogBuildPiece* Log);
    /** Split or hewn wood suitable for doors, gates and boards. */
    MYPROJECT3_API bool IsSplitBoard(const ALogBuildPiece* Log);

    MYPROJECT3_API UStaticMesh* LoadEngineShape(const TCHAR* ShapeName);
    MYPROJECT3_API UMaterialInterface* MakeTint(UObject* Outer, const FLinearColor& Colour);
    MYPROJECT3_API UMaterialInterface* GetGhostMaterial();
    MYPROJECT3_API APawn* FindPlayerPawn(const UWorld* World);
}
