#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WorldStructuralGraphSubsystem.h"
#include "StructuralPieceInterface.generated.h"

/** Material-neutral view of one connection anchor on a physical piece. */
struct FStructuralAnchorView
{
    FName AnchorId = NAME_None;
    FName Tag = NAME_None;
    FTransform LocalTransform = FTransform::Identity;
    bool bCanReceive = true;
    bool bCanConnect = true;
    bool bOccupied = false;
};

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UStructuralPieceInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * Shared contract for log, stone and stick actors so pieces of different
 * materials can connect through one rule table. Same-material connections keep
 * using each domain's own API; the canonical graph stays the authority.
 */
class MYPROJECT3_API IStructuralPieceInterface
{
    GENERATED_BODY()

public:
    virtual EStructuralNodeKind GetStructuralKind() const = 0;
    virtual FGuid GetStructuralNodeId() const = 0;
    virtual bool GetStructuralAnchor(FName AnchorId, FStructuralAnchorView& OutAnchor) const = 0;
    virtual void GetStructuralAnchorIds(TArray<FName>& OutAnchorIds) const = 0;
    virtual bool SetStructuralAnchorOccupied(FName AnchorId, bool bOccupied) = 0;
    virtual bool IsStructurallyPlaced() const = 0;

    /** True for a loose piece with no parent, no children and no cross-material parent. */
    virtual bool CanAttachAsCrossMaterialChild() const = 0;
    /** Marks the piece as placed on another material; physics stops. */
    virtual void ApplyCrossMaterialAttachment(AActor* Parent) = 0;
    /** Returns the piece to a loose physical state; never touches the graph. */
    virtual void ClearCrossMaterialAttachment() = 0;
    virtual AActor* GetCrossMaterialParent() const = 0;
};
