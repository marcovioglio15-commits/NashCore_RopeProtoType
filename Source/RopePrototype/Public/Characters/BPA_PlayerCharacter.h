// Summary: Character implementing third-person camera, inertia-based locomotion, jump, rope interaction, fall death, and level timer.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BPA_PlayerCharacter.generated.h"

class UBPC_RopeTraversalComponent;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class ABPA_PlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
#pragma region Methods
    // Summary: Sets default component hierarchy and movement defaults.
    ABPA_PlayerCharacter();

    // Summary: Tick handles camera interpolation, fall tracking, and rope prompts.
    virtual void Tick(float DeltaSeconds) override;

    // Summary: Initializes bindings and runtime state.
    virtual void BeginPlay() override;

    // Summary: Binds input axes and actions.
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Summary: Detects movement mode changes for fall tracking.
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

    // Summary: Captures landing to evaluate fatal falls.
    virtual void Landed(const FHitResult& Hit) override;

    // Summary: Stops timer and locks controls when level is complete.
    void CompleteLevel();
#pragma endregion Methods

protected:
#pragma region Variables And Properties
#pragma region Components
    // Summary: Spring arm used to orbit camera around the character.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(ToolTip="Camera boom positioning the follow camera", AllowPrivateAccess="true"))
    USpringArmComponent* CameraBoom;

    // Summary: Player follow camera.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(ToolTip="Third-person follow camera", AllowPrivateAccess="true"))
    UCameraComponent* FollowCamera;

    // Summary: Rope traversal component owning rope mechanics.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope", meta=(ToolTip="Rope traversal component handling aiming, throw, and swing", AllowPrivateAccess="true"))
    UBPC_RopeTraversalComponent* RopeComponent;
#pragma endregion Components

#pragma region Serialized Fields
    // Summary: Max walk speed in centimeters per second.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ToolTip="Target walk speed in centimeters per second", AllowPrivateAccess="true"))
    float MaxWalkSpeed;

    // Summary: Acceleration applied when input is present.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ToolTip="Acceleration for inertia-driven movement", AllowPrivateAccess="true"))
    float MovementAcceleration;

    // Summary: Deceleration applied when no input is present.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ToolTip="Braking deceleration for inertia-driven stop", AllowPrivateAccess="true"))
    float MovementDeceleration;

    // Summary: Scalar applied to jump strength based on current speed.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ToolTip="Multiplier translating horizontal speed into jump boost", AllowPrivateAccess="true"))
    float JumpSpeedInfluence;

    // Summary: Base jump velocity independent from movement.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ToolTip="Base jump Z velocity in centimeters per second", AllowPrivateAccess="true"))
    float BaseJumpZ;

    // Summary: Third-person default camera boom length.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ToolTip="Default camera boom arm length", AllowPrivateAccess="true"))
    float DefaultArmLength;

    // Summary: Aiming camera boom length for zoomed view.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ToolTip="Shorter boom when aiming the rope", AllowPrivateAccess="true"))
    float AimArmLength;

    // Summary: Interp speed when switching camera states.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ToolTip="Interpolation speed for camera boom changes", AllowPrivateAccess="true"))
    float CameraInterpSpeed;

    // Summary: Fatal fall height threshold.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ToolTip="Vertical distance that triggers death when landed", AllowPrivateAccess="true"))
    float FatalFallHeight;

    // Summary: Delay after death before respawn.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ToolTip="Seconds to wait after fatal fall before respawn", AllowPrivateAccess="true"))
    float RespawnDelay;

    // Summary: Duration of screen fade when dying.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ToolTip="Seconds the screen remains black after death", AllowPrivateAccess="true"))
    float DeathFadeSeconds;

    // Summary: Fall time required to reach maximum camera shake.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(ToolTip="Seconds falling beyond fatal height before max shake", AllowPrivateAccess="true"))
    float FallShakeRampSeconds;

    // Summary: Starting location used for respawn.
    UPROPERTY(EditInstanceOnly, Category="Checkpoint", meta=(ToolTip="Spawn point used when resetting the character", AllowPrivateAccess="true"))
    FVector RespawnLocation;

    // Summary: Timer display precision update interval.
    UPROPERTY(EditDefaultsOnly, Category="Timer", meta=(ToolTip="How often to update UI timer in seconds", AllowPrivateAccess="true"))
    float TimerTickRate;
#pragma endregion Serialized Fields

#pragma region State
    // Summary: Tracks whether player is currently aiming.
    bool bIsAiming;

    // Summary: Start height of current fall for death detection.
    float FallStartZ;

    // Summary: Whether we are tracking a fall segment.
    bool bTrackingFall;

    // Summary: Accumulated time spent falling past threshold.
    float FallOverThresholdTime;

    // Summary: Accumulated level timer seconds.
    float LevelTimerSeconds;

    // Summary: Whether timer is running.
    bool bTimerActive;

    // Summary: Cached forward movement input value.
    float CachedForwardInput;

    // Summary: Cached right movement input value.
    float CachedRightInput;
#pragma endregion State
#pragma endregion Variables And Properties

private:
#pragma region Methods
#pragma region Input
    // Summary: Moves character forward and backward with inertia.
    void MoveForward(float Value);

    // Summary: Moves character right and left with inertia.
    void MoveRight(float Value);

    // Summary: Starts jump logic.
    void StartJump();

    // Summary: Stops jump logic.
    void StopJump();

    // Summary: Begins aiming mode.
    void BeginAim();

    // Summary: Ends aiming mode.
    void EndAim();

    // Summary: Throws rope while aiming.
    void ThrowRope();

    // Summary: Toggles rope hold or drop.
    void ToggleHold();

    // Summary: Starts rope recall input.
    void StartRecall();

    // Summary: Stops rope recall input.
    void StopRecall();

    // Summary: Climb input up the rope.
    void ClimbUp();

    // Summary: Climb input down the rope.
    void ClimbDown();

    // Summary: Clears climb input.
    void StopClimbInput();
#pragma endregion Input

#pragma region Helpers
    // Summary: Adjusts camera boom based on aim state.
    void UpdateCamera(float DeltaSeconds);

    // Summary: Captures start height when entering falling.
    void BeginFallTrace();

    // Summary: Ends fall trace and handles potential death.
    void EndFallTrace(const float LandHeight);

    // Summary: Processes fatal fall and schedules respawn.
    void HandleFatalFall();

    // Summary: Respawns character at configured location.
    void Respawn();

    // Summary: Adds rope swing input mapping.
    void UpdateRopeSwingInput();

    // Summary: Ticks and broadcasts timer.
    void TickLevelTimer(float DeltaSeconds);
#pragma endregion Helpers
#pragma endregion Methods
};
