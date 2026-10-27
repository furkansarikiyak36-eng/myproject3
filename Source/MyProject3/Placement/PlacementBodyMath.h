#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementTypes.h"

/**
 * Geometry of long bodies for the placement kernel, with no world access:
 * stacks, struts, floors, steps, gables and post spacing. Every function works
 * on FPlacementBody, so the same rule serves logs, sticks or ropes.
 */
namespace PlacementBodyMath
{
    /**
     * Distance from a body's start end to its station nearest Along (also
     * measured from the start). Stations sit at the centre of each complete
     * module: 37.5, 112.5 ... for 75 cm. A body shorter than a module has one at
     * its middle.
     */
    MYPROJECT3_API float NearestStation(float LengthCm, float ModuleCm, float Along);

    /**
     * A body on a lying body, on the stations of both: along it (the next row of a
     * wall: the upper stations sit over the lower ones), across it (a corner: a
     * station of each crosses the other), or standing on a station as a post.
     * CarryYawDeg is the direction the body is carried.
     */
    MYPROJECT3_API bool SolveStack(const FPlacementBody& Lower, const FPlacementBody& Upper, const FVector& AimPoint,
        float CarryYawDeg, bool bUpright, float ModuleCm, float Interlock, float PostSeat, FPlacementPose& OutPose);

    /** Smallest angle between two undirected horizontal lines, in degrees (0..90). */
    MYPROJECT3_API float LineYawDistance(float YawA, float YawB);

    /**
     * A body lying across a top point (a post top), centred on it so it overhangs
     * equally both ways. Its direction is the carry direction rounded to StepDeg
     * from ReferenceYawDeg (the line to the nearest neighbour), kept at
     * PreviousYawDeg within HysteresisDeg. When one of OtherTops lies within
     * ToleranceDeg of the carry direction and the body reaches it with MinOverhangCm
     * to spare at each end, it spans both, centred between them (corner to corner).
     */
    MYPROJECT3_API bool SolveCrossOnTop(const FVector& Top, const FPlacementBody& Held, float CarryYawDeg,
        float ReferenceYawDeg, float StepDeg, const float* PreviousYawDeg, float HysteresisDeg,
        const TArray<FVector>& OtherTops, float MinOverhangCm, float ToleranceDeg,
        FPlacementPose& OutPose, int32& OutOtherIndex, float& OutYawDeg);

    /**
     * The bend of an end joint (degrees, + turns left seen from above): the carry
     * direction relative to the target end's outward direction, rounded to StepDeg,
     * at most MaxBendDeg either way. Carried back toward the target it goes on
     * straight (0). PreviousBendDeg is kept within HysteresisDeg.
     */
    MYPROJECT3_API float ChooseBend(float OutwardYawDeg, float CarryYawDeg, float StepDeg, float MaxBendDeg,
        const float* PreviousBendDeg, float HysteresisDeg);

    /**
     * A body joined end to end at a bend (a < or L corner): the two axes meet one
     * target radius inside the target's end, the held end sits one held radius
     * past that corner, both at the target's height. bSourceAtPositiveEnd: the
     * held body's +axis end is the one at the corner.
     */
    MYPROJECT3_API FPlacementPose SolveBentEnd(const FVector& TargetEnd, const FVector& Outward, float TargetRadius,
        const FPlacementBody& Held, float BendDeg, bool bSourceAtPositiveEnd);

    /** A diagonal already lying across a body: where it crosses (cm from the body centre) and which way it leans (+1 or -1). */
    struct FDiagonalCrossing
    {
        float AlongCm = 0.0f;
        float Lean = 1.0f;
    };

    /**
     * A body lying across a lying body at 45 degrees, centred on the station under
     * the aim. Mirror of an existing diagonal: leaning the other way near the
     * mirrored place (about the lower body's centre) it crosses exactly there.
     * Corner to corner: crossing at an end station, it runs exactly to one of
     * Corners (crossing points of other bodies, at their centre height) within
     * ToleranceDeg of the carry direction, centred between the two.
     */
    MYPROJECT3_API bool SolveDiagonalStack(const FPlacementBody& Lower, const FPlacementBody& Upper, const FVector& AimPoint,
        float CarryYawDeg, float ModuleCm, float Interlock, const TArray<FDiagonalCrossing>& Existing,
        const TArray<FVector>& Corners, float MinOverhangCm, float ToleranceDeg, FPlacementPose& OutPose, int32& OutCornerIndex);

    /** How a diagonal body crosses Lower (false when it does not lie on it at about 45 degrees). */
    MYPROJECT3_API bool MeasureDiagonal(const FPlacementBody& Lower, const FPlacementBody& Diagonal, FDiagonalCrossing& Out);

    /**
     * A strut from the side of a post up to the beam resting on it, on the aimed
     * side. It meets the beam under the beam station nearest LegCm from the post
     * (never the post's own station) and goes as far down the post, at 45 degrees.
     * ModuleCm 0 uses LegCm as it is. With MirrorLegCm (a strut already on the other
     * side) it uses exactly that leg, so the pair is symmetric about the post.
     */
    MYPROJECT3_API bool SolveStrut(const FPlacementBody& Post, const FPlacementBody& Beam, const FPlacementBody& Strut,
        const FVector& AimPoint, float LegCm, float ModuleCm, float MinLengthCm, FPlacementPose& OutPose, float& OutLength,
        FVector& OutBeamPoint, const float* MirrorLegCm = nullptr);

    /**
     * The leg of a strut already standing between Post and Beam, and its side along
     * the beam (+1 toward the beam's +axis). False when Strut is no such strut.
     */
    MYPROJECT3_API bool MeasureStrut(const FPlacementBody& Post, const FPlacementBody& Beam, const FPlacementBody& Strut,
        float& OutLegCm, float& OutSide);

    /**
     * A board across two parallel supports A and B at the aimed place along A.
     * Pitch is the spacing of boards (a floor) or of steps (stairs).
     */
    MYPROJECT3_API bool SolveDeck(FVector A0, FVector A1, float RadiusA, const FVector& B0, const FVector& B1, float RadiusB,
        const FPlacementBody& Board, const FVector& AimPoint, float Pitch, bool bStairs, FPlacementPose& OutPose, float& OutSpan);

    /** A single step board in front of a floor edge, on the side of From, set Drop cm lower. */
    MYPROJECT3_API bool SolveStep(const FPlacementBody& Edge, const FPlacementBody& Step, const FVector& AimPoint,
        const FVector& From, float GapCm, float DropCm, FPlacementPose& OutPose);

    /** Gable row Row under a sloped beam; false when the slope is used up. OutLength is the row's length. */
    MYPROJECT3_API bool SolveGableRow(const FPlacementBody& Beam, const FPlacementBody& Held, int32 Row, float MinLengthCm,
        FPlacementPose& OutPose, float& OutLength);

    /**
     * Moves Base so it stands one of the Spans (cm) from the nearest of Feet, when
     * it is within MaxErrorCm of that span. OutIndex is the foot it lines up with.
     */
    MYPROJECT3_API bool SolvePostSpacing(const FVector& Base, const TArray<FVector>& Feet, const TArray<float>& Spans,
        float MaxErrorCm, FVector& OutBase, int32& OutIndex, float& OutSpan);

    /** True when Upper rests on top of the post Post (closest point at or above its top, within reach). */
    MYPROJECT3_API bool RestsOnTop(const FPlacementBody& Post, const FPlacementBody& Upper, float ReachCm = 20.0f);
}
