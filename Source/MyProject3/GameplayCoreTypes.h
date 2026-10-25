#pragma once

#include "CoreMinimal.h"
#include "GameplayCoreTypes.generated.h"

/**
 * Machine-readable outcomes shared by gameplay domains.
 * Keep these codes stable; UI text is descriptive and is not an identifier.
 */
UENUM(BlueprintType)
enum class EWorldOperationResultCode : uint8
{
    Success UMETA(DisplayName = "Success"),
    InvalidTarget UMETA(DisplayName = "Invalid Target"),
    OutOfRange UMETA(DisplayName = "Out Of Range"),
    MissingTool UMETA(DisplayName = "Missing Tool"),
    MissingMaterial UMETA(DisplayName = "Missing Material"),
    InvalidState UMETA(DisplayName = "Invalid State"),
    PrerequisiteNotMet UMETA(DisplayName = "Prerequisite Not Met"),
    BlockedByPlacement UMETA(DisplayName = "Blocked By Placement"),
    UnsupportedSurface UMETA(DisplayName = "Unsupported Surface"),
    CapacityExceeded UMETA(DisplayName = "Capacity Exceeded"),
    NotPermitted UMETA(DisplayName = "Not Permitted"),
    MissingConfiguration UMETA(DisplayName = "Missing Configuration"),
    InsufficientResources UMETA(DisplayName = "Insufficient Resources"),
    Conflict UMETA(DisplayName = "Conflict"),
    Unsupported UMETA(DisplayName = "Unsupported"),
    InternalError UMETA(DisplayName = "Internal Error")
};

/**
 * Generic lifecycle only. Wood-cut state, crop growth, durability, and other
 * domain-specific state must remain owned by their respective systems.
 */
UENUM(BlueprintType)
enum class EWorldObjectLifecycleState : uint8
{
    World UMETA(DisplayName = "In World"),
    Held UMETA(DisplayName = "Held"),
    Placed UMETA(DisplayName = "Placed"),
    Consumed UMETA(DisplayName = "Consumed"),
    Destroyed UMETA(DisplayName = "Destroyed")
};

/**
 * A result envelope for one attempted gameplay operation.
 * Failure results must not contain a success code or imply a committed change.
 */
USTRUCT(BlueprintType)
struct FWorldOperationResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    bool bSucceeded = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    EWorldOperationResultCode Code = EWorldOperationResultCode::InternalError;

    /** Unique diagnostic/request identifier; may be invalid for legacy callers. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    FGuid OperationId;

    /** Human-readable explanation. Code remains the stable machine-readable value. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    FText Message;

    /** Stable IDs of objects actually changed by a successful commit. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Operation")
    TArray<FGuid> AffectedObjectIds;

    bool IsSuccess() const
    {
        return bSucceeded && Code == EWorldOperationResultCode::Success;
    }

    static FWorldOperationResult Success(
        const FGuid& InOperationId,
        const FText& InMessage,
        const TArray<FGuid>& InAffectedObjectIds);

    static FWorldOperationResult Failure(
        EWorldOperationResultCode InCode,
        const FGuid& InOperationId,
        const FText& InMessage);
};
