#include "Placement/PlacementFeatureProvider.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StickPrimitivePiece.h"
#include "StoneBuildingPiece.h"
#include "StructuralPieceInterface.h"

/**
 * Stones and sticks for the placement kernel. Both expose the anchors they
 * already own (IStructuralPieceInterface) as features and keep making their own
 * joints; the kernel adds nothing material-specific.
 */
namespace
{
    template <typename TPiece>
    class TAnchoredPieceProvider : public IPlacementFeatureProvider
    {
    public:
        explicit TAnchoredPieceProvider(const TCHAR* InFamily) : FamilyName(InFamily) {}

        virtual bool Supports(const AActor* Piece) const override { return Cast<TPiece>(Piece) != nullptr; }
        virtual FName GetFamily(const AActor* Piece) const override { return FamilyName; }

        virtual uint32 GetGeometryRevision(const AActor* Piece) const override
        {
            const TPiece* Typed = Cast<TPiece>(Piece);
            if (!Typed)
                return 0;
            const FVector Scale = Typed->GetActorScale3D() * 1000.0;
            uint32 Hash = GetTypeHash(Typed->Mesh ? Typed->Mesh->GetStaticMesh() : nullptr);
            Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt(Scale.X), FMath::RoundToInt(Scale.Y), FMath::RoundToInt(Scale.Z))));
            return HashCombine(Hash, GetTypeHash(Typed->Anchors.Num()));
        }

        virtual void GetFeatures(const AActor* Piece, TArray<FPlacementFeature>& OutFeatures) const override
        {
            OutFeatures.Reset();
            const TPiece* Typed = Cast<TPiece>(Piece);
            if (!Typed)
                return;
            // A query only: the piece adds its top and bottom anchors itself (BeginPlay, pickup).
            TArray<FName> Ids;
            Typed->GetStructuralAnchorIds(Ids);
            for (const FName& Id : Ids)
            {
                FStructuralAnchorView View;
                if (!Typed->GetStructuralAnchor(Id, View))
                    continue;
                FPlacementFeature Feature;
                Feature.Id = Id;
                Feature.Kind = EPlacementFeatureKind::Authored;
                Feature.Tag = View.Tag;
                Feature.LocalFrame = View.LocalTransform;
                Feature.WorldFrame = View.LocalTransform * Typed->GetActorTransform();
                Feature.bFlipIncoming = false;
                Feature.bCanSource = View.bCanConnect;
                Feature.bCanReceive = View.bCanReceive;
                Feature.bOccupied = View.bOccupied;
                OutFeatures.Add(Feature);
            }
        }

        virtual bool CanLink(const AActor* Target, const FPlacementFeature& TargetFeature,
            const AActor* Source, const FPlacementFeature& SourceFeature) const override
        {
            const TPiece* TargetPiece = Cast<TPiece>(Target);
            return TargetPiece && Cast<TPiece>(Source) && Target != Source && TargetPiece->IsPlaced() &&
                !TargetFeature.bOccupied && TargetFeature.bCanReceive;
        }

        virtual bool CommitLink(AActor* Target, const FPlacementFeature& TargetFeature,
            AActor* Source, const FPlacementFeature& SourceFeature) const override
        {
            TPiece* TargetPiece = Cast<TPiece>(Target);
            TPiece* SourcePiece = Cast<TPiece>(Source);
            if (!TargetPiece || !SourcePiece)
                return false;
            // The piece ends where the preview showed it, even if the joint snaps it elsewhere.
            const FTransform Wanted = SourcePiece->GetActorTransform();
            if (!Connect(SourcePiece, TargetPiece, TargetFeature.Id, SourceFeature.Id))
                return false;
            SourcePiece->SetActorTransform(Wanted, false, nullptr, ETeleportType::TeleportPhysics);
            return true;
        }

        virtual bool RevertLink(AActor* Target, const FPlacementFeature& TargetFeature,
            AActor* Source, const FPlacementFeature& SourceFeature) const override
        {
            TPiece* SourcePiece = Cast<TPiece>(Source);
            // The graph edge and the anchors go together; then the piece is back in the hands.
            if (!SourcePiece || !SourcePiece->RemoveConnection())
                return false;
            SourcePiece->SetHeldState(true);
            return true;
        }

        virtual bool IsPlaced(const AActor* Piece) const override
        {
            const TPiece* Typed = Cast<TPiece>(Piece);
            return Typed && Typed->IsPlaced() && !Typed->IsHeld();
        }

        virtual bool HasOwnGroundPlacement() const override { return true; }

        virtual bool CommitGroundPlacement(AActor* Piece, const FTransform& Transform) const override
        {
            TPiece* Typed = Cast<TPiece>(Piece);
            return Typed && Typed->TryCommitGroundPlacement(Transform.GetLocation(), Transform.Rotator());
        }

    protected:
        virtual bool Connect(TPiece* Source, TPiece* Target, FName TargetAnchor, FName OwnAnchor) const
        {
            return Source->TryConnectToAnchor(Target, TargetAnchor, OwnAnchor);
        }

    private:
        FName FamilyName;
    };

    class FStoneFeatureProvider : public TAnchoredPieceProvider<AStoneBuildingPiece>
    {
    public:
        FStoneFeatureProvider() : TAnchoredPieceProvider(TEXT("stone")) {}

        virtual bool CanLink(const AActor* Target, const FPlacementFeature& TargetFeature,
            const AActor* Source, const FPlacementFeature& SourceFeature) const override
        {
            const AStoneBuildingPiece* TargetStone = Cast<AStoneBuildingPiece>(Target);
            const AStoneBuildingPiece* SourceStone = Cast<AStoneBuildingPiece>(Source);
            return TargetStone && SourceStone && TargetStone->IsPlaced() &&
                SourceStone->CanConnectToAnchor(TargetStone, TargetFeature.Id, SourceFeature.Id);
        }
    };

    class FStickFeatureProvider : public TAnchoredPieceProvider<AStickPrimitivePiece>
    {
    public:
        FStickFeatureProvider() : TAnchoredPieceProvider(TEXT("stick")) {}

        /** A stick is a long body: it can lean and other pieces can rest along it. */
        virtual bool GetBody(const AActor* Piece, FPlacementBody& OutBody) const override
        {
            const AStickPrimitivePiece* Stick = Cast<AStickPrimitivePiece>(Piece);
            const UStaticMeshComponent* Mesh = Stick ? Stick->Mesh.Get() : nullptr;
            if (!Mesh || !Mesh->GetStaticMesh())
                return false;
            const FTransform Transform = Mesh->GetComponentTransform();
            const FBoxSphereBounds Bounds = Mesh->GetStaticMesh()->GetBounds();
            const FVector Extent = Bounds.BoxExtent * Transform.GetScale3D().GetAbs();
            OutBody.Centre = Transform.TransformPosition(Bounds.Origin);
            OutBody.Axis = Transform.GetUnitAxis(EAxis::X);
            OutBody.HalfLength = Extent.X;
            OutBody.Radius = FMath::Max(Extent.Y, Extent.Z);
            return OutBody.HalfLength > KINDA_SMALL_NUMBER;
        }

    protected:
        virtual bool Connect(AStickPrimitivePiece* Source, AStickPrimitivePiece* Target, FName TargetAnchor, FName OwnAnchor) const override
        {
            // Stick joints take a loose stick; physics stays off so it can attach.
            Source->SetHeldState(false);
            if (Source->Mesh)
                Source->Mesh->SetSimulatePhysics(false);
            if (Source->TryConnectToAnchor(Target, TargetAnchor, OwnAnchor))
                return true;
            Source->SetHeldState(true);
            return false;
        }
    };
}

REGISTER_PLACEMENT_PROVIDER(FStoneFeatureProvider)
REGISTER_PLACEMENT_PROVIDER(FStickFeatureProvider)
