#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldStructuralGraphSubsystem.h"
#include "MaterialConnectionSubsystem.generated.h"

/** Which anchor of which material may carry which other material. */
USTRUCT(BlueprintType)
struct FMaterialConnectionRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials")
    EStructuralNodeKind ParentKind = EStructuralNodeKind::Stone;

    /** NAME_None matches any parent anchor tag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials")
    FName ParentAnchorTag = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials")
    EStructuralNodeKind ChildKind = EStructuralNodeKind::Wood;

    /** NAME_None matches any child anchor tag. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials")
    FName ChildAnchorTag = NAME_None;

    /** Relative joint strength for later load/damage tuning; must be positive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials", meta=(ClampMin="0.01"))
    float ConnectionStrength = 1.0f;
};

/**
 * Connects pieces of different materials (log, stone, stick) through one rule
 * table. A connection commits graph edge, anchor reservation and actor
 * attachment together, or none of them.
 */
UCLASS()
class MYPROJECT3_API UMaterialConnectionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure|Materials")
    TArray<FMaterialConnectionRule> Rules;

    UFUNCTION(BlueprintPure, Category="Structure|Materials")
    bool CanConnect(AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId,
        FString& OutReason) const;

    UFUNCTION(BlueprintCallable, Category="Structure|Materials")
    bool TryConnect(AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId,
        FString& OutReason);

    /** Same as TryConnect, but places the child at a caller-chosen world transform (for example upright). */
    bool TryConnectAt(AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId,
        const FTransform& ChildWorldTransform, FString& OutReason);

    /** Removes a cross-material child from its parent and makes it loose again. */
    UFUNCTION(BlueprintCallable, Category="Structure|Materials")
    bool TryDisconnect(AActor* Child);

    /** Called by a parent leaving play so cross-material children do not keep stale state. */
    void ReleaseCrossMaterialChildren(AActor* Parent);

    /** Rebuilds actor-local state for an edge the canonical graph already holds. */
    bool RestoreLinkFromGraph(AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId);

    const FMaterialConnectionRule* FindRule(EStructuralNodeKind ParentKind, FName ParentTag,
        EStructuralNodeKind ChildKind, FName ChildTag) const;
};
