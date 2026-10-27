#include "LogSnapHUD.h"

#include "LogBuildPiece.h"
#include "LogCarryComponent.h"
#include "LogBuildToolsComponent.h"
#include "PhysicalHealthComponent.h"
#include "ShelterSensorComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void ALogSnapHUD::DrawCrosshair(float CentreX, float CentreY)
{
    // A small dot with a dark rim so it stays visible on bright and dark surfaces.
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), CentreX - 3.0f, CentreY - 3.0f, 6.0f, 6.0f);
    DrawRect(CrosshairColor, CentreX - 2.0f, CentreY - 2.0f, 4.0f, 4.0f);
}

void ALogSnapHUD::DrawWorldSegments(const TArray<FVector>& Points, const FLinearColor& Colour, float Thickness)
{
    for (int32 Index = 0; Index + 1 < Points.Num(); Index += 2)
    {
        const FVector A = Project(Points[Index], false);
        const FVector B = Project(Points[Index + 1], false);
        if (A.Z > 0.0f && B.Z > 0.0f)
            DrawLine(A.X, A.Y, B.X, B.Y, Colour, Thickness);
    }
}

void ALogSnapHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
        return;

    const float CentreX = Canvas->ClipX * 0.5f;
    const float CentreY = Canvas->ClipY * 0.5f;
    DrawCrosshair(CentreX, CentreY);

    APawn* Pawn = GetOwningPawn();
    const ULogCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<ULogCarryComponent>() : nullptr;
    if (!Carry)
        return;

    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    float TextY = CentreY + 28.0f;

    if (!Carry->IsCarryingLog() && !Carry->IsHoldingMaterialPiece())
    {
        const ALogBuildPiece* ArmedLog = nullptr;
        FLogCutChoice Choice;
        const ALogBuildPiece* Aimed = Carry->GetAimedLooseLog();
        // A marked cut stays shown until the player aims at another log.
        const bool bArmed = Carry->GetArmedCutChoice(ArmedLog, Choice) && (!Aimed || Aimed == ArmedLog);
        const ALogBuildPiece* Log = bArmed ? ArmedLog : Aimed;
        // Built pieces only show a cut when the length cut can shorten them (standing posts).
        if (Log && !bArmed && Log->IsPlaced() && !Carry->ResolveAimedCutChoice(Log, Choice))
            Log = nullptr;
        if (Log)
        {
            // The marked cut stays where it was marked; otherwise it follows the aim.
            const bool bResolved = bArmed || Carry->ResolveAimedCutChoice(Log, Choice);
            FString Reason;
            const bool bAllowed = bResolved && Carry->CanMakeCut(Log, Choice, Reason);
            const FLinearColor Colour = bAllowed ? AllowedCutColor : BlockedCutColor;

            TArray<FVector> Points;
            const bool bLines = Choice.Kind == ELogCutKind::Sharpen
                ? Carry->BuildSharpenSegments(Log, Points)
                : (Choice.Share >= 0.0f
                    ? Carry->BuildCutRingSegments(Log, Choice.Share, Points)
                    : Carry->BuildCutPreviewSegments(Log, Choice.Action, Points));
            if (bLines)
                DrawWorldSegments(Points, Colour, bArmed ? 3.0f : 2.0f);

            // The dashed line says where the axe goes; words only when it cannot cut there.
            const FString Label = !bResolved ? FString(TEXT("Too short to cut again")) : (!bAllowed ? Reason : FString());
            if (!Label.IsEmpty())
            {
                float W = 0.0f, H = 0.0f;
                GetTextSize(Label, W, H, Font);
                DrawText(Label, Colour, CentreX - W * 0.5f, TextY, Font);
                TextY += H + 4.0f;
            }
        }
    }
    else if (Carry->IsCarryingLog())
    {
        // One white dashed outline where the log will go; nothing else.
        TArray<FVector> Outline, Arrow;
        if (Carry->BuildPlacementPreviewSegments(Outline, Arrow))
            DrawWorldSegments(Outline, PlacementColor, 1.5f);
    }

    const FString Status = Carry->GetPlacementStatus();
    if (!Status.IsEmpty() && Status != TEXT("Placed.") &&
        (Carry->IsCarryingLog() || Carry->IsHoldingMaterialPiece() || !Status.StartsWith(TEXT("Aim"))))
    {
        float W = 0.0f, H = 0.0f;
        GetTextSize(Status, W, H, Font);
        DrawText(Status, FLinearColor::White, CentreX - W * 0.5f, TextY, Font);
        TextY += H + 4.0f;
    }

    const ULogBuildToolsComponent* Tools = Pawn->FindComponentByClass<ULogBuildToolsComponent>();
    if (Tools && !Tools->GetStatus().IsEmpty())
    {
        float W = 0.0f, H = 0.0f;
        GetTextSize(Tools->GetStatus(), W, H, Font);
        DrawText(Tools->GetStatus(), FLinearColor(1.0f, 0.85f, 0.4f), CentreX - W * 0.5f, TextY, Font);
    }

    // Corner readout: health, shelter, placement mode, selected plan.
    FString Corner;
    if (const UPhysicalHealthComponent* Health = Pawn->FindComponentByClass<UPhysicalHealthComponent>())
        Corner += FString::Printf(TEXT("Health %.0f   "), Health->Health);
    if (const UShelterSensorComponent* Shelter = Pawn->FindComponentByClass<UShelterSensorComponent>())
        Corner += FString::Printf(TEXT("Shelter %.0f%%   "), Shelter->GetShelterProtection() * 100.0f);
    // The mode only shows when the player chose one by hand.
    if (const UEnum* ModeEnum = StaticEnum<ELogPlacementMode>(); ModeEnum && Carry->IsManualPlacementMode())
        Corner += FString::Printf(TEXT("Mode: %s   "), *ModeEnum->GetDisplayNameTextByValue(static_cast<int64>(Carry->PlacementMode)).ToString());
    if (Tools)
        Corner += FString::Printf(TEXT("Plan: %s   F1 help"), *ABuildPlanActor::GetPlanName(Tools->GetSelectedPlan()));
    DrawText(Corner, FLinearColor(0.85f, 0.85f, 0.85f), 16.0f, 16.0f, Font);

    if (Tools && Tools->IsHelpVisible())
    {
        TArray<FString> Lines;
        ULogBuildToolsComponent::GetHelpText().ParseIntoArrayLines(Lines);
        float Y = 40.0f;
        for (const FString& Line : Lines)
        {
            DrawText(Line, FLinearColor::White, 16.0f, Y, Font);
            Y += 16.0f;
        }
    }
}
