// Summary: Implements default pawn selection for the core player character.
#include "GameModes/GM_Core.h"

#include "Characters/BPA_PlayerCharacter.h"

#pragma region Methods
AGM_Core::AGM_Core()
{
    // Set the default pawn to the custom player character.
    DefaultPawnClass = ABPA_PlayerCharacter::StaticClass();
}
#pragma endregion Methods
