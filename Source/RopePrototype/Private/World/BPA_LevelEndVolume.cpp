// Summary: Implements level completion trigger logic.
#include "World/BPA_LevelEndVolume.h"

#include "Characters/BPA_PlayerCharacter.h"

#pragma region Methods
#pragma region Lifecycle
ABPA_LevelEndVolume::ABPA_LevelEndVolume()
{
    // Prevent duplicate completion triggers.
    bAlreadyTriggered = false;
}

void ABPA_LevelEndVolume::BeginPlay()
{
    Super::BeginPlay();
    // Bind overlap delegate to detect player finish.
    OnActorBeginOverlap.AddDynamic(this, &ABPA_LevelEndVolume::HandleOverlap);
}
#pragma endregion Lifecycle

#pragma region Overlap
void ABPA_LevelEndVolume::HandleOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
    // Ignore repeat overlaps after completion.
    if (bAlreadyTriggered)
    {
        return;
    }

    // Verify overlapping actor is the player character.
    ABPA_PlayerCharacter* const RopeCharacter = Cast<ABPA_PlayerCharacter>(OtherActor);

    if (RopeCharacter == nullptr)
    {
        return;
    }

    // Mark completion and lock controls.
    bAlreadyTriggered = true;
    RopeCharacter->CompleteLevel();
}
#pragma endregion Overlap
#pragma endregion Methods
