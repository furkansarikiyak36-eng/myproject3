#include "GameplayCoreTypes.h"

FWorldOperationResult FWorldOperationResult::Success(
    const FGuid& InOperationId,
    const FText& InMessage,
    const TArray<FGuid>& InAffectedObjectIds)
{
    FWorldOperationResult Result;
    Result.bSucceeded = true;
    Result.Code = EWorldOperationResultCode::Success;
    Result.OperationId = InOperationId.IsValid() ? InOperationId : FGuid::NewGuid();
    Result.Message = InMessage;
    Result.AffectedObjectIds = InAffectedObjectIds;
    return Result;
}

FWorldOperationResult FWorldOperationResult::Failure(
    EWorldOperationResultCode InCode,
    const FGuid& InOperationId,
    const FText& InMessage)
{
    FWorldOperationResult Result;
    Result.bSucceeded = false;
    Result.Code = (InCode == EWorldOperationResultCode::Success)
        ? EWorldOperationResultCode::InternalError
        : InCode;
    Result.OperationId = InOperationId.IsValid() ? InOperationId : FGuid::NewGuid();
    Result.Message = InMessage;
    Result.AffectedObjectIds.Reset();
    return Result;
}
