#include "MaterialConnectionSubsystem.h"

#include "StructuralPieceInterface.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
    IStructuralPieceInterface* AsPiece(AActor* Actor)
    {
        return IsValid(Actor) ? Cast<IStructuralPieceInterface>(Actor) : nullptr;
    }

    const IStructuralPieceInterface* AsPiece(const AActor* Actor)
    {
        return IsValid(Actor) ? Cast<const IStructuralPieceInterface>(Actor) : nullptr;
    }

    bool TagMatches(FName RuleTag, FName AnchorTag)
    {
        return RuleTag.IsNone() || RuleTag == AnchorTag;
    }
}

void UMaterialConnectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Defaults cover the mixed joints the first construction slice needs; a
    // project can replace them with authored rules.
    auto AddRule = [this](EStructuralNodeKind ParentKind, EStructuralNodeKind ChildKind, float Strength)
    {
        FMaterialConnectionRule Rule;
        Rule.ParentKind = ParentKind;
        Rule.ChildKind = ChildKind;
        Rule.ConnectionStrength = Strength;
        Rules.Add(Rule);
    };
    AddRule(EStructuralNodeKind::Stone, EStructuralNodeKind::Wood, 1.0f);  // log wall on stone footing
    AddRule(EStructuralNodeKind::Stone, EStructuralNodeKind::Stick, 0.5f); // stake braced on stone
    AddRule(EStructuralNodeKind::Wood, EStructuralNodeKind::Stick, 0.5f);  // rafter or rail on a log
    AddRule(EStructuralNodeKind::Wood, EStructuralNodeKind::Stone, 0.8f);  // hearth stone on a log beam
}

const FMaterialConnectionRule* UMaterialConnectionSubsystem::FindRule(
    EStructuralNodeKind ParentKind, FName ParentTag, EStructuralNodeKind ChildKind, FName ChildTag) const
{
    for (const FMaterialConnectionRule& Rule : Rules)
    {
        if (Rule.ParentKind == ParentKind && Rule.ChildKind == ChildKind &&
            TagMatches(Rule.ParentAnchorTag, ParentTag) && TagMatches(Rule.ChildAnchorTag, ChildTag) &&
            FMath::IsFinite(Rule.ConnectionStrength) && Rule.ConnectionStrength > 0.0f)
        {
            return &Rule;
        }
    }
    return nullptr;
}

bool UMaterialConnectionSubsystem::CanConnect(
    AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId, FString& OutReason) const
{
    const IStructuralPieceInterface* ParentPiece = AsPiece(Parent);
    const IStructuralPieceInterface* ChildPiece = AsPiece(Child);
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!ParentPiece || !ChildPiece || Parent == Child || !Graph)
    {
        OutReason = TEXT("Both actors must be structural pieces in the same world.");
        return false;
    }
    if (ParentPiece->GetStructuralKind() == ChildPiece->GetStructuralKind())
    {
        OutReason = TEXT("Same-material joints use that material's own connection API.");
        return false;
    }

    FStructuralAnchorView ParentAnchor;
    FStructuralAnchorView ChildAnchor;
    if (!ParentPiece->GetStructuralAnchor(ParentAnchorId, ParentAnchor) ||
        !ChildPiece->GetStructuralAnchor(ChildAnchorId, ChildAnchor))
    {
        OutReason = TEXT("A requested anchor does not exist.");
        return false;
    }
    if (!ParentAnchor.bCanReceive || ParentAnchor.bOccupied ||
        !ChildAnchor.bCanConnect || ChildAnchor.bOccupied)
    {
        OutReason = TEXT("An anchor is occupied or cannot take this role.");
        return false;
    }
    if (!FindRule(ParentPiece->GetStructuralKind(), ParentAnchor.Tag,
        ChildPiece->GetStructuralKind(), ChildAnchor.Tag))
    {
        OutReason = TEXT("No material connection rule allows this pairing.");
        return false;
    }
    if (!ParentPiece->IsStructurallyPlaced() || !Graph->HasValidSupportPath(ParentPiece->GetStructuralNodeId()))
    {
        OutReason = TEXT("The supporting piece is not placed on a valid support path.");
        return false;
    }
    if (!ChildPiece->CanAttachAsCrossMaterialChild())
    {
        OutReason = TEXT("The attached piece must be loose, unparented and carry nothing.");
        return false;
    }
    return true;
}

bool UMaterialConnectionSubsystem::TryConnect(
    AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId, FString& OutReason)
{
    if (!CanConnect(Parent, ParentAnchorId, Child, ChildAnchorId, OutReason))
        return false;

    IStructuralPieceInterface* ParentPiece = AsPiece(Parent);
    IStructuralPieceInterface* ChildPiece = AsPiece(Child);
    UWorldStructuralGraphSubsystem* Graph = GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>();

    FStructuralAnchorView ParentAnchor;
    FStructuralAnchorView ChildAnchor;
    ParentPiece->GetStructuralAnchor(ParentAnchorId, ParentAnchor);
    ChildPiece->GetStructuralAnchor(ChildAnchorId, ChildAnchor);

    // The graph edge is the fallible authoritative step; commit it first.
    if (!Graph->TryConnect(ParentPiece->GetStructuralNodeId(), ParentAnchorId,
        ChildPiece->GetStructuralNodeId(), ChildAnchorId))
    {
        OutReason = TEXT("The structural graph rejected the joint (support, load, cycle or anchor conflict).");
        return false;
    }

    const FTransform PreviousTransform = Child->GetActorTransform();
    const FTransform ParentAnchorWorld = ParentAnchor.LocalTransform * Parent->GetActorTransform();
    const FTransform DesiredChildTransform = ChildAnchor.LocalTransform.Inverse() * ParentAnchorWorld;
    // A simulating body cannot be attached; stop physics before attaching.
    ChildPiece->ApplyCrossMaterialAttachment(Parent);
    Child->SetActorTransform(DesiredChildTransform, false, nullptr, ETeleportType::TeleportPhysics);
    if (!Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
    {
        ChildPiece->ClearCrossMaterialAttachment();
        Child->SetActorTransform(PreviousTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Graph->TryDisconnect(ChildPiece->GetStructuralNodeId());
        OutReason = TEXT("Actor attachment failed; the joint was rolled back.");
        return false;
    }

    ParentPiece->SetStructuralAnchorOccupied(ParentAnchorId, true);
    ChildPiece->SetStructuralAnchorOccupied(ChildAnchorId, true);
    OutReason.Reset();
    return true;
}

bool UMaterialConnectionSubsystem::TryDisconnect(AActor* Child)
{
    IStructuralPieceInterface* ChildPiece = AsPiece(Child);
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!ChildPiece || !Graph || !ChildPiece->GetCrossMaterialParent())
        return false;

    AActor* Parent = ChildPiece->GetCrossMaterialParent();
    IStructuralPieceInterface* ParentPiece = AsPiece(Parent);
    FStructuralNodeRecord Node;
    if (!ParentPiece || !Graph->GetNode(ChildPiece->GetStructuralNodeId(), Node) ||
        Node.ParentNodeId != ParentPiece->GetStructuralNodeId() ||
        !Graph->TryDisconnect(ChildPiece->GetStructuralNodeId()))
    {
        return false;
    }

    ParentPiece->SetStructuralAnchorOccupied(Node.ParentAnchorId, false);
    ChildPiece->SetStructuralAnchorOccupied(Node.OwnAnchorId, false);
    Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    ChildPiece->ClearCrossMaterialAttachment();
    return true;
}

void UMaterialConnectionSubsystem::ReleaseCrossMaterialChildren(AActor* Parent)
{
    const IStructuralPieceInterface* ParentPiece = AsPiece(Parent);
    UWorldStructuralGraphSubsystem* Graph =
        GetWorld() ? GetWorld()->GetSubsystem<UWorldStructuralGraphSubsystem>() : nullptr;
    if (!ParentPiece || !Graph)
        return;

    for (const FGuid& ChildId : Graph->GetDirectDependents(ParentPiece->GetStructuralNodeId()))
    {
        FStructuralNodeRecord ChildNode;
        if (!Graph->GetNode(ChildId, ChildNode))
            continue;
        AActor* ChildActor = ChildNode.Actor.Get();
        IStructuralPieceInterface* ChildPiece = AsPiece(ChildActor);
        if (!ChildPiece || ChildPiece->GetCrossMaterialParent() != Parent)
            continue;
        if (!TryDisconnect(ChildActor))
        {
            // The parent is leaving play regardless; never leave the child
            // pinned to a destroyed support with a stale placed state.
            ChildPiece->SetStructuralAnchorOccupied(ChildNode.OwnAnchorId, false);
            ChildActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            ChildPiece->ClearCrossMaterialAttachment();
        }
    }
}

bool UMaterialConnectionSubsystem::RestoreLinkFromGraph(
    AActor* Parent, FName ParentAnchorId, AActor* Child, FName ChildAnchorId)
{
    IStructuralPieceInterface* ParentPiece = AsPiece(Parent);
    IStructuralPieceInterface* ChildPiece = AsPiece(Child);
    FStructuralAnchorView ParentAnchor;
    FStructuralAnchorView ChildAnchor;
    if (!ParentPiece || !ChildPiece || Parent == Child ||
        !ParentPiece->GetStructuralAnchor(ParentAnchorId, ParentAnchor) ||
        !ChildPiece->GetStructuralAnchor(ChildAnchorId, ChildAnchor) ||
        ParentAnchor.bOccupied || ChildAnchor.bOccupied)
    {
        return false;
    }
    ChildPiece->ApplyCrossMaterialAttachment(Parent);
    if (!Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform))
    {
        ChildPiece->ClearCrossMaterialAttachment();
        return false;
    }
    ParentPiece->SetStructuralAnchorOccupied(ParentAnchorId, true);
    ChildPiece->SetStructuralAnchorOccupied(ChildAnchorId, true);
    return true;
}
