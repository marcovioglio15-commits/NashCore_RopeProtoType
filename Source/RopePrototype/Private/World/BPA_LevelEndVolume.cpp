// Summary: Implements goal volume logic that fades out and returns to the main menu.
#include "World/BPA_LevelEndVolume.h"

#include "Characters/BPA_PlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#pragma region Methods
#pragma region Lifecycle
ABPA_LevelEndVolume::ABPA_LevelEndVolume()
{
    // Default to the main menu map and a one-second fade window.
    MainMenuLevelName = FName(TEXT("MainMenu"));
    MenuTransitionDelay = 1.0f;
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
    // Ignore repeat overlaps or invalid actors.
    if (bAlreadyTriggered || OverlappedActor != this || OtherActor == nullptr)
    {
        return;
    }

    ABPA_PlayerCharacter* const PlayerCharacter = Cast<ABPA_PlayerCharacter>(OtherActor);

    if (PlayerCharacter == nullptr)
    {
        return;
    }

    bAlreadyTriggered = true;
    StartTransition(*PlayerCharacter);
}
#pragma endregion Overlap

#pragma region Transition
void ABPA_LevelEndVolume::StartTransition(ABPA_PlayerCharacter& PlayerCharacter)
{
    // Lock player and match the death fade for a seamless exit.
    PlayerCharacter.CompleteLevel();
    PlayerCharacter.PlayLevelExitFade();

    if (MainMenuLevelName.IsNone())
    {
        return;
    }

    if (UWorld* const World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MenuTransitionHandle);
        World->GetTimerManager().SetTimer(MenuTransitionHandle, this, &ABPA_LevelEndVolume::LoadMainMenu, MenuTransitionDelay, false);
        return;
    }

    UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}

void ABPA_LevelEndVolume::LoadMainMenu()
{
    if (MainMenuLevelName.IsNone())
    {
        return;
    }

    UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
#pragma endregion Transition
#pragma endregion Methods
