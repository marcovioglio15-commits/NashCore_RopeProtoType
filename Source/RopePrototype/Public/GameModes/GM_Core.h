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
#pragma region UProperties
    // Summary: HUD class that can be set in the editor for this game mode.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
    TSubclassOf<AHUD> HUDClassOverride;
#pragma endregion UProperties

#pragma region Methods
    // Summary: Sets default pawn to rope prototype character.
    AGM_Core();
#pragma endregion Methods
};
