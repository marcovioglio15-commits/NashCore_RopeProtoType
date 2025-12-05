// Summary: Minimal game mode assigning the core player character and enabling level timer completion hooks.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_Core.generated.h"

UCLASS()
class AGM_Core : public AGameModeBase
{
    GENERATED_BODY()

public:
#pragma region Methods
    // Summary: Sets default pawn to rope prototype character.
    AGM_Core();
#pragma endregion Methods
};
