#pragma once

#include "CoreMinimal.h"

/**
 * Shared vocabulary of the global placement kernel. Nothing here knows about
 * logs, stones or sticks: each material describes itself through features and
 * bodies (IPlacementFeatureProvider) and the rules live in profiles (data).
 */

/** Where on a piece a connection can be made. */
enum class EPlacementFeatureKind : uint8
{
    /** An end face (a log's cut end). */
    End,
    /** A place along a body (a log's 75 cm module); its direction is solved, not stored. */
    Station,
    /** A flat face (a stone side, a plank top). */
    Face,
    /** An edge (a stone edge). */
    Edge,
    /** A point authored on the mesh or by hand (notch, socket, a stone's bottom). */
    Authored
};

/** The algorithm a profile uses. New behaviour should fit one of these first. */
enum class EPlacementSolver : uint8
{
    /** One source feature onto one target feature. */
    AlignOne,
    /** Lies parallel to or across a target body on its grid (stacks, floors, steps, gables). */
    AlignParallel,
    /** Spans two bodies at once (struts, beams). */
    SpanTwo,
    /** Rests on a surface (posts on the ground). */
    RestOnSurface
};

/** How the angle the player roughly holds becomes the angle used. */
enum class EPlacementAnglePolicy : uint8
{
    /** One angle only. */
    Fixed,
    /** A list of allowed angles; the held angle snaps to the nearest. */
    Grid,
    /** Any angle between Min and Max, rounded to Step. */
    Range
};

enum class EPlacementOperation : uint8
{
    Connect,
    SurfacePlace,
    Receiver,
    Voxel,
    Process
};

enum class EPlacementPhysics : uint8
{
    /** Placed and still; structural links hold it. */
    Static,
    /** Physics on, it falls and settles; no structural link. */
    Rest,
    /** Joined at one end, the free end swings. Only from a profile that asks for it. */
    Hinge
};

enum class EPlacementLinkRole : uint8
{
    /** The piece rests on the target (the graph parent). */
    Primary,
    /** The piece also rests on the target. */
    Secondary,
    /** The target also rests on the piece (a beam on a new strut). */
    Carries
};

/** Feature id of a link between whole bodies: a structural link with no anchor reserved. */
inline FName PlacementBodyFeatureId() { return FName(TEXT("Body")); }

/** One place on a piece that can take part in a connection. Built on request, never an actor. */
struct FPlacementFeature
{
    /** Stable within one geometry revision: "End.A", "Station.03", or an authored id. */
    FName Id = NAME_None;
    EPlacementFeatureKind Kind = EPlacementFeatureKind::Authored;
    /** Compatibility tag (for example LogEnd). */
    FName Tag = NAME_None;
    /** Frame relative to the piece; X points out of the connection. */
    FTransform LocalFrame = FTransform::Identity;
    /** The same frame in the world. */
    FTransform WorldFrame = FTransform::Identity;
    /** The incoming piece is turned 180 degrees so the two pieces extend away from each other. */
    bool bFlipIncoming = true;
    bool bCanSource = false;
    bool bCanReceive = true;
    bool bOccupied = false;
    /**
     * A station on a body: the frame lies on the body's axis (X along it) and the
     * side a joint uses is solved from where the player aims, RadiusCm out.
     */
    bool bRadialFree = false;
    /** Contact radius measured in world centimetres for a station feature. */
    float RadiusCm = 0.0f;
    /** 0 = continuous round-surface direction; positive values quantize the radial face direction. */
    float RadialStepDeg = 90.0f;
};

/** A long piece seen as a line with a thickness: logs, planks, sticks, ropes. */
struct FPlacementBody
{
    FVector Centre = FVector::ZeroVector;
    /** Unit vector along the body. */
    FVector Axis = FVector::XAxisVector;
    float HalfLength = 0.0f;
    float Radius = 0.0f;

    FVector EndA() const { return Centre - Axis * HalfLength; }
    FVector EndB() const { return Centre + Axis * HalfLength; }
    float Length() const { return 2.0f * HalfLength; }
    FVector Top() const { return EndA().Z > EndB().Z ? EndA() : EndB(); }
    FVector Bottom() const { return EndA().Z < EndB().Z ? EndA() : EndB(); }
    bool IsUpright(float MinUp = 0.8f) const { return FMath::Abs(Axis.Z) >= MinUp; }
    bool IsLying(float MaxUp = 0.3f) const { return FMath::Abs(Axis.Z) <= MaxUp; }
};

/** Where a body should go: its centre, its axis and which way is up. */
struct FPlacementPose
{
    FVector Centre = FVector::ZeroVector;
    FVector Axis = FVector::XAxisVector;
    FVector Up = FVector::UpVector;
};

/** A feature named by the piece, its id and the geometry revision it was read at. */
struct FPlacementFeatureRef
{
    TWeakObjectPtr<AActor> Piece;
    FName FeatureId = NAME_None;
    uint32 Revision = 0;

    bool IsSet() const { return Piece.IsValid() && !FeatureId.IsNone(); }
    bool IsBody() const { return FeatureId == PlacementBodyFeatureId(); }
    bool operator==(const FPlacementFeatureRef& Other) const
    {
        return Piece == Other.Piece && FeatureId == Other.FeatureId && Revision == Other.Revision;
    }
};

struct FPlacementLink
{
    FPlacementFeatureRef Source;
    FPlacementFeatureRef Target;
    EPlacementLinkRole Role = EPlacementLinkRole::Primary;
};

/** What the player (or the helper) wants: the input to Solve. */
struct FPlacementIntent
{
    AActor* Held = nullptr;
    /** The piece under the crosshair (the post or beam, the support, the edge ...). */
    AActor* AimActor = nullptr;
    FVector AimPoint = FVector::ZeroVector;
    FVector ViewDirection = FVector::ForwardVector;
    /** Where the one placing stands (steps go on that side). */
    FVector PlacerLocation = FVector::ZeroVector;
    /** The angle the player roughly holds, in degrees; the solver rounds it per profile. */
    float HeldAngleDeg = 0.0f;
    /** The direction the player carries the piece (yaw, degrees). */
    float CarryYawDeg = 0.0f;
    /** The player holds the piece upright (post mode). */
    bool bUpright = false;
    /** Pieces to consider; empty means AimActor only. */
    TArray<AActor*> Candidates;
    /** Only these profiles; empty means every enabled Connect profile. */
    TArray<FName> AllowedProfiles;
    /** Per-call values that win over the profile's params (designer tuning on a component). */
    TMap<FName, float> ParamOverrides;
};

/** The single result of Solve. The preview draws it and Commit applies exactly it. */
struct FPlacementSolution
{
    bool bValid = false;
    FName ProfileId = NAME_None;
    /** Where the held piece goes. */
    FTransform PieceTransform = FTransform::Identity;
    TArray<FPlacementLink> Links;
    /** The angle the solver chose (degrees), kept for hysteresis. */
    float AngleDeg = 0.0f;
    /** Lower is better; only valid candidates are scored. */
    float Score = TNumericLimits<float>::Max();
    /** Why the solution is not valid; shown to the player. */
    FString Reason;
    /** Geometry revision of the held piece when solved. */
    uint32 HeldRevision = 0;
    /** Length the piece must be cut to (0: as it is). */
    float RequiredLengthCm = 0.0f;
    /** Distance between the two supports, when there are two. */
    float SpanCm = 0.0f;
    /** A point of interest for the preview (where a strut meets the beam, a post's partner). */
    FVector MarkerPoint = FVector::ZeroVector;

    const FPlacementLink* GetPrimaryLink() const { return FindLink(EPlacementLinkRole::Primary); }
    const FPlacementLink* FindLink(EPlacementLinkRole Role) const
    {
        for (const FPlacementLink& Link : Links)
            if (Link.Role == Role)
                return &Link;
        return nullptr;
    }
    AActor* GetTarget(EPlacementLinkRole Role) const
    {
        const FPlacementLink* Link = FindLink(Role);
        return Link ? Link->Target.Piece.Get() : nullptr;
    }
};
