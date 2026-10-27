#pragma once

#include "CoreMinimal.h"
#include "Placement/PlacementTypes.h"

/** One side of a profile: which feature of which material it accepts. */
struct FPlacementFeatureFilter
{
    /** NAME_None accepts any family (unless Families lists some). */
    FName Family = NAME_None;
    /** More than one family ("families" in the JSON); empty means Family alone decides. */
    TArray<FName> Families;
    /** Cross-sections the piece may have (Round, Hewn ...); empty accepts any. */
    TArray<FName> Sections;
    bool bAnyKind = true;
    EPlacementFeatureKind Kind = EPlacementFeatureKind::Authored;
    /** Empty accepts any tag. */
    TArray<FName> Tags;

    bool Matches(FName PieceFamily, const FPlacementFeature& Feature) const;
    bool AllowsFamily(FName PieceFamily) const;
    bool AllowsSection(FName PieceSection) const;
};

/** A placement rule from data: what joins what, how it is solved and what it leaves behind. */
struct FPlacementProfile
{
    FName Id = NAME_None;
    bool bEnabled = true;
    EPlacementOperation Operation = EPlacementOperation::Connect;
    EPlacementSolver Solver = EPlacementSolver::AlignOne;
    /**
     * What the solver lays out: AlignOne "snap" | "anchor"; AlignParallel "stack" |
     * "deck" | "step" | "gable"; SpanTwo "strut"; RestOnSurface "post" | "free".
     */
    FName Mode = NAME_None;
    /** Numbers the solver reads (grid, interlock, leg length ...); see PlacementProfiles.json. */
    TMap<FName, float> Params;
    FPlacementFeatureFilter Source;
    FPlacementFeatureFilter Target;

    EPlacementAnglePolicy AnglePolicy = EPlacementAnglePolicy::Fixed;
    /** Fixed: the first value. Grid: the allowed angles. */
    TArray<float> AnglesDeg;
    float AngleMinDeg = 0.0f;
    float AngleMaxDeg = 0.0f;
    float AngleStepDeg = 15.0f;
    /** The chosen angle only changes once the held angle is this much past the halfway point. */
    float AngleHysteresisDeg = 4.0f;

    /** How far (cm) from the aim or the source feature a target feature may be. */
    float SearchRadiusCm = 250.0f;
    /** Links the solution must have; fewer is invalid, never a smaller placement. */
    int32 RequiredLinks = 1;
    /** Links a target feature can take. */
    int32 Capacity = 1;
    bool bStructural = true;
    EPlacementPhysics Physics = EPlacementPhysics::Static;
    /** Higher wins between equally good candidates. */
    float Priority = 0.0f;

    /** A param, or Default when the profile does not set it. */
    float GetParam(FName Name, float Default) const
    {
        const float* Value = Params.Find(Name);
        return Value ? *Value : Default;
    }
};

/**
 * Loads and checks the profile catalogue (Config/Placement/PlacementProfiles.json).
 * A profile that breaks a rule is rejected with a reason, not half used.
 */
class MYPROJECT3_API FPlacementProfileRegistry
{
public:
    /** Parses a catalogue; returns false when the text is not a catalogue at all. */
    bool LoadFromString(const FString& Json);
    /** Loads the project file, or the built-in catalogue when the file is missing or broken. */
    void LoadDefault();

    const TArray<FPlacementProfile>& GetProfiles() const { return Profiles; }
    const FPlacementProfile* Find(FName Id) const;
    const TArray<FString>& GetErrors() const { return Errors; }
    int32 GetSchemaVersion() const { return SchemaVersion; }

    static FString GetProjectCataloguePath();
    /** The catalogue compiled into the game, used when the project file cannot be read. */
    static const TCHAR* GetBuiltInCatalogue();

    /** Checks one profile against the catalogue rules; empty when valid. */
    static FString Validate(const FPlacementProfile& Profile);
    /** Links a mode always makes (stack 1, deck 2, strut 2 ...); -1 for an unknown mode. */
    static int32 GetModeLinkCount(EPlacementSolver Solver, FName Mode);

private:
    TArray<FPlacementProfile> Profiles;
    TArray<FString> Errors;
    int32 SchemaVersion = 0;
};
