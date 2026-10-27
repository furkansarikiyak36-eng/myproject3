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
        if (const ALogBuildPiece* Log = Carry->GetAimedLooseLog())
        {
            const ELogCutAction Action = Carry->GetSelectedCutAction();
            const bool bAllowed = Log->CanApplyCutAction(Action);
            const FLinearColor Colour = bAllowed ? AllowedCutColor : BlockedCutColor;

            TArray<FVector> Points;
            if (Carry->BuildCutPreviewSegments(Log, Action, Points))
            {
                for (int32 Index = 0; Index + 1 < Points.Num(); Index += 2)
                {
                    const FVector A = Project(Points[Index], false);
                    const FVector B = Project(Points[Index + 1], false);
                    if (A.Z > 0.0f && B.Z > 0.0f)
                        DrawLine(A.X, A.Y, B.X, B.Y, Colour, 2.0f);
                }
            }

            const FString ActionName = StaticEnum<ELogCutAction>()->GetDisplayNameTextByValue(
                static_cast<int64>(Action)).ToString();
            const FString Label = bAllowed
                ? FString::Printf(TEXT("%s  [F: cut, C: change]"), *ActionName)
                : FString::Printf(TEXT("%s - not possible on this piece  [C: change]"), *ActionName);
            float W = 0.0f, H = 0.0f;
            GetTextSize(Label, W, H, Font);
            DrawText(Label, Colour, CentreX - W * 0.5f, TextY, Font);
            TextY += H + 4.0f;
        }
    }

    const FString Status = Carry->GetPlacementStatus();
    if (!Status.IsEmpty() && (Carry->IsCarryingLog() || Carry->IsHoldingMaterialPiece() || !Status.StartsWith(TEXT("Aim"))))
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
    if (const UEnum* ModeEnum = StaticEnum<ELogPlacementMode>())
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
