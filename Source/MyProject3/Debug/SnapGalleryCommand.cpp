// Console command mp3.SnapGallery: builds every placement kind next to the player
// through the player's own path (pick up, look, contextual preview, place) and
// takes a screenshot of each preview and result. For checking placement by eye.
//
//   UnrealEditor.exe MyProject3.uproject /Game/DemoAssets/Maps/VerticalSlice_C -game -windowed
//       -ResX=1280 -ResY=720 -ExecCmds="mp3.SnapGallery quit"
//
// Screenshots go to Saved/Screenshots/SnapGallery; a summary line per case goes to the log (LogSnapGallery).

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraComponent.h"
#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "Placement/PlacementFeatureProvider.h"
#include "Placement/PlacementKernelSubsystem.h"
#include "VerticalSliceBWorldActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSnapGallery, Log, All);

namespace
{
    class FSnapGallery : public TSharedFromThis<FSnapGallery>
    {
    public:
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<APlayerController> Controller;
        TWeakObjectPtr<ULogCarryComponent> Carry;
        TArray<TFunction<void()>> Stages;
        FVector Base = FVector::ZeroVector;
        FLogProcessingMeshSet Family;
        FTimerHandle Timer;
        bool bQuitWhenDone = false;
        int32 Next = 0;

        UPlacementKernelSubsystem* Kernel() const { return World.IsValid() ? World->GetSubsystem<UPlacementKernelSubsystem>() : nullptr; }

        float GroundZ(const FVector2D& At) const
        {
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(SnapGalleryGround), false, Controller.IsValid() ? Controller->GetPawn() : nullptr);
            const FVector From(At.X, At.Y, Base.Z + 5000.0f);
            return World->LineTraceSingleByChannel(Hit, From, From - FVector(0, 0, 20000.0f), ECC_Visibility, Params) ? Hit.ImpactPoint.Z : Base.Z;
        }

        ALogBuildPiece* Spawn(const FVector& At)
        {
            const FTransform Transform(FRotator::ZeroRotator, At);
            ALogBuildPiece* Log = World->SpawnActorDeferred<ALogBuildPiece>(ALogBuildPiece::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Log->BaseDefinitionId = TEXT("Gallery.Log");
            Log->SelectedProcessingMeshSetId = Family.VariantSetId;
            Log->ProcessingMeshSets.Add(Family);
            Log->SetPieceStaticMesh(Family.BaseRoundMesh);
            Log->FinishSpawning(Transform);
            Log->GetLogMesh()->SetSimulatePhysics(false);
            return Log;
        }

        static FPlacementBody BodyOf(const AActor* Piece)
        {
            FPlacementBody Body;
            FPlacementProviderRegistry::GetBody(Piece, Body);
            return Body;
        }

        /** A built log with its body along Axis, centred at Centre, resting on Support (or nothing). */
        ALogBuildPiece* BuildLog(const FVector& Centre, const FVector& Axis, AActor* Support = nullptr)
        {
            ALogBuildPiece* Log = Spawn(Centre + FVector(0, 0, 3000));
            FPlacementPose Pose;
            Pose.Centre = Centre;
            Pose.Axis = Axis.GetSafeNormal();
            Pose.Up = FMath::Abs(Pose.Axis.Z) > 0.9f ? FVector::ForwardVector : FVector::UpVector;
            UPlacementKernelSubsystem* K = Kernel();
            const FTransform Transform = K->MakeBodyTransform(Log, Pose);
            FPlacementCommitOptions Options;
            Options.FromOwner = EWorldItemOwnerKind::World;
            Options.FromLifecycle = EWorldObjectLifecycleState::World;
            FString Reason;
            if (!K->CommitPlacement(Log, K->MakeDirectSolution(Log, Transform, TEXT("piece.free_place"), Support), Options, Reason))
                UE_LOG(LogSnapGallery, Warning, TEXT("Could not build a support: %s"), *Reason);
            return Log;
        }

        ALogBuildPiece* Lying(const FVector2D& At, float YawDeg, AActor* Support = nullptr)
        {
            ALogBuildPiece* Probe = Spawn(FVector(At, GroundZ(At) + 3000));
            const float Radius = BodyOf(Probe).Radius;
            Probe->Destroy();
            return BuildLog(FVector(At, GroundZ(At) + Radius), FRotator(0, YawDeg, 0).Vector(), Support);
        }

        ALogBuildPiece* Standing(const FVector2D& At, float HeightCm)
        {
            ALogBuildPiece* Probe = Spawn(FVector(At, GroundZ(At) + 3000));
            const float Half = BodyOf(Probe).HalfLength;
            Probe->Destroy();
            // A post sunk into the ground so its top stands HeightCm high.
            return BuildLog(FVector(At, GroundZ(At) + HeightCm - Half), FVector::UpVector);
        }

        void Hold()
        {
            const FVector Near = Controller->GetPawn()->GetActorLocation() + FVector(0, 0, 200);
            ALogBuildPiece* Held = Spawn(Near);
            if (!Carry->TryPickupPiece(Held))
                UE_LOG(LogSnapGallery, Warning, TEXT("Pick up failed"));
        }

        /** The camera at Point - view * Distance, looking at Point. */
        void Look(const FVector& Point, float YawDeg, float PitchDeg, float Distance)
        {
            const FRotator View(PitchDeg, YawDeg, 0.0f);
            const FVector Camera = Point - View.Vector() * Distance;
            APawn* Pawn = Controller->GetPawn();
            // Turn first: the camera's offset from the pawn turns with it.
            Controller->SetControlRotation(View);
            Pawn->SetActorRotation(FRotator(0.0f, YawDeg, 0.0f), ETeleportType::TeleportPhysics);
            const UCameraComponent* Eye = Pawn->FindComponentByClass<UCameraComponent>();
            const FVector EyeOffset = Eye ? Eye->GetComponentLocation() - Pawn->GetActorLocation() : FVector::ZeroVector;
            Pawn->SetActorLocation(Camera - EyeOffset, false, nullptr, ETeleportType::TeleportPhysics);
        }

        void Shot(const FString& Name)
        {
            const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("SnapGallery"), Name + TEXT(".png"));
            FScreenshotRequest::RequestScreenshot(Path, true, false);
        }

        void Report(const FString& Name)
        {
            const AActor* Held = Carry->HeldPiece.Get();
            const FPlacementBody Body = Held ? BodyOf(Held) : FPlacementBody();
            FVector EyeLocation;
            FRotator EyeRotation;
            Controller->GetPlayerViewPoint(EyeLocation, EyeRotation);
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(SnapGalleryAim), false, Controller->GetPawn());
            Params.AddIgnoredActor(Held);
            World->LineTraceSingleByChannel(Hit, EyeLocation, EyeLocation + EyeRotation.Vector() * 1000.0f, ECC_Visibility, Params);
            UE_LOG(LogSnapGallery, Display, TEXT("%s: aim hits %s at %s"), *Name, *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString());
            UE_LOG(LogSnapGallery, Display, TEXT("%s: action %d, status '%s', held yaw %.1f, centre %s"), *Name,
                static_cast<int32>(Carry->GetContextAction()), *Carry->GetPlacementStatus(),
                FMath::RadiansToDegrees(FMath::Atan2(Body.Axis.Y, Body.Axis.X)), *Body.Centre.ToString());
        }

        /** Hold a log, look, show the preview, place it, show the result from above. */
        void Case(const FString& Name, TFunction<void()> Aim, const FVector& Overview)
        {
            Stages.Add([this] { Hold(); });
            Stages.Add(MoveTemp(Aim));
            Stages.Add([this, Name] { Report(Name + TEXT(" preview")); Shot(Name + TEXT("_1_preview")); });
            Stages.Add([this, Name, Overview]
            {
                const bool bPlaced = Carry->PlaceHeldPiece();
                UE_LOG(LogSnapGallery, Display, TEXT("%s: placed %d, status '%s'"), *Name, bPlaced ? 1 : 0, *Carry->GetPlacementStatus());
                if (!bPlaced)
                    Carry->DropHeldPiece();
                Look(Overview, 30.0f, -55.0f, 1100.0f);
            });
            Stages.Add([this, Name] { Shot(Name + TEXT("_2_placed")); });
        }

        void Setup()
        {
            int32 Row = 0;
            auto Spot = [this, &Row](float X = 0.0f) { return FVector2D(Base.X + X, Base.Y + 1500.0f * Row); };

            // End to end: bends of 45, 90, 135 and -90 from a beam's +X end.
            for (const float Bend : { 45.0f, 90.0f, 135.0f, -90.0f })
            {
                ALogBuildPiece* Beam = Lying(Spot(), 0.0f);
                const FPlacementBody BeamBody = BodyOf(Beam);
                // Standing, looking down at the last few cm of the log, as a player does.
                const FVector End = BeamBody.EndB() - BeamBody.Axis * 12.0f;
                Case(FString::Printf(TEXT("bend_%+.0f"), Bend), [this, End, Bend] { Look(End, Bend, -30.0f, 300.0f); }, End);
                ++Row;
            }
            // A post alone, carried at 45 degrees.
            {
                ALogBuildPiece* Post = Standing(Spot(), 250.0f);
                const FVector Top = BodyOf(Post).Top();
                Case(TEXT("post_45"), [this, Top] { Look(Top, 45.0f, -25.0f, 350.0f); }, Top);
                ++Row;
            }
            // Two posts on a diagonal: post to post, centred.
            {
                ALogBuildPiece* Post = Standing(Spot(), 250.0f);
                Standing(Spot(330.0f) + FVector2D(0, 230.0f), 250.0f);
                const FVector Top = BodyOf(Post).Top();
                Case(TEXT("post_span"), [this, Top] { Look(Top, 35.0f, -25.0f, 350.0f); }, Top);
                ++Row;
            }
            // A wall: a diagonal, then its mirror.
            {
                ALogBuildPiece* Wall = Lying(Spot(), 0.0f);
                const FPlacementBody Body = BodyOf(Wall);
                const FVector Left = Body.Centre - Body.Axis * 187.5f;
                const FVector Right = Body.Centre + Body.Axis * 187.5f;
                Case(TEXT("wall_diagonal"), [this, Left] { Look(Left, 45.0f, -40.0f, 350.0f); }, Body.Centre);
                Case(TEXT("wall_mirror"), [this, Right] { Look(Right, -45.0f, -40.0f, 350.0f); }, Body.Centre);
                ++Row;
            }
            // A frame of two parallel beams: corner to corner from an end station.
            {
                ALogBuildPiece* BeamA = Lying(Spot(), 0.0f);
                Lying(Spot(150.0f) + FVector2D(0, 250.0f), 0.0f);
                const FPlacementBody Body = BodyOf(BeamA);
                const FVector Corner = Body.EndA() + Body.Axis * 37.5f;
                Case(TEXT("frame_corner"), [this, Corner] { Look(Corner, 50.0f, -40.0f, 350.0f); }, Body.Centre);
                ++Row;
            }
            // A post carrying a beam: a strut on each side.
            {
                ALogBuildPiece* Post = Standing(Spot(), 300.0f);
                const FPlacementBody PostBody = BodyOf(Post);
                ALogBuildPiece* Probe = Spawn(PostBody.Top() + FVector(0, 0, 3000));
                const float BeamRadius = BodyOf(Probe).Radius;
                Probe->Destroy();
                BuildLog(PostBody.Top() + FVector(20.0f, 0, BeamRadius), FVector::ForwardVector, Post);
                const FVector SideA = PostBody.Top() + FVector(PostBody.Radius, 0, -150.0f);
                const FVector SideB = PostBody.Top() + FVector(-PostBody.Radius, 0, -150.0f);
                Case(TEXT("strut_a"), [this, SideA] { Look(SideA, 180.0f, -5.0f, 300.0f); }, PostBody.Top());
                Case(TEXT("strut_b"), [this, SideB] { Look(SideB, 0.0f, -5.0f, 300.0f); }, PostBody.Top());
                ++Row;
            }
            Stages.Add([this] { UE_LOG(LogSnapGallery, Display, TEXT("Snap gallery done.")); });
        }

        void Step()
        {
            if (!World.IsValid() || !Controller.IsValid() || !Carry.IsValid())
                return;
            if (Next < Stages.Num())
            {
                Stages[Next++]();
                return;
            }
            World->GetTimerManager().ClearTimer(Timer);
            if (bQuitWhenDone && GEngine)
                GEngine->Exec(World.Get(), TEXT("quit"));
        }
    };

    TSharedPtr<FSnapGallery> ActiveGallery;

    void StartGallery(const TArray<FString>& Args, UWorld* World)
    {
        APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        ULogCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<ULogCarryComponent>() : nullptr;
        if (!Carry)
        {
            // Started from the command line before the pawn exists: try again shortly.
            if (World)
            {
                FTimerHandle Retry;
                World->GetTimerManager().SetTimer(Retry, FTimerDelegate::CreateLambda([Args, World] { StartGallery(Args, World); }), 1.0f, false);
            }
            return;
        }
        if (ACharacter* Character = Cast<ACharacter>(Pawn))
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        TSharedPtr<FSnapGallery> Gallery = MakeShared<FSnapGallery>();
        Gallery->World = World;
        Gallery->Controller = Controller;
        Gallery->Carry = Carry;
        Gallery->bQuitWhenDone = Args.Contains(TEXT("quit"));
        Gallery->Family = AVerticalSliceBWorldActor::BuildLogMeshFamily();
        // On a flat, lit platform high above the scene: no slope, trees or shade in the way.
        const FVector Above = Pawn->GetActorLocation();
        Gallery->Base = FVector(Above.X, Above.Y, 20000.0f);
        if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
        {
            AStaticMeshActor* Platform = World->SpawnActor<AStaticMeshActor>(Gallery->Base + FVector(0, 6000, -50), FRotator::ZeroRotator);
            Platform->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Platform->GetStaticMeshComponent()->SetStaticMesh(Cube);
            Platform->SetActorScale3D(FVector(200.0f, 200.0f, 1.0f));
        }
        Gallery->Setup();
        World->GetTimerManager().SetTimer(Gallery->Timer, FTimerDelegate::CreateSP(Gallery.ToSharedRef(), &FSnapGallery::Step),
            0.6f, true, 2.0f);
        ActiveGallery = Gallery;
        UE_LOG(LogSnapGallery, Display, TEXT("Snap gallery started at %s with %d stages."), *Gallery->Base.ToString(), Gallery->Stages.Num());
    }

    FAutoConsoleCommandWithWorldAndArgs SnapGalleryCommand(TEXT("mp3.SnapGallery"),
        TEXT("Builds every placement kind next to the player and screenshots each (arg 'quit' exits after)."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartGallery));
}
