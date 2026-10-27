#include "Placement/PlacementProfiles.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool ParseKind(const FString& Text, bool& bOutAny, EPlacementFeatureKind& OutKind)
    {
        bOutAny = false;
        if (Text.IsEmpty() || Text == TEXT("Any")) { bOutAny = true; return true; }
        if (Text == TEXT("End")) { OutKind = EPlacementFeatureKind::End; return true; }
        if (Text == TEXT("Station")) { OutKind = EPlacementFeatureKind::Station; return true; }
        if (Text == TEXT("Face")) { OutKind = EPlacementFeatureKind::Face; return true; }
        if (Text == TEXT("Edge")) { OutKind = EPlacementFeatureKind::Edge; return true; }
        if (Text == TEXT("Authored")) { OutKind = EPlacementFeatureKind::Authored; return true; }
        return false;
    }

    bool ParseSolver(const FString& Text, EPlacementSolver& Out)
    {
        if (Text == TEXT("AlignOne")) { Out = EPlacementSolver::AlignOne; return true; }
        if (Text == TEXT("AlignParallel")) { Out = EPlacementSolver::AlignParallel; return true; }
        if (Text == TEXT("SpanTwo")) { Out = EPlacementSolver::SpanTwo; return true; }
        if (Text == TEXT("RestOnSurface")) { Out = EPlacementSolver::RestOnSurface; return true; }
        return false;
    }

    bool ParseOperation(const FString& Text, EPlacementOperation& Out)
    {
        if (Text.IsEmpty() || Text == TEXT("Connect")) { Out = EPlacementOperation::Connect; return true; }
        if (Text == TEXT("SurfacePlace")) { Out = EPlacementOperation::SurfacePlace; return true; }
        if (Text == TEXT("Receiver")) { Out = EPlacementOperation::Receiver; return true; }
        if (Text == TEXT("Voxel")) { Out = EPlacementOperation::Voxel; return true; }
        if (Text == TEXT("Process")) { Out = EPlacementOperation::Process; return true; }
        return false;
    }

    bool ParsePhysics(const FString& Text, EPlacementPhysics& Out)
    {
        if (Text.IsEmpty() || Text == TEXT("Static")) { Out = EPlacementPhysics::Static; return true; }
        if (Text == TEXT("Rest")) { Out = EPlacementPhysics::Rest; return true; }
        if (Text == TEXT("Hinge")) { Out = EPlacementPhysics::Hinge; return true; }
        return false;
    }

    bool ParseFilter(const TSharedPtr<FJsonObject>& Object, FPlacementFeatureFilter& Out, FString& OutError)
    {
        if (!Object.IsValid())
            return true; // Missing side accepts anything.
        FString Family;
        if (Object->TryGetStringField(TEXT("family"), Family) && !Family.IsEmpty())
            Out.Family = FName(*Family);
        FString Kind;
        Object->TryGetStringField(TEXT("kind"), Kind);
        if (!ParseKind(Kind, Out.bAnyKind, Out.Kind))
        {
            OutError = FString::Printf(TEXT("unknown feature kind '%s'"), *Kind);
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
        if (Object->TryGetArrayField(TEXT("tags"), Tags))
            for (const TSharedPtr<FJsonValue>& Tag : *Tags)
                Out.Tags.Add(FName(*Tag->AsString()));
        return true;
    }

    bool ParseProfile(const TSharedPtr<FJsonObject>& Object, FPlacementProfile& Out, FString& OutError)
    {
        FString Text;
        if (!Object->TryGetStringField(TEXT("id"), Text) || Text.IsEmpty())
        {
            OutError = TEXT("profile without an id");
            return false;
        }
        Out.Id = FName(*Text);
        Object->TryGetBoolField(TEXT("enabled"), Out.bEnabled);

        Text.Reset();
        Object->TryGetStringField(TEXT("operation"), Text);
        if (!ParseOperation(Text, Out.Operation))
        {
            OutError = FString::Printf(TEXT("unknown operation '%s'"), *Text);
            return false;
        }
        Text.Reset();
        if (!Object->TryGetStringField(TEXT("solver"), Text) || !ParseSolver(Text, Out.Solver))
        {
            OutError = FString::Printf(TEXT("unknown solver '%s'"), *Text);
            return false;
        }
        FString Mode;
        if (Object->TryGetStringField(TEXT("mode"), Mode) && !Mode.IsEmpty())
            Out.Mode = FName(*Mode);
        else
            Out.Mode = Out.Solver == EPlacementSolver::AlignOne ? FName(TEXT("snap")) : NAME_None;
        const TSharedPtr<FJsonObject>* Params = nullptr;
        if (Object->TryGetObjectField(TEXT("params"), Params))
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Params)->Values)
            {
                double Value = 0.0;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value))
                {
                    OutError = FString::Printf(TEXT("param '%s' is not a number"), *Pair.Key);
                    return false;
                }
                Out.Params.Add(FName(*Pair.Key), static_cast<float>(Value));
            }
        }
        const TSharedPtr<FJsonObject>* Side = nullptr;
        if (Object->TryGetObjectField(TEXT("source"), Side) && !ParseFilter(*Side, Out.Source, OutError))
            return false;
        Side = nullptr;
        if (Object->TryGetObjectField(TEXT("target"), Side) && !ParseFilter(*Side, Out.Target, OutError))
            return false;

        const TSharedPtr<FJsonObject>* Angle = nullptr;
        if (Object->TryGetObjectField(TEXT("angle"), Angle))
        {
            FString Policy;
            (*Angle)->TryGetStringField(TEXT("policy"), Policy);
            if (Policy.IsEmpty() || Policy == TEXT("Fixed"))
                Out.AnglePolicy = EPlacementAnglePolicy::Fixed;
            else if (Policy == TEXT("Grid"))
                Out.AnglePolicy = EPlacementAnglePolicy::Grid;
            else if (Policy == TEXT("Range"))
                Out.AnglePolicy = EPlacementAnglePolicy::Range;
            else
            {
                OutError = FString::Printf(TEXT("unknown angle policy '%s'"), *Policy);
                return false;
            }
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if ((*Angle)->TryGetArrayField(TEXT("values"), Values))
                for (const TSharedPtr<FJsonValue>& Value : *Values)
                    Out.AnglesDeg.Add(static_cast<float>(Value->AsNumber()));
            double Number = 0.0;
            if ((*Angle)->TryGetNumberField(TEXT("min"), Number)) Out.AngleMinDeg = Number;
            if ((*Angle)->TryGetNumberField(TEXT("max"), Number)) Out.AngleMaxDeg = Number;
            if ((*Angle)->TryGetNumberField(TEXT("step"), Number)) Out.AngleStepDeg = Number;
            if ((*Angle)->TryGetNumberField(TEXT("hysteresis"), Number)) Out.AngleHysteresisDeg = Number;
        }

        double Number = 0.0;
        if (Object->TryGetNumberField(TEXT("searchRadiusCm"), Number)) Out.SearchRadiusCm = Number;
        if (Object->TryGetNumberField(TEXT("priority"), Number)) Out.Priority = Number;
        int32 Integer = 0;
        if (Object->TryGetNumberField(TEXT("requiredLinks"), Integer))
            Out.RequiredLinks = Integer;
        else
            Out.RequiredLinks = FMath::Max(0, FPlacementProfileRegistry::GetModeLinkCount(Out.Solver, Out.Mode));
        if (Object->TryGetNumberField(TEXT("capacity"), Integer)) Out.Capacity = Integer;
        Object->TryGetBoolField(TEXT("structural"), Out.bStructural);
        Text.Reset();
        Object->TryGetStringField(TEXT("physics"), Text);
        if (!ParsePhysics(Text, Out.Physics))
        {
            OutError = FString::Printf(TEXT("unknown physics '%s'"), *Text);
            return false;
        }
        return true;
    }
}

bool FPlacementFeatureFilter::Matches(FName PieceFamily, const FPlacementFeature& Feature) const
{
    if (!Family.IsNone() && Family != PieceFamily)
        return false;
    if (!bAnyKind && Kind != Feature.Kind)
        return false;
    return Tags.Num() == 0 || Tags.Contains(Feature.Tag);
}

int32 FPlacementProfileRegistry::GetModeLinkCount(EPlacementSolver Solver, FName Mode)
{
    const FString Name = Mode.ToString();
    switch (Solver)
    {
    case EPlacementSolver::AlignOne:
        return Name == TEXT("snap") || Name == TEXT("anchor") ? 1 : -1;
    case EPlacementSolver::AlignParallel:
        if (Name == TEXT("stack") || Name == TEXT("step") || Name == TEXT("gable"))
            return 1;
        return Name == TEXT("deck") ? 2 : -1;
    case EPlacementSolver::SpanTwo:
        return Name == TEXT("strut") ? 2 : -1;
    case EPlacementSolver::RestOnSurface:
        return Name == TEXT("post") || Name == TEXT("free") ? 0 : -1;
    }
    return -1;
}

FString FPlacementProfileRegistry::Validate(const FPlacementProfile& Profile)
{
    if (Profile.Id.IsNone())
        return TEXT("profile without an id");
    const int32 ModeLinks = GetModeLinkCount(Profile.Solver, Profile.Mode);
    if (ModeLinks < 0)
        return FString::Printf(TEXT("mode '%s' does not belong to this solver"), *Profile.Mode.ToString());
    if (Profile.RequiredLinks != ModeLinks)
        return FString::Printf(TEXT("mode '%s' makes %d links, not %d"), *Profile.Mode.ToString(), ModeLinks, Profile.RequiredLinks);
    if (Profile.Solver == EPlacementSolver::RestOnSurface && Profile.bStructural && Profile.Mode != TEXT("free"))
        return TEXT("RestOnSurface cannot be structural");
    if (Profile.Operation == EPlacementOperation::Connect && Profile.Solver == EPlacementSolver::RestOnSurface)
        return TEXT("RestOnSurface belongs to SurfacePlace, not Connect");
    if (Profile.Operation == EPlacementOperation::SurfacePlace && Profile.Solver != EPlacementSolver::RestOnSurface)
        return TEXT("SurfacePlace uses RestOnSurface");
    switch (Profile.AnglePolicy)
    {
    case EPlacementAnglePolicy::Fixed:
        if (Profile.AnglesDeg.Num() > 1)
            return TEXT("Fixed angle takes one value");
        break;
    case EPlacementAnglePolicy::Grid:
        if (Profile.AnglesDeg.Num() == 0)
            return TEXT("Grid angle needs values");
        break;
    case EPlacementAnglePolicy::Range:
        if (Profile.AngleStepDeg <= 0.0f || Profile.AngleMinDeg > Profile.AngleMaxDeg)
            return TEXT("Range angle needs min <= max and step > 0");
        break;
    }
    if (Profile.SearchRadiusCm <= 0.0f)
        return TEXT("searchRadiusCm must be positive");
    if (Profile.Capacity < 1)
        return TEXT("capacity must be at least 1");
    return FString();
}

bool FPlacementProfileRegistry::LoadFromString(const FString& Json)
{
    Profiles.Reset();
    Errors.Reset();
    SchemaVersion = 0;
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Errors.Add(TEXT("not valid JSON"));
        return false;
    }
    if (!Root->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion) || SchemaVersion != 1)
    {
        Errors.Add(FString::Printf(TEXT("unsupported schemaVersion %d"), SchemaVersion));
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
    if (!Root->TryGetArrayField(TEXT("profiles"), List))
    {
        Errors.Add(TEXT("no profiles array"));
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *List)
    {
        FPlacementProfile Profile;
        FString Error;
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid() || !ParseProfile(Object, Profile, Error))
        {
            Errors.Add(Error.IsEmpty() ? TEXT("profile is not an object") : Error);
            continue;
        }
        Error = Validate(Profile);
        if (Error.IsEmpty() && Find(Profile.Id))
            Error = TEXT("duplicate id");
        if (!Error.IsEmpty())
        {
            Errors.Add(FString::Printf(TEXT("%s: %s"), *Profile.Id.ToString(), *Error));
            continue;
        }
        Profiles.Add(MoveTemp(Profile));
    }
    return true;
}

void FPlacementProfileRegistry::LoadDefault()
{
    FString Text;
    if (FFileHelper::LoadFileToString(Text, *GetProjectCataloguePath()) && LoadFromString(Text) && Profiles.Num() > 0)
    {
        for (const FString& Error : Errors)
            UE_LOG(LogTemp, Warning, TEXT("Placement profile rejected: %s"), *Error);
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("Placement catalogue %s missing or broken; using the built-in profiles."),
        *GetProjectCataloguePath());
    LoadFromString(GetBuiltInCatalogue());
}

const FPlacementProfile* FPlacementProfileRegistry::Find(FName Id) const
{
    return Profiles.FindByPredicate([Id](const FPlacementProfile& Profile) { return Profile.Id == Id; });
}

FString FPlacementProfileRegistry::GetProjectCataloguePath()
{
    return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("Placement"), TEXT("PlacementProfiles.json"));
}

const TCHAR* FPlacementProfileRegistry::GetBuiltInCatalogue()
{
    // BEGIN GENERATED CATALOGUE (UnrealEditorScripts/SyncPlacementCatalogue.py)
    static const TCHAR* Catalogue =
        TEXT(R"JSON({
  "schemaVersion": 1,
  "notes": [
    "Runtime catalogue of the global placement kernel (Source/MyProject3/Placement).",
    "Plan: Docs/Architecture/GLOBAL_SNAP_SYSTEM_PLAN.md. Units: cm and degrees.",
    "A copy is compiled in (FPlacementProfileRegistry::GetBuiltInCatalogue) for packaged builds; the test Placement.BuiltInCatalogueMatchesProjectFile keeps the two equal.",
    "Feature kinds: End, Station, Face, Edge, Authored, Any. Solvers and modes: AlignOne snap|anchor, AlignParallel stack|deck|step|gable, SpanTwo strut, RestOnSurface post|free.",
    "enabled=false marks a profile that is designed but not yet used by the game."
  ],
  "profiles": [
    { "id": "log.end_to_end", "solver": "AlignOne", "mode": "snap",
      "source": { "family": "log", "kind": "End", "tags": ["LogEnd"] },
      "target": { "family": "log", "kind": "End" },
      "angle": { "policy": "Fixed", "values": [0] }, "searchRadiusCm": 250, "priority": 2 },
    { "id": "log.end_to_station", "solver": "AlignOne", "mode": "snap",
      "source": { "family": "log", "kind": "End", "tags": ["LogEnd"] },
      "target": { "family": "log", "kind": "Station" },
      "angle": { "policy": "Fixed", "values": [0] }, "searchRadiusCm": 250, "priority": 1,
      "params": { "radialStepDeg": 90 } },
    { "id": "log.authored", "solver": "AlignOne", "mode": "snap",
      "source": { "family": "log", "kind": "Any" },
      "target": { "family": "log", "kind": "Any" },
      "angle": { "policy": "Fixed", "values": [0] }, "searchRadiusCm": 250, "priority": 0 },
    { "id": "log.parallel_stack", "solver": "AlignParallel", "mode": "stack",
      "source": { "family": "log" }, "target": { "family": "log" },
      "angle": { "policy": "Grid", "values": [0, 90] },
      "params": { "moduleCm": 75, "interlock": 0.82, "postSeat": 0.6, "maxRows": 6, "slotRadiusShare": 0.5 } },
    { "id": "log.deck", "solver": "AlignParallel", "mode": "deck",
      "source": { "family": "log" },
      "angle": { "policy": "Fixed", "values": [90] },
      "params": { "lineReachCm": 80, "minGapCm": 30, "spanMarginCm": 20, "slotRadiusShare": 0.7, "stairs": 0 } },
    { "id": "log.stairs", "solver": "AlignParallel", "mode": "deck",
      "source": { "family": "log" },
      "angle": { "policy": "Fixed", "values": [90] },
      "params": { "lineReachCm": 80, "minGapCm": 30, "spanMarginCm": 20, "slotRadiusShare": 0.7, "stairs": 1, "pitchCm": 40 } },
    { "id": "log.step", "solver": "AlignParallel", "mode": "step",
      "source": { "family": "log" }, "target": { "family": "log" },
      "angle": { "policy": "Fixed", "values": [0] },
      "params": { "lengthCm": 150, "gapCm": 25, "dropCm": 25 } },
    { "id": "log.gable", "solver": "AlignParallel", "mode": "gable",
      "source": { "family": "log" }, "target": { "family": "log" },
      "angle": { "policy": "Fixed", "values": [0] },
      "params": { "minLengthCm": 40, "maxRows": 24, "minSlope": 0.15, "maxSlope": 0.9 } },
    { "id": "log.strut", "solver": "SpanTwo", "mode": "strut",
      "source": { "family": "log" }, "target": { "family": "log" },
      "angle": { "policy": "Fixed", "values": [0] }, "searchRadiusCm": 250,
      "params": { "legCm": 75, "moduleCm": 75, "minLengthCm": 40 } },
    { "id": "log.post_on_ground", "operation": "SurfacePlace", "solver": "RestOnSurface", "mode": "post",
      "source": { "family": "log" }, "structural": false,
      "params": { "spanStepCm": 150, "spanCount": 4, "maxErrorCm": 30 } },
    { "id": "piece.free_place", "operation": "SurfacePlace", "solver": "RestOnSurface", "mode": "free",
      "structural": true },
    { "id": "stone.on_anchor", "solver": "AlignOne", "mode": "anchor",
      "source": { "family": "stone", "kind": "Authored" }, "target": { "kind": "Any" },
      "angle": { "policy": "Fixed", "values": [0] }, "searchRadiusCm": 120,
      "params": { "keepTargetRotation": 1 } },
    { "id": "stick.on_anchor", "solver": "AlignOne", "mode": "anchor",
      "source": { "family": "stick", "kind": "Authored" }, "target": { "kind": "Any" },
      "angle": { "policy": "Grid", "values": [0, 45, 90] }, "searchRadiusCm": 120 },
    { "id": "log.station_cross", "enabled": false, "solver": "AlignOne", "mode": "snap",
      "source": { "family": "log", "kind": "Station" }, "target": { "family": "log", "kind": "Station" },
      "angle": { "policy": "Grid", "values": [0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 165] }, "searchRadiusCm": 200 },
    { "id": "log.ground_rest", "enabled": false, "operation": "SurfacePlace", "solver": "RestOnSurface", "mode": "free",
      "structural": false, "physics": "Rest" }
  ]
})JSON")
        ;
    return Catalogue;
    // END GENERATED CATALOGUE
}
