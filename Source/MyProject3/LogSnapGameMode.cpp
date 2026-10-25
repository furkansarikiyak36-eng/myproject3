#include "LogSnapGameMode.h"

#include "LogSnapCharacter.h"

ALogSnapGameMode::ALogSnapGameMode()
{
    DefaultPawnClass = ALogSnapCharacter::StaticClass();
}
