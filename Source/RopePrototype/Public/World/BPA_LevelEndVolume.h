// Summary: Goal volume that fades to black and returns the player to the main menu.
#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "TimerManager.h"
#include "BPA_LevelEndVolume.generated.h"

class ABPA_PlayerCharacter;

UCLASS()
class ABPA_LevelEndVolume : public ATriggerBox
{
    GENERATED_BODY()

public:
#pragma region Methods
#pragma region Lifecycle
    // Summary: Sets sane defaults for transition timing and target map.
    ABPA_LevelEndVolume();

    // Summary: Registers overlap delegate.
    virtual void BeginPlay() override;
#pragma endregion Lifecycle

private:
#pragma region Overlap
    // Summary: Handles overlap with player to begin exit sequence.
    UFUNCTION()
    void HandleOverlap(AActor* OverlappedActor, AActor* OtherActor);
#pragma endregion Overlap

#pragma region Transition
    // Summary: Kicks off fade and schedules the main menu load.
    void StartTransition(ABPA_PlayerCharacter& PlayerCharacter);

    // Summary: Loads the configured main menu level after the fade.
    void LoadMainMenu();
#pragma endregion Transition
#pragma endregion Methods

#pragma region Members
#pragma region Config
    // Summary: Name of the level to load once the finish volume is triggered.
    UPROPERTY(EditDefaultsOnly, Category="Level End", meta=(Tooltip="Map to load after the player reaches the goal volume"))
    FName MainMenuLevelName;

    // Summary: Delay used to let the fade reach black before opening the menu.
    UPROPERTY(EditDefaultsOnly, Category="Level End", meta=(Tooltip="Seconds to wait before loading the menu after triggering the goal"))
    float MenuTransitionDelay;
#pragma endregion Config

#pragma region State
    // Summary: Ensures completion triggers once.
    bool bAlreadyTriggered;

    // Summary: Timer used to defer the level load until after fade.
    FTimerHandle MenuTransitionHandle;
#pragma endregion State
#pragma endregion Members
};
