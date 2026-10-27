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

    /**
     * A strut from the side of a post up to the beam resting on it, on the aimed
     * side. It meets the beam under the beam station nearest LegCm from the post
     * (never the post's own station) and goes as far down the post, at 45 degrees.
     * ModuleCm 0 uses LegCm as it is.
     */
    MYPROJECT3_API bool SolveStrut(const FPlacementBody& Post, const FPlacementBody& Beam, const FPlacementBody& Strut,
        const FVector& AimPoint, float LegCm, float ModuleCm, float MinLengthCm, FPlacementPose& OutPose, float& OutLength,
        FVector& OutBeamPoint);

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
