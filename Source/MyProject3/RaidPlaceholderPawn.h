#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RaidPlaceholderPawn.generated.h"

/**
 * Lightweight primitive-only raider placeholder.
 *
 * WorldRaidAdapterSubsystem owns the fixed-step attack logic; this pawn exists
 * only to make the raid visible until a project-specific AI pawn is authored.
 */
UCLASS()
class MYPROJECT3_API ARaidPlaceholderPawn : public APawn
{
    GENERATED_BODY()

public:
    ARaidPlaceholderPawn();
};
