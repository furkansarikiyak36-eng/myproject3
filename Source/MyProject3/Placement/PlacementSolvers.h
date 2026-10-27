#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementTypes.h"

struct FPlacementProfile;

/**
 * Pure math of the placement kernel: no world access, no actors, so every rule
 * can be tested on its own. The player never sees any of these numbers.
 */
namespace PlacementMath
{
    /** Wraps an angle to [0, 360). */
    MYPROJECT3_API float WrapDegrees(float Degrees);
    /** Shortest difference between two angles, in [0, 180]. */
    MYPROJECT3_API float AngleDistance(float A, float B);

    /**
     * The angle the profile allows that is nearest to HeldDeg. With a previous
     * angle the choice only changes once HeldDeg is past the halfway point by the
     * profile's hysteresis, so the preview does not flicker between two angles.
     */
    MYPROJECT3_API float ChooseAngle(const FPlacementProfile& Profile, float HeldDeg,
        const float* PreviousDeg = nullptr);

    /**
     * Actor transform that puts the source feature (LocalFrame on the held piece)
     * on the target feature's world frame, turned AngleDeg about the connection
     * axis. The held piece keeps HeldScale: a shortened log stays short, and the
     * target's scale never carries over.
     */
    MYPROJECT3_API FTransform AlignOne(const FTransform& SourceLocalFrame, const FVector& HeldScale,
        const FTransform& TargetWorldFrame, bool bFlipIncoming, float AngleDeg);

    /**
     * Centres of the body stations of a piece LengthCm long, measured from its
     * start: one per complete module (75 cm: 37.5, 112.5 ...). A remainder
     * shorter than a module gets none.
     */
    MYPROJECT3_API void ComputeStationOffsets(float LengthCm, float ModuleCm, TArray<float>& OutOffsets);
}
