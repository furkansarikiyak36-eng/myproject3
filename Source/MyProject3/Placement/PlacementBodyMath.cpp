#include "Placement/PlacementBodyMath.h"

namespace PlacementBodyMath
{
    float NearestStation(float LengthCm, float ModuleCm, float Along)
    {
        const int32 Count = ModuleCm > KINDA_SMALL_NUMBER ? FMath::FloorToInt((LengthCm + 0.5f) / ModuleCm) : 0;
        if (Count <= 0)
            return 0.5f * LengthCm;
        const int32 Index = FMath::Clamp(FMath::RoundToInt(Along / ModuleCm - 0.5f), 0, Count - 1);
        return ModuleCm * (Index + 0.5f);
    }

    bool SolveStack(const FPlacementBody& Lower, const FPlacementBody& Upper, const FVector& AimPoint,
        float CarryYawDeg, bool bUpright, float ModuleCm, float Interlock, float PostSeat, FPlacementPose& OutPose)
    {
        if (ModuleCm <= 1.0f)
            return false;
        FVector Along(Lower.Axis.X, Lower.Axis.Y, 0.0f);
        if (!Along.Normalize())
            return false;
        // Positions along the lower body, from its centre; its stations are counted from its -axis end.
        const float StartA = -Lower.HalfLength;
        const float AimAlong = FVector::DotProduct(AimPoint - Lower.Centre, Along);
        const float StationAlong = StartA + NearestStation(Lower.Length(), ModuleCm, AimAlong - StartA);
        const FVector Up = FVector::UpVector;

        if (bUpright)
        {
            // A post standing on a station, seated in a shallow notch.
            OutPose.Centre = Lower.Centre + Along * StationAlong + Up * (Lower.Radius * PostSeat + Upper.HalfLength);
            OutPose.Axis = Up;
            OutPose.Up = Along;
            return true;
        }

        const FVector Want = FRotator(0.0f, CarryYawDeg, 0.0f).Vector();
        const float Alignment = FVector::DotProduct(Want, Along);
        const float StackZ = Lower.Centre.Z + (Lower.Radius + Upper.Radius) * Interlock;
        if (FMath::Abs(Alignment) >= 0.7071f)
        {
            // Along the lower body, the next row of a wall: it starts on a module
            // boundary, so every upper station sits over a lower one. It may run
            // past the lower body's end.
            const float Start = StartA + FMath::RoundToFloat((AimAlong - Upper.HalfLength - StartA) / ModuleCm) * ModuleCm;
            OutPose.Centre = Lower.Centre + Along * (Start + Upper.HalfLength);
            OutPose.Axis = Along * FMath::Sign(Alignment);
        }
        else
        {
            // Across it, as at a corner: a station of the upper body crosses the
            // lower station under the crosshair.
            FVector Across = FVector::CrossProduct(Up, Along);
            if (FVector::DotProduct(Across, Want) < 0.0f)
                Across = -Across;
            const FVector Crossing = Lower.Centre + Along * StationAlong;
            // Measured from the upper body's far (+Across) end, which lies past the crossing.
            const float FromFarEnd = Upper.HalfLength - FVector::DotProduct(AimPoint - Crossing, Across);
            const float UpperStation = NearestStation(Upper.Length(), ModuleCm, FromFarEnd);
            OutPose.Centre = Crossing + Across * (Upper.HalfLength - UpperStation);
            OutPose.Axis = Across;
        }
        OutPose.Centre.Z = StackZ;
        OutPose.Up = Up;
        return true;
    }

    bool SolveStrut(const FPlacementBody& Post, const FPlacementBody& Beam, const FPlacementBody& Strut,
        const FVector& AimPoint, float LegCm, float ModuleCm, float MinLengthCm, FPlacementPose& OutPose, float& OutLength,
        FVector& OutBeamPoint)
    {
        const FVector Top = Post.Top();
        FVector Along = Beam.Axis;
        Along.Z = 0.0f;
        Along = Along.GetSafeNormal();
        if (Along.IsNearlyZero())
            return false;
        if (FVector::DotProduct(AimPoint - Top, Along) < 0.0f)
            Along = -Along;
        const FVector BeamAxis = Beam.Axis.GetSafeNormal();
        const FVector OnBeam = FMath::ClosestPointOnSegment(Top, Beam.EndA(), Beam.EndB());
        const float PostFromStart = FVector::DotProduct(OnBeam - Beam.EndA(), BeamAxis);
        const float Direction = FVector::DotProduct(Along, BeamAxis) > 0.0f ? 1.0f : -1.0f;
        // Where the strut meets the beam: on the beam station nearest one leg out.
        float MeetFromStart = PostFromStart + Direction * LegCm;
        if (ModuleCm > KINDA_SMALL_NUMBER)
        {
            MeetFromStart = NearestStation(Beam.Length(), ModuleCm, MeetFromStart);
            if (FMath::Abs(MeetFromStart - PostFromStart) < 0.5f * ModuleCm)
                MeetFromStart = NearestStation(Beam.Length(), ModuleCm, MeetFromStart + Direction * ModuleCm);
        }
        const float Leg = FMath::Abs(MeetFromStart - PostFromStart);
        if (MeetFromStart < 0.0f || MeetFromStart > Beam.Length() || Leg < 0.5f * ModuleCm ||
            FVector::DotProduct(BeamAxis * (MeetFromStart - PostFromStart), Along) <= 0.0f)
            return false;
        const FVector Meet = Beam.EndA() + BeamAxis * MeetFromStart;
        // Ends: under the beam at that station, and on the post's side face as far below the beam.
        const FVector Low = Top - FVector(0, 0, Leg + Beam.Radius) + Along * (Post.Radius + Strut.Radius * 0.5f);
        const FVector High = Meet - FVector(0, 0, Beam.Radius + Strut.Radius * 0.5f);
        const FVector Axis = High - Low;
        OutLength = Axis.Size();
        if (OutLength < MinLengthCm)
            return false;
        OutBeamPoint = Meet - FVector(0, 0, Beam.Radius);
        OutPose.Centre = 0.5f * (Low + High);
        OutPose.Axis = Axis.GetSafeNormal();
        OutPose.Up = FVector::CrossProduct(OutPose.Axis, FVector::CrossProduct(FVector::UpVector, OutPose.Axis)).GetSafeNormal();
        return !OutPose.Centre.ContainsNaN();
    }

    bool SolveDeck(FVector A0, FVector A1, float RadiusA, const FVector& B0, const FVector& B1, float RadiusB,
        const FPlacementBody& Board, const FVector& AimPoint, float Pitch, bool bStairs, FPlacementPose& OutPose, float& OutSpan)
    {
        FVector Axis1 = (A1 - A0).GetSafeNormal();
        if (Axis1.Z < 0.0f)
        {
            Axis1 = -Axis1;
            Swap(A0, A1);
        }
        const bool bSloped = Axis1.Z > 0.3f;
        const FVector P = FMath::ClosestPointOnSegment(AimPoint, A0, A1);
        const float SupportLength = FVector::Dist(A0, A1);
        const float Along = FMath::Clamp(FMath::RoundToFloat(FVector::DotProduct(P - A0, Axis1) / Pitch) * Pitch,
            Board.Radius, FMath::Max(Board.Radius, SupportLength - Board.Radius));
        const FVector P2 = A0 + Axis1 * Along;
        const FVector Q2 = FMath::ClosestPointOnSegment(P2, B0, B1);
        FVector BoardAxis = Q2 - P2;
        BoardAxis.Z = 0.0f;
        if (!BoardAxis.Normalize())
            return false;
        FVector Centre = 0.5f * (P2 + Q2);
        Centre.Z = FMath::Max(P2.Z + RadiusA, Q2.Z + RadiusB) + Board.Radius;
        FVector UpHint = FVector::UpVector;
        if (bSloped && !bStairs)
        {
            // Ramp boards tilt with the slope so their faces form one surface.
            UpHint = FVector::CrossProduct(BoardAxis, Axis1).GetSafeNormal();
            if (UpHint.Z < 0.0f)
                UpHint = -UpHint;
        }
        OutPose.Centre = Centre;
        OutPose.Axis = BoardAxis;
        OutPose.Up = UpHint;
        OutSpan = FVector::Dist2D(P2, Q2);
        return true;
    }

    bool SolveStep(const FPlacementBody& Edge, const FPlacementBody& Step, const FVector& AimPoint,
        const FVector& From, float GapCm, float DropCm, FPlacementPose& OutPose)
    {
        // In front of the edge, on the side of From, a little lower than its top.
        const FVector OnEdge = FMath::ClosestPointOnSegment(AimPoint, Edge.EndA(), Edge.EndB());
        const FVector Along = Edge.Axis.GetSafeNormal2D();
        auto SideOf = [&](const FVector& Point)
        {
            FVector Out = Point - OnEdge;
            Out.Z = 0.0f;
            return (Out - Along * FVector::DotProduct(Out, Along)).GetSafeNormal();
        };
        FVector Out = SideOf(From);
        if (Out.IsNearlyZero())
            Out = SideOf(AimPoint); // Otherwise the side of the edge that was aimed at.
        if (Out.IsNearlyZero())
            return false;
        FVector Centre = OnEdge + Out * (Edge.Radius + Step.Radius + GapCm);
        Centre.Z = Edge.Centre.Z + Edge.Radius - DropCm - Step.Radius;
        OutPose.Centre = Centre;
        OutPose.Axis = Along;
        OutPose.Up = FVector::UpVector;
        return true;
    }

    bool SolveGableRow(const FPlacementBody& Beam, const FPlacementBody& Held, int32 Row, float MinLengthCm,
        FPlacementPose& OutPose, float& OutLength)
    {
        FVector Low = Beam.EndA();
        FVector High = Beam.EndB();
        if (Low.Z > High.Z)
            Swap(Low, High);
        const FVector Run = High - Low;
        const FVector Along = Run.GetSafeNormal2D();
        const float Span = Run.Size2D();
        const float Tan = (High.Z - Low.Z) / FMath::Max(Span, 1.0f);
        const float Step = 2.0f * Held.Radius * 0.9f;
        // Rows from the beam's low end upwards; each ends where it meets the slope.
        const float Z = Low.Z - Beam.Radius + Held.Radius + Row * Step;
        const float Start = (Z + Held.Radius + Beam.Radius - Low.Z) / Tan;
        OutLength = Span - Start;
        if (OutLength < MinLengthCm)
            return false;
        OutPose.Centre = Low + Along * (Start + 0.5f * OutLength);
        OutPose.Centre.Z = Z;
        OutPose.Axis = Along;
        OutPose.Up = FVector::UpVector;
        return true;
    }

    bool SolvePostSpacing(const FVector& Base, const TArray<FVector>& Feet, const TArray<float>& Spans,
        float MaxErrorCm, FVector& OutBase, int32& OutIndex, float& OutSpan)
    {
        OutIndex = INDEX_NONE;
        float BestError = MaxErrorCm;
        for (int32 Index = 0; Index < Feet.Num(); ++Index)
        {
            const FVector& Foot = Feet[Index];
            const float Distance = FVector::Dist2D(Foot, Base);
            for (const float Span : Spans)
            {
                const float Error = FMath::Abs(Distance - Span);
                if (Error >= BestError)
                    continue;
                BestError = Error;
                OutIndex = Index;
                OutSpan = Span;
                FVector Direction = Base - Foot;
                Direction.Z = 0.0f;
                OutBase = FVector(Foot.X, Foot.Y, Base.Z) + Direction.GetSafeNormal() * Span;
            }
        }
        return OutIndex != INDEX_NONE;
    }

    bool RestsOnTop(const FPlacementBody& Post, const FPlacementBody& Upper, float ReachCm)
    {
        const FVector Top = Post.Top();
        const FVector Closest = FMath::ClosestPointOnSegment(Top, Upper.EndA(), Upper.EndB());
        return Closest.Z >= Top.Z - 5.0f && FVector::Dist(Closest, Top) < Post.Radius + Upper.Radius + ReachCm;
    }
}
