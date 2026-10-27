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

    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor AllowedCutColor = FLinearColor(0.2f, 1.0f, 0.3f, 1.0f);

    UPROPERTY(EditAnywhere, Category = "Log HUD")
    FLinearColor BlockedCutColor = FLinearColor(1.0f, 0.25f, 0.2f, 1.0f);

private:
    void DrawCrosshair(float CentreX, float CentreY);
};
