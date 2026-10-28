#include "RaidDirectorSubsystem.h"

FWorldOperationResult URaidDirectorSubsystem::Failure(EWorldOperationResultCode Code,const FGuid& OperationId,const TCHAR* Message) const {
 return FWorldOperationResult::Failure(Code,OperationId,FText::FromString(Message));
}
FWorldOperationResult URaidDirectorSubsystem::ValidateNewOperation(const FGuid& OperationId) const {
 if(!OperationId.IsValid()) return Failure(EWorldOperationResultCode::InvalidState,OperationId,TEXT("Operation ID must be valid."));
 if(ProcessedOperationIds.Contains(OperationId)) return Failure(EWorldOperationResultCode::Conflict,OperationId,TEXT("Operation ID has already been processed."));
 return FWorldOperationResult::Success(OperationId,FText::GetEmpty(),{});
}
void URaidDirectorSubsystem::RememberOperation(const FGuid& OperationId){if(OperationId.IsValid()) ProcessedOperationIds.Add(OperationId);}

FWorldOperationResult URaidDirectorSubsystem::ConfigureDirector(const FRaidDirectorConfig& C,FGuid Op){
 auto V=ValidateNewOperation(Op); if(!V.IsSuccess()) return V;
 if(C.MaxQueuedThreats<=0||C.MaxAliveThreats<=0||!FMath::IsFinite(C.BaseDifficulty)||!FMath::IsFinite(C.MaxDifficulty)||C.BaseDifficulty<0||C.MaxDifficulty<C.BaseDifficulty||C.DefaultWaveCount<=0||C.BaseEnemiesPerWave<=0||!FMath::IsFinite(C.NoiseWeight)||!FMath::IsFinite(C.StockWeight)||!FMath::IsFinite(C.DefenseWeight)||C.NoiseWeight<0||C.StockWeight<0||C.DefenseWeight<0)
  return Failure(EWorldOperationResultCode::MissingConfiguration,Op,TEXT("Raid configuration contains invalid limits or weights."));
 if(QueuedThreatCount>C.MaxQueuedThreats||AliveThreatCount>C.MaxAliveThreats) return Failure(EWorldOperationResultCode::Conflict,Op,TEXT("New limits are below current population."));
 Config=C; RememberOperation(Op); return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Raid director configured.")),{});
}
FWorldOperationResult URaidDirectorSubsystem::EvaluateThreat(const FRaidThreatContext& C,float& Out,FGuid Op){
 auto V=ValidateNewOperation(Op); if(!V.IsSuccess()) return V;
 if(C.WorldTick<0||C.WorldTick<LastEvaluatedWorldTick||!FMath::IsFinite(C.PlayerProgress)||!FMath::IsFinite(C.SettlementNoise)||!FMath::IsFinite(C.ValuableStock)||!FMath::IsFinite(C.DefenseRating)||C.PlayerProgress<0||C.SettlementNoise<0||C.ValuableStock<0||C.DefenseRating<0)
  return Failure(EWorldOperationResultCode::InvalidState,Op,TEXT("Threat context is invalid or world time moved backwards."));
 Out=FMath::Clamp(Config.BaseDifficulty+C.PlayerProgress+C.SettlementNoise*Config.NoiseWeight+C.ValuableStock*Config.StockWeight-C.DefenseRating*Config.DefenseWeight,Config.BaseDifficulty,Config.MaxDifficulty);
 LastEvaluatedWorldTick=C.WorldTick; RememberOperation(Op); return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Threat difficulty evaluated.")),{});
}
FWorldOperationResult URaidDirectorSubsystem::ScheduleRaid(const FRaidThreatContext& C,int64 Earliest,int32 RequestedWaves,FGuid& OutId,FGuid Op){
 auto V=ValidateNewOperation(Op); if(!V.IsSuccess()) return V;
 if(QueuedThreatCount>=Config.MaxQueuedThreats) return Failure(EWorldOperationResultCode::CapacityExceeded,Op,TEXT("Threat queue is full."));
 if(Earliest<C.WorldTick||C.WorldTick<0||RequestedWaves<=0||!FMath::IsFinite(C.PlayerProgress)||!FMath::IsFinite(C.SettlementNoise)||!FMath::IsFinite(C.ValuableStock)||!FMath::IsFinite(C.DefenseRating)||C.PlayerProgress<0||C.SettlementNoise<0||C.ValuableStock<0||C.DefenseRating<0)
  return Failure(EWorldOperationResultCode::InvalidState,Op,TEXT("Raid schedule inputs are invalid."));
 FRaidRuntimeState S; S.RaidId=FGuid::NewGuid(); S.Phase=ERaidPhase::Scheduled;
 S.Difficulty=FMath::Clamp(Config.BaseDifficulty+C.PlayerProgress+C.SettlementNoise*Config.NoiseWeight+C.ValuableStock*Config.StockWeight-C.DefenseRating*Config.DefenseWeight,Config.BaseDifficulty,Config.MaxDifficulty);
 S.Revision=1; Raids.Add(S.RaidId,S); TArray<FRaidWavePlan> Plans; Plans.SetNum(FMath::Clamp(RequestedWaves,1,32));
 for(int32 I=0;I<Plans.Num();++I){Plans[I].WaveIndex=I;Plans[I].Difficulty=S.Difficulty*(1.0f+0.15f*I);Plans[I].PlannedEnemies=FMath::Clamp(FMath::RoundToInt(Config.BaseEnemiesPerWave*(1.0f+0.2f*I)),1,Config.MaxAliveThreats);}
 WavePlans.Add(S.RaidId,Plans); ++QueuedThreatCount; OutId=S.RaidId; RememberOperation(Op);
 return FWorldOperationResult::Success(Op,FText::FromString(FString::Printf(TEXT("Raid scheduled for world tick %lld; external clock adapter must trigger it."),Earliest)),{S.RaidId});
}
FWorldOperationResult URaidDirectorSubsystem::BuildWavePlans(FGuid Id,int32 PerWave,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V; FRaidRuntimeState* R=Raids.Find(Id);
 if(!R||R->Phase!=ERaidPhase::Scheduled||PerWave<=0)return Failure(EWorldOperationResultCode::PrerequisiteNotMet,Op,TEXT("Raid must be scheduled and wave size positive."));
 const TArray<FRaidWavePlan>* ExistingPlans=WavePlans.Find(Id); const int32 WaveCount=ExistingPlans?FMath::Clamp(ExistingPlans->Num(),1,32):FMath::Clamp(Config.DefaultWaveCount,1,32); TArray<FRaidWavePlan> Plans;for(int32 I=0;I<WaveCount;++I){FRaidWavePlan P;P.WaveIndex=I;P.Difficulty=R->Difficulty*(1.0f+0.15f*I);P.PlannedEnemies=FMath::Clamp(FMath::RoundToInt(PerWave*(1.0f+0.2f*I)),1,Config.MaxAliveThreats);Plans.Add(P);}
 WavePlans.Add(Id,Plans);R->Phase=ERaidPhase::Preparing;++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Wave plans built.")),{Id});
}
FWorldOperationResult URaidDirectorSubsystem::ChooseSpawnCandidate(FGuid Id,const TArray<FRaidSpawnCandidate>& C,FRaidSpawnCandidate& Out,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);
 if(!R||(R->Phase!=ERaidPhase::Preparing&&R->Phase!=ERaidPhase::Active))return Failure(EWorldOperationResultCode::PrerequisiteNotMet,Op,TEXT("Raid is not ready to select a spawn."));
 TArray<FRaidSpawnCandidate> E;for(const auto& X:C)if(X.CandidateId.IsValid()&&X.bIsValid&&!X.bVisibleToPlayer&&X.bHasNavigation)E.Add(X);
 if(E.IsEmpty())return Failure(EWorldOperationResultCode::UnsupportedSurface,Op,TEXT("No spawn candidate passed validity, visibility and navigation gates."));
 E.Sort([](const FRaidSpawnCandidate& A,const FRaidSpawnCandidate& B){return A.CandidateId.ToString()<B.CandidateId.ToString();});
 const int32 I=DeterministicRandom.RandRange(0,E.Num()-1);Out=E[I];R->LastSpawnLocation=Out.Location;++R->Revision;RememberOperation(Op);
 return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Spawn candidate selected; no actor spawned.")),{Id,Out.CandidateId});
}
FWorldOperationResult URaidDirectorSubsystem::SelectTarget(FGuid Id,const TArray<FRaidTargetCandidate>& C,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);
 if(!R||R->Phase==ERaidPhase::Completed||R->Phase==ERaidPhase::Cancelled||R->Phase==ERaidPhase::Failed)return Failure(EWorldOperationResultCode::PrerequisiteNotMet,Op,TEXT("Raid cannot select target in current phase."));
 const FRaidTargetCandidate* Best=nullptr;for(const auto& X:C){if(!X.TargetId.IsValid()||!X.bValid||X.bProtectedFromRaid||!FMath::IsFinite(X.StrategicValue)||X.StrategicValue<0)continue;
 if(!Best||X.StrategicValue>Best->StrategicValue||(FMath::IsNearlyEqual(X.StrategicValue,Best->StrategicValue)&&X.TargetId.ToString()<Best->TargetId.ToString()))Best=&X;}
 if(!Best)return Failure(EWorldOperationResultCode::InvalidTarget,Op,TEXT("No eligible raid target was supplied."));
 R->SelectedTargetId=Best->TargetId;++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Target selected; navigation remains adapter-owned.")),{Id,Best->TargetId});
}
FWorldOperationResult URaidDirectorSubsystem::BeginNextWave(FGuid Id,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);TArray<FRaidWavePlan>* P=WavePlans.Find(Id);
 if(!R||!P||(R->Phase!=ERaidPhase::Preparing&&R->Phase!=ERaidPhase::Recovering))return Failure(EWorldOperationResultCode::PrerequisiteNotMet,Op,TEXT("Raid is not ready for next wave."));
 if(R->AliveEnemyCount>0)return Failure(EWorldOperationResultCode::Conflict,Op,TEXT("Current wave still has unresolved enemies."));
 int32 Next=R->CurrentWaveIndex+1;if(!P->IsValidIndex(Next)){R->Phase=ERaidPhase::Completed;QueuedThreatCount=FMath::Max(0,QueuedThreatCount-1);++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("All waves completed.")),{Id});}
 R->CurrentWaveIndex=Next;R->Phase=ERaidPhase::Active;++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Wave activated for adapter-driven spawning.")),{Id});
}
FWorldOperationResult URaidDirectorSubsystem::RecordSpawnedEnemies(FGuid Id,int32 Count,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);TArray<FRaidWavePlan>* P=WavePlans.Find(Id);
 if(!R||!P||R->Phase!=ERaidPhase::Active||!P->IsValidIndex(R->CurrentWaveIndex)||Count<=0)return Failure(EWorldOperationResultCode::PrerequisiteNotMet,Op,TEXT("Active wave and positive spawn count are required."));
 FRaidWavePlan& W=(*P)[R->CurrentWaveIndex];if(Count>W.PlannedEnemies-W.SpawnedEnemies||Count>Config.MaxAliveThreats-AliveThreatCount)return Failure(EWorldOperationResultCode::CapacityExceeded,Op,TEXT("Spawn count exceeds wave budget or alive limit."));
 W.SpawnedEnemies+=Count;R->AliveEnemyCount+=Count;R->QueuedEnemyCount+=Count;AliveThreatCount+=Count;++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Population recorded; call only after actual spawn success.")),{Id});
}
FWorldOperationResult URaidDirectorSubsystem::ResolveEnemies(FGuid Id,int32 Count,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);TArray<FRaidWavePlan>* P=WavePlans.Find(Id);
 if(!R||!P||R->Phase!=ERaidPhase::Active||!P->IsValidIndex(R->CurrentWaveIndex)||Count<=0||Count>R->AliveEnemyCount)return Failure(EWorldOperationResultCode::InvalidState,Op,TEXT("Resolved enemy count is invalid."));
 R->AliveEnemyCount-=Count;R->QueuedEnemyCount=FMath::Max(0,R->QueuedEnemyCount-Count);AliveThreatCount=FMath::Max(0,AliveThreatCount-Count);(*P)[R->CurrentWaveIndex].ResolvedEnemies+=Count;if(R->AliveEnemyCount==0)R->Phase=ERaidPhase::Recovering;++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Enemy resolutions recorded.")),{Id});
}
FWorldOperationResult URaidDirectorSubsystem::CancelRaid(FGuid Id,FGuid Op){
 auto V=ValidateNewOperation(Op);if(!V.IsSuccess())return V;FRaidRuntimeState* R=Raids.Find(Id);
 if(!R||R->Phase==ERaidPhase::Completed||R->Phase==ERaidPhase::Cancelled||R->Phase==ERaidPhase::Failed)return Failure(EWorldOperationResultCode::InvalidTarget,Op,TEXT("Raid is missing or terminal."));
 if(R->AliveEnemyCount>0)return Failure(EWorldOperationResultCode::Conflict,Op,TEXT("Resolve registered live enemies before cancellation."));
 R->Phase=ERaidPhase::Cancelled;QueuedThreatCount=FMath::Max(0,QueuedThreatCount-1);++R->Revision;RememberOperation(Op);return FWorldOperationResult::Success(Op,FText::FromString(TEXT("Raid cancelled without live population leak.")),{Id});
}
bool URaidDirectorSubsystem::GetRaidState(FGuid Id,FRaidRuntimeState& Out)const{const auto* S=Raids.Find(Id);if(!S)return false;Out=*S;return true;}
TArray<FRaidWavePlan> URaidDirectorSubsystem::GetWavePlans(FGuid Id)const{const auto* P=WavePlans.Find(Id);return P?*P:TArray<FRaidWavePlan>();}
FRaidDirectorSnapshot URaidDirectorSubsystem::GetSnapshot()const{FRaidDirectorSnapshot S;S.LastEvaluatedWorldTick=LastEvaluatedWorldTick;S.QueuedThreatCount=QueuedThreatCount;S.AliveThreatCount=AliveThreatCount;for(const auto& P:Raids)if(P.Value.Phase==ERaidPhase::Active)++S.ActiveRaidCount;return S;}
