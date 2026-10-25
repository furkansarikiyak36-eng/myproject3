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

/** Committed facts only; requests and previews are not world events. */
UENUM(BlueprintType)
enum class EWorldCommittedEventType : uint8
{
    None UMETA(DisplayName = "None"),
    ObjectProcessed UMETA(DisplayName = "Object Processed"),
    ResourceConsumed UMETA(DisplayName = "Resource Consumed"),
    ConstructionAttached UMETA(DisplayName = "Construction Attached"),
    ConstructionDetached UMETA(DisplayName = "Construction Detached"),
    ConstructionDamaged UMETA(DisplayName = "Construction Damaged"),
    TerrainChanged UMETA(DisplayName = "Terrain Changed"),
    CropStateChanged UMETA(DisplayName = "Crop State Changed"),
    AnimalStateChanged UMETA(DisplayName = "Animal State Changed"),
    WeatherChanged UMETA(DisplayName = "Weather Changed"),
    WorldHazardStarted UMETA(DisplayName = "World Hazard Started"),
    WorldHazardEnded UMETA(DisplayName = "World Hazard Ended"),
    DiscoveryRecorded UMETA(DisplayName = "Discovery Recorded")
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


/** Expected revision for optimistic stale-request rejection. */
USTRUCT(BlueprintType)
struct FWorldExpectedObjectRevision
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FGuid ObjectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    int64 ExpectedRevision = INDEX_NONE;
};

/**
 * Shared request envelope. Carries intent/context only; the owning domain
 * resolves IDs, validates prerequisites, computes changes, and commits.
 */
USTRUCT(BlueprintType)
struct FWorldOperationRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FGuid OperationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FName OperationType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FGuid RequestingObjectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    TArray<FGuid> TargetObjectIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FName SourceDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FName ToolDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FName RecipeDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    TArray<FWorldExpectedObjectRevision> ExpectedRevisions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FVector ContextLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Operation")
    FName ContextAnchorId = NAME_None;

    bool EnsureOperationId()
    {
        if (!OperationId.IsValid())
        {
            OperationId = FGuid::NewGuid();
        }
        return OperationId.IsValid();
    }

    bool IsStructurallyValid() const
    {
        if (!OperationId.IsValid() || OperationType.IsNone())
        {
            return false;
        }

        for (const FGuid& TargetId : TargetObjectIds)
        {
            if (!TargetId.IsValid())
            {
                return false;
            }
        }

        for (const FWorldExpectedObjectRevision& Expected : ExpectedRevisions)
        {
            if (!Expected.ObjectId.IsValid() || Expected.ExpectedRevision < 0)
            {
                return false;
            }
        }

        return true;
    }
};

/** Unitized resource quantity; inventory mutation remains domain-owned. */
USTRUCT(BlueprintType)
struct FWorldResourceAmount
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Resources")
    FName ResourceDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Resources")
    double Quantity = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Resources")
    FName UnitId = NAME_None;

    bool IsValid() const
    {
        return !ResourceDefinitionId.IsNone() &&
            !UnitId.IsNone() &&
            FMath::IsFinite(Quantity) &&
            Quantity >= 0.0;
    }

    bool CanConsume(double RequestedQuantity) const
    {
        return IsValid() &&
            FMath::IsFinite(RequestedQuantity) &&
            RequestedQuantity >= 0.0 &&
            Quantity >= RequestedQuantity;
    }
};

/** Versioned event payload for a fact that has already been committed. */
USTRUCT(BlueprintType)
struct FWorldCommittedEvent
{
    GENERATED_BODY()

    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    int32 SchemaVersion = CurrentSchemaVersion;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    EWorldCommittedEventType Type = EWorldCommittedEventType::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    FGuid EventId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    FGuid OperationId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    TArray<FGuid> AffectedObjectIds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    double WorldTimeSeconds = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gameplay|Events")
    FText Message;
};
