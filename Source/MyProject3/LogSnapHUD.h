#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LogSnapHUD.generated.h"

/** Crosshair, dashed cut-line preview on the aimed log and placement status. */
UCLASS()
class MYPROJECT3_API ALogSnapHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

    /** Red dashes: the log can be cut here (as in Sons of the Forest). */
    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor AllowedCutColor = FLinearColor(1.0f, 0.18f, 0.12f, 1.0f);

    /** Grey dashes: this cut is not possible on the aimed piece. */
    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor BlockedCutColor = FLinearColor(0.55f, 0.55f, 0.55f, 0.8f);

    /** White dashes: where the carried log will lie, and the arrow into its joint. */
    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor PlacementColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

private:
    void DrawCrosshair(float CentreX, float CentreY);
    void DrawWorldSegments(const TArray<FVector>& Points, const FLinearColor& Colour, float Thickness);
};
