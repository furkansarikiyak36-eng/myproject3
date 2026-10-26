#include "LogSnapGameMode.h"

#include "LogSnapCharacter.h"
#include "LogSnapHUD.h"

ALogSnapGameMode::ALogSnapGameMode()
{
    DefaultPawnClass = ALogSnapCharacter::StaticClass();
    HUDClass = ALogSnapHUD::StaticClass();
}
