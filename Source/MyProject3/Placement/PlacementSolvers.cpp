#include "Placement/PlacementSolvers.h"

#include "Placement/PlacementProfiles.h"

namespace PlacementMath
{
    float WrapDegrees(float Degrees)
    {
        float Wrapped = FMath::Fmod(Degrees, 360.0f);
        if (Wrapped < 0.0f)
            Wrapped += 360.0f;
        return Wrapped >= 360.0f ? 0.0f : Wrapped;
    }

    float AngleDistance(float A, float B)
    {
        const float Difference = FMath::Abs(WrapDegrees(A) - WrapDegrees(B));
        return FMath::Min(Difference, 360.0f - Difference);
    }

    namespace
    {
        void CollectAllowed(const FPlacementProfile& Profile, TArray<float>& Out)
        {
            switch (Profile.AnglePolicy)
            {
            case EPlacementAnglePolicy::Fixed:
                Out.Add(Profile.AnglesDeg.Num() > 0 ? Profile.AnglesDeg[0] : 0.0f);
                break;
            case EPlacementAnglePolicy::Grid:
                Out.Append(Profile.AnglesDeg);
                break;
            case EPlacementAnglePolicy::Range:
                for (float Angle = Profile.AngleMinDeg; Angle <= Profile.AngleMaxDeg + KINDA_SMALL_NUMBER;
                     Angle += Profile.AngleStepDeg)
                    Out.Add(Angle);
                break;
            }
        }
    }

    float ChooseAngle(const FPlacementProfile& Profile, float HeldDeg, const float* PreviousDeg)
    {
        TArray<float> Allowed;
        CollectAllowed(Profile, Allowed);
        if (Allowed.Num() == 0)
            return 0.0f;
        float Best = Allowed[0];
        float BestDistance = AngleDistance(HeldDeg, Best);
        for (const float Angle : Allowed)
        {
            const float Distance = AngleDistance(HeldDeg, Angle);
            if (Distance < BestDistance)
            {
                Best = Angle;
                BestDistance = Distance;
            }
        }
        // Keep the previous angle until the held angle is clearly nearer the new one.
        if (PreviousDeg)
        {
            for (const float Angle : Allowed)
            {
                if (AngleDistance(Angle, *PreviousDeg) < 0.01f &&
                    AngleDistance(HeldDeg, Angle) <= BestDistance + Profile.AngleHysteresisDeg)
                    return Angle;
            }
        }
        return Best;
    }

    FTransform AlignOne(const FTransform& SourceLocalFrame, const FVector& HeldScale,
        const FTransform& TargetWorldFrame, bool bFlipIncoming, float AngleDeg)
    {
        // Wanted world rotation of the joint: the target frame, turned about the
        // connection axis (its X), then optionally facing away from the target.
        FQuat Joint = TargetWorldFrame.GetRotation();
        if (!FMath::IsNearlyZero(AngleDeg))
            Joint = Joint * FQuat(FVector::XAxisVector, FMath::DegreesToRadians(AngleDeg));
        if (bFlipIncoming)
            Joint = Joint * FQuat(FVector::UpVector, PI);
        // Actor rotation that turns the source frame onto the joint, and the actor
        // position that brings the (scaled) source point onto the target point.
        const FQuat ActorRotation = Joint * SourceLocalFrame.GetRotation().Inverse();
        const FVector SourceOffset = ActorRotation.RotateVector(SourceLocalFrame.GetLocation() * HeldScale);
        return FTransform(ActorRotation, TargetWorldFrame.GetLocation() - SourceOffset, HeldScale);
    }

    void ComputeStationOffsets(float LengthCm, float ModuleCm, TArray<float>& OutOffsets)
    {
        OutOffsets.Reset();
        if (ModuleCm <= KINDA_SMALL_NUMBER || LengthCm <= KINDA_SMALL_NUMBER)
            return;
        // A small tolerance keeps a 599.9 cm log at 8 stations.
        const int32 Count = FMath::FloorToInt((LengthCm + 0.5f) / ModuleCm);
        for (int32 Index = 0; Index < Count; ++Index)
            OutOffsets.Add(ModuleCm * (Index + 0.5f));
    }
}
