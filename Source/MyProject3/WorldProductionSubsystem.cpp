#include "WorldProductionSubsystem.h"

int32 UWorldProductionSubsystem::FindRecipe(FName RecipeId) const
{
    return Recipes.IndexOfByPredicate([RecipeId](const FWorldProductionRecipe& R)
        { return R.RecipeId == RecipeId; });
}

int64 UWorldProductionSubsystem::CountStockUnits(const TMap<FName, int32>& Stock) const
{
    int64 Total = 0;
    for (const TPair<FName, int32>& Pair : Stock)
    {
        if (Pair.Key.IsNone() || Pair.Value < 0) return MAX_int64;
        Total += Pair.Value;
        if (Total > MAX_int32) return MAX_int64;
    }
    return Total;
}

bool UWorldProductionSubsystem::RegisterRecipe(const FWorldProductionRecipe& Recipe)
{
    if (Recipe.RecipeId.IsNone() || Recipe.Inputs.IsEmpty() || Recipe.Outputs.IsEmpty() ||
        !FMath::IsFinite(Recipe.DurationSeconds) || Recipe.DurationSeconds <= 0.0 ||
        FindRecipe(Recipe.RecipeId) != INDEX_NONE || MaxRecipes <= 0 || Recipes.Num() >= MaxRecipes)
        return false;

    auto ValidateIngredients = [](const TArray<FProductionIngredient>& List)
    {
        for (const FProductionIngredient& I : List)
            if (I.ItemId.IsNone() || I.Quantity <= 0) return false;
        return true;
    };
    if (!ValidateIngredients(Recipe.Inputs) || !ValidateIngredients(Recipe.Outputs)) return false;
    Recipes.Add(Recipe);
    return true;
}

bool UWorldProductionSubsystem::RegisterStation(const FWorldProductionStation& Station)
{
    if (!Station.StationId.IsValid() || Station.StationTag.IsNone() ||
        Station.StorageCapacityUnits <= 0 || Station.Revision <= 0 ||
        Stations.Contains(Station.StationId) || MaxStations <= 0 || Stations.Num() >= MaxStations ||
        !Station.Jobs.IsEmpty())
        return false;
    for (const TPair<FName, int32>& Pair : Station.Stock)
        if (Pair.Key.IsNone() || Pair.Value < 0) return false;
    if (CountStockUnits(Station.Stock) > Station.StorageCapacityUnits) return false;
    Stations.Add(Station.StationId, Station);
    return true;
}

bool UWorldProductionSubsystem::AddStock(
    const FGuid& StationId, FName ItemId, int32 Quantity, int64 ExpectedRevision)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision != ExpectedRevision || Station->Revision == MAX_int64 ||
        ItemId.IsNone() || Quantity <= 0) return false;

    const int32 Existing = Station->Stock.FindRef(ItemId);
    if (Existing > MAX_int32 - Quantity) return false;
    TMap<FName, int32> Proposed = Station->Stock;
    Proposed.Add(ItemId, Existing + Quantity);
    int64 Reserved = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running) continue;
        const int32 RecipeIndex = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(RecipeIndex)) return false;
        for (const FProductionIngredient& Output : Recipes[RecipeIndex].Outputs) Reserved += Output.Quantity;
    }
    if (CountStockUnits(Proposed) + Reserved > Station->StorageCapacityUnits) return false;
    Station->Stock = MoveTemp(Proposed);
    ++Station->Revision;
    return true;
}

bool UWorldProductionSubsystem::EnqueueJob(
    const FGuid& StationId, FName RecipeId, FGuid& OutJobId)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    const int32 RecipeIndex = FindRecipe(RecipeId);
    if (!Station || !Recipes.IsValidIndex(RecipeIndex) || Station->Revision == MAX_int64 ||
        MaxJobsPerStation <= 0 || Station->Jobs.Num() >= MaxJobsPerStation)
        return false;

    const FWorldProductionRecipe& Recipe = Recipes[RecipeIndex];
    if (!Recipe.RequiredStationTag.IsNone() && Recipe.RequiredStationTag != Station->StationTag)
        return false;

    TMap<FName, int64> Required;
    for (const FProductionIngredient& Input : Recipe.Inputs)
        Required.FindOrAdd(Input.ItemId) += Input.Quantity;
    for (const TPair<FName, int64>& Need : Required)
        if (Need.Value > MAX_int32 || Station->Stock.FindRef(Need.Key) < Need.Value) return false;

    int64 Reserved = 0;
    for (const FWorldProductionJob& Job : Station->Jobs)
    {
        if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running) continue;
        const int32 ExistingRecipe = FindRecipe(Job.RecipeId);
        if (!Recipes.IsValidIndex(ExistingRecipe)) return false;
        for (const FProductionIngredient& Output : Recipes[ExistingRecipe].Outputs) Reserved += Output.Quantity;
    }
    int64 NewOutputUnits = 0;
    for (const FProductionIngredient& Output : Recipe.Outputs) NewOutputUnits += Output.Quantity;
    if (CountStockUnits(Station->Stock) + Reserved + NewOutputUnits > Station->StorageCapacityUnits)
        return false;

    // All input, station and output-capacity checks precede mutation.
    for (const TPair<FName, int64>& Need : Required)
        Station->Stock.FindOrAdd(Need.Key) -= static_cast<int32>(Need.Value);

    FWorldProductionJob Job;
    Job.JobId = FGuid::NewGuid();
    Job.RecipeId = RecipeId;
    Job.State = EProductionJobState::Queued;
    Station->Jobs.Add(Job);
    ++Station->Revision;
    OutJobId = Job.JobId;
    return OutJobId.IsValid();
}

bool UWorldProductionSubsystem::CanAdvanceProductionStep(double DeltaSeconds) const
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0 || DeltaSeconds > 3600.0)
        return false;
    for (const TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        const FWorldProductionStation& Station = Pair.Value;
        bool bHasWork = false;
        for (const FWorldProductionJob& Job : Station.Jobs)
        {
            if (Job.State != EProductionJobState::Queued && Job.State != EProductionJobState::Running)
                continue;
            bHasWork = true;
            const int32 RecipeIndex = FindRecipe(Job.RecipeId);
            if (!Recipes.IsValidIndex(RecipeIndex) ||
                !FMath::IsFinite(Recipes[RecipeIndex].DurationSeconds) ||
                Recipes[RecipeIndex].DurationSeconds <= 0.0 ||
                !FMath::IsFinite(Job.ProgressSeconds) ||
                !FMath::IsFinite(Job.ProgressSeconds + DeltaSeconds))
                return false;
        }
        if (bHasWork && Station.Revision == MAX_int64) return false;
    }
    return true;
}

bool UWorldProductionSubsystem::AdvanceProductionStep(double DeltaSeconds)
{
    if (!CanAdvanceProductionStep(DeltaSeconds)) return false;

    for (TPair<FGuid, FWorldProductionStation>& Pair : Stations)
    {
        FWorldProductionStation& Station = Pair.Value;
        for (FWorldProductionJob& Job : Station.Jobs)
        {
            if (Job.State == EProductionJobState::Completed || Job.State == EProductionJobState::Blocked)
                continue;
            const int32 RecipeIndex = FindRecipe(Job.RecipeId);
            const FWorldProductionRecipe& Recipe = Recipes[RecipeIndex];
            if (Job.State == EProductionJobState::Queued)
                Job.State = EProductionJobState::Running;
            Job.ProgressSeconds = FMath::Min(Recipe.DurationSeconds, Job.ProgressSeconds + DeltaSeconds);
            if (Job.ProgressSeconds < Recipe.DurationSeconds) break;

            TMap<FName, int32> ProposedStock = Station.Stock;
            bool bCanCommitOutputs = true;
            for (const FProductionIngredient& Output : Recipe.Outputs)
            {
                const int32 Current = ProposedStock.FindRef(Output.ItemId);
                if (Current > MAX_int32 - Output.Quantity) { bCanCommitOutputs = false; break; }
                ProposedStock.Add(Output.ItemId, Current + Output.Quantity);
            }
            if (!bCanCommitOutputs || CountStockUnits(ProposedStock) > Station.StorageCapacityUnits)
            {
                Job.State = EProductionJobState::Blocked;
                Job.BlockedReason = FName(TEXT("OutputCapacity"));
                break;
            }
            Station.Stock = MoveTemp(ProposedStock);
            Job.State = EProductionJobState::Completed;
            Job.BlockedReason = NAME_None;
            ++Station.Revision;
            break; // one physical production line processes one job at a time
        }
    }
    return true;
}

bool UWorldProductionSubsystem::CollectCompletedJob(const FGuid& StationId, const FGuid& JobId)
{
    FWorldProductionStation* Station = Stations.Find(StationId);
    if (!Station || Station->Revision == MAX_int64) return false;
    const int32 Index = Station->Jobs.IndexOfByPredicate([JobId](const FWorldProductionJob& J)
        { return J.JobId == JobId; });
    if (!Station->Jobs.IsValidIndex(Index) ||
        Station->Jobs[Index].State != EProductionJobState::Completed) return false;
    Station->Jobs.RemoveAt(Index);
    ++Station->Revision;
    return true;
}

bool UWorldProductionSubsystem::GetStation(
    const FGuid& StationId, FWorldProductionStation& OutStation) const
{
    const FWorldProductionStation* Found = Stations.Find(StationId);
    if (!Found) return false;
    OutStation = *Found;
    return true;
}
