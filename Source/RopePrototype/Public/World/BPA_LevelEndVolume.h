// Summary: Trigger volume marking the end of the level, locks controls and stops timer.
#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "BPA_LevelEndVolume.generated.h"

class ABPA_PlayerCharacter;

UCLASS()
class ABPA_LevelEndVolume : public ATriggerBox
{
    GENERATED_BODY()

public:
#pragma region Methods
    // Summary: Binds overlap event for completion detection.
    ABPA_LevelEndVolume();

    // Summary: Registers overlap delegate.
    virtual void BeginPlay() override;

protected:
#pragma region State
    // Summary: Ensures completion triggers once.
    bool bAlreadyTriggered;
#pragma endregion State

private:
    // Summary: Handles overlap with player to end level.
    UFUNCTION()
    void HandleOverlap(AActor* OverlappedActor, AActor* OtherActor);
#pragma endregion Methods
};
