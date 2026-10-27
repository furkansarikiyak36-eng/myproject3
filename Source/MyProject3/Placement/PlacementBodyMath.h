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
     * A body on a lying body: along it (the next row of a wall), across it (a
     * corner), or standing on it as a post. Grid positions are counted from the
     * lower body's -axis end. CarryYawDeg is the direction the body is carried.
     */
    MYPROJECT3_API bool SolveStack(const FPlacementBody& Lower, const FPlacementBody& Upper, const FVector& AimPoint,
        float CarryYawDeg, bool bUpright, float GridCm, float Interlock, float PostSeat, FPlacementPose& OutPose);

    /**
     * A strut from the side of a post up to the beam resting on it, on the aimed
     * side: from LegCm down the post to LegCm along the beam.
     */
    MYPROJECT3_API bool SolveStrut(const FPlacementBody& Post, const FPlacementBody& Beam, const FPlacementBody& Strut,
        const FVector& AimPoint, float LegCm, float MinLengthCm, FPlacementPose& OutPose, float& OutLength, FVector& OutBeamPoint);

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
