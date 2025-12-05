// Summary: Implements third-person character with inertia-based movement, rope interactions, fall safety, and timer tracking.
#include "Characters/BPA_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BPC_RopeTraversalComponent.h"
#include "TimerManager.h"

#pragma region Methods
#pragma region Lifecycle
ABPA_PlayerCharacter::ABPA_PlayerCharacter()
{
    // Enable ticking for continuous camera and rope updates.
    PrimaryActorTick.bCanEverTick = true;

    // Create camera boom attached to root to manage orbit distance.
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    // Create follow camera on boom socket to inherit rotation.
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // Create rope traversal component to handle rope mechanics.
    RopeComponent = CreateDefaultSubobject<UBPC_RopeTraversalComponent>(TEXT("RopeComponent"));

    // Configure movement tuning for inertia and jump scaling.
    MaxWalkSpeed = 800.0f;
    MovementAcceleration = 2400.0f;
    MovementDeceleration = 1200.0f;
    JumpSpeedInfluence = 0.35f;
    BaseJumpZ = 420.0f;
    DefaultArmLength = 400.0f;
    AimArmLength = 320.0f;
    CameraInterpSpeed = 6.0f;
    FatalFallHeight = 1200.0f;
    RespawnDelay = 1.75f;
    DeathFadeSeconds = 1.0f;
    FallShakeRampSeconds = 0.65f;
    TimerTickRate = 0.05f;

    bUseControllerRotationYaw = false;
    bIsAiming = false;
    bTrackingFall = false;
    FallStartZ = 0.0f;
    FallOverThresholdTime = 0.0f;
    LevelTimerSeconds = 0.0f;
    bTimerActive = true;
    CachedForwardInput = 0.0f;
    CachedRightInput = 0.0f;

    // Apply movement defaults to character movement component.
    UCharacterMovementComponent* const MoveComp = GetCharacterMovement();

    if (MoveComp != nullptr)
    {
        MoveComp->MaxWalkSpeed = MaxWalkSpeed;
        MoveComp->MaxAcceleration = MovementAcceleration;
        MoveComp->BrakingDecelerationWalking = MovementDeceleration;
        MoveComp->bOrientRotationToMovement = true;
        MoveComp->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
        MoveComp->JumpZVelocity = BaseJumpZ;
        MoveComp->AirControl = 0.4f;
    }
}

void ABPA_PlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Initialize respawn point at spawn if not set.
    if (RespawnLocation.IsNearlyZero())
    {
        RespawnLocation = GetActorLocation();
    }
}
#pragma endregion Lifecycle

#pragma region Tick
void ABPA_PlayerCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateCamera(DeltaSeconds);
    UpdateRopeSwingInput();
    TickLevelTimer(DeltaSeconds);

    // Track fall duration past fatal threshold for feedback usage.
    if (bTrackingFall)
    {
        const float CurrentFallDistance = FallStartZ - GetActorLocation().Z;

        if (CurrentFallDistance > FatalFallHeight)
        {
            FallOverThresholdTime += DeltaSeconds;
        }
        else
        {
            FallOverThresholdTime = 0.0f;
        }
    }
}
#pragma endregion Tick

#pragma region Input Binding
void ABPA_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* const PlayerInputComponent)
{
    check(PlayerInputComponent);

    // Bind locomotion and camera axes.
    PlayerInputComponent->BindAxis("MoveForward", this, &ABPA_PlayerCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ABPA_PlayerCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ABPA_PlayerCharacter::AddControllerYawInput);
    PlayerInputComponent->BindAxis("LookUp", this, &ABPA_PlayerCharacter::AddControllerPitchInput);

    // Bind jump hold and release.
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ABPA_PlayerCharacter::StartJump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ABPA_PlayerCharacter::StopJump);
    // Bind rope aim, throw, hold, recall, and climb interactions.
    PlayerInputComponent->BindAction("Aim", IE_Pressed, this, &ABPA_PlayerCharacter::BeginAim);
    PlayerInputComponent->BindAction("Aim", IE_Released, this, &ABPA_PlayerCharacter::EndAim);
    PlayerInputComponent->BindAction("ThrowRope", IE_Pressed, this, &ABPA_PlayerCharacter::ThrowRope);
    PlayerInputComponent->BindAction("ToggleHold", IE_Pressed, this, &ABPA_PlayerCharacter::ToggleHold);
    PlayerInputComponent->BindAction("RecallRope", IE_Pressed, this, &ABPA_PlayerCharacter::StartRecall);
    PlayerInputComponent->BindAction("RecallRope", IE_Released, this, &ABPA_PlayerCharacter::StopRecall);
    PlayerInputComponent->BindAction("ClimbUp", IE_Pressed, this, &ABPA_PlayerCharacter::ClimbUp);
    PlayerInputComponent->BindAction("ClimbUp", IE_Released, this, &ABPA_PlayerCharacter::StopClimbInput);
    PlayerInputComponent->BindAction("ClimbDown", IE_Pressed, this, &ABPA_PlayerCharacter::ClimbDown);
    PlayerInputComponent->BindAction("ClimbDown", IE_Released, this, &ABPA_PlayerCharacter::StopClimbInput);
}
#pragma endregion Input Binding

#pragma region Movement And Rotation

void ABPA_PlayerCharacter::OnMovementModeChanged(const EMovementMode PrevMovementMode, const uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    // Begin tracking fall when entering falling mode, otherwise finalize.
    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        if (MoveComp->MovementMode == MOVE_Falling)
        {
            BeginFallTrace();
        }
        else if (bTrackingFall)
        {
            EndFallTrace(GetActorLocation().Z);
        }
    }
    else if (bTrackingFall)
    {
        EndFallTrace(GetActorLocation().Z);
    }
}

void ABPA_PlayerCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    // Capture land height for fatal fall evaluation.
    EndFallTrace(Hit.Location.Z);
}

void ABPA_PlayerCharacter::MoveForward(const float Value)
{
    // Cache forward input for swing usage.
    CachedForwardInput = Value;

    // Skip when no input present.
    if (Value == 0.0f)
    {
        return;
    }

    // Ignore standard movement while hanging on rope.
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
    {
        return;
    }

    // Move character along control rotation forward axis.
    const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::X);
    AddMovementInput(Direction, Value);
}

void ABPA_PlayerCharacter::MoveRight(const float Value)
{
    // Cache right input for swing usage.
    CachedRightInput = Value;

    // Skip when no input present.
    if (Value == 0.0f)
    {
        return;
    }

    // Ignore standard movement while hanging on rope.
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
    {
        return;
    }

    // Move character along control rotation right axis.
    const FVector Direction = FRotationMatrix(GetControlRotation()).GetUnitAxis(EAxis::Y);
    AddMovementInput(Direction, Value);
}
#pragma endregion Movement And Rotation

#pragma region Jump And Aim
void ABPA_PlayerCharacter::StartJump()
{
    // When hanging, releasing rope with jump uses rope release logic.
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
    {
        RopeComponent->ReleaseRope(true);
        return;
    }

    // Scale jump Z based on current horizontal speed to add momentum feel.
    UCharacterMovementComponent* const MoveComp = GetCharacterMovement();

    if (MoveComp != nullptr)
    {
        const float SpeedAlpha = MoveComp->Velocity.Size2D() / MaxWalkSpeed;
        MoveComp->JumpZVelocity = BaseJumpZ + SpeedAlpha * JumpSpeedInfluence * BaseJumpZ;
    }

    Jump();
}

void ABPA_PlayerCharacter::StopJump()
{
    // End jump hold for variable jump height.
    StopJumping();
}

void ABPA_PlayerCharacter::BeginAim()
{
    // Enter aim mode so camera follows controller yaw.
    bIsAiming = true;
    bUseControllerRotationYaw = true;

    // Disable orient-to-movement to prevent auto-rotation while aiming.
    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = false;
    }

    // Notify rope component to start preview.
    if (RopeComponent != nullptr)
    {
        RopeComponent->StartAim();
    }
}

void ABPA_PlayerCharacter::EndAim()
{
    // Exit aim mode restoring movement-driven rotation.
    bIsAiming = false;
    bUseControllerRotationYaw = false;

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        MoveComp->bOrientRotationToMovement = true;
    }

    // Notify rope component to stop preview.
    if (RopeComponent != nullptr)
    {
        RopeComponent->StopAim();
    }
}
#pragma endregion Jump And Aim

#pragma region Rope Actions
void ABPA_PlayerCharacter::ThrowRope()
{
    // Forward throw request to rope component.
    if (RopeComponent != nullptr)
    {
        RopeComponent->ThrowRope();
    }
}

void ABPA_PlayerCharacter::ToggleHold()
{
    // Toggle rope hold state or grab if near.
    if (RopeComponent != nullptr)
    {
        RopeComponent->ToggleHoldRequest();
    }
}

void ABPA_PlayerCharacter::StartRecall()
{
    // Start recalling rope when button is held.
    if (RopeComponent != nullptr)
    {
        RopeComponent->BeginRecall();
    }
}

void ABPA_PlayerCharacter::StopRecall()
{
    // Cancel recall when button is released.
    if (RopeComponent != nullptr)
    {
        RopeComponent->CancelRecall();
    }
}

void ABPA_PlayerCharacter::ClimbUp()
{
    // Apply upward climb input to rope.
    if (RopeComponent != nullptr)
    {
        RopeComponent->BeginClimbUp();
    }
}

void ABPA_PlayerCharacter::ClimbDown()
{
    // Apply downward climb input to rope.
    if (RopeComponent != nullptr)
    {
        RopeComponent->BeginClimbDown();
    }
}

void ABPA_PlayerCharacter::StopClimbInput()
{
    // Clear climb input when key released.
    if (RopeComponent != nullptr)
    {
        RopeComponent->StopClimb();
    }
}
#pragma endregion Rope Actions

#pragma region Camera
void ABPA_PlayerCharacter::UpdateCamera(const float DeltaSeconds)
{
    // Interpolate boom length toward aim or default distance.
    if (CameraBoom == nullptr)
    {
        return;
    }

    const float TargetArm = bIsAiming ? AimArmLength : DefaultArmLength;
    const float NewArm = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, CameraInterpSpeed);
    CameraBoom->TargetArmLength = NewArm;
}
#pragma endregion Camera

#pragma region Fall Handling
void ABPA_PlayerCharacter::BeginFallTrace()
{
    // Start tracking fall distance once when entering falling.
    if (!bTrackingFall)
    {
        bTrackingFall = true;
        FallStartZ = GetActorLocation().Z;
        FallOverThresholdTime = 0.0f;
    }
}

void ABPA_PlayerCharacter::EndFallTrace(const float LandHeight)
{
    // Stop tracking fall and evaluate death on landing or mode change.
    if (!bTrackingFall)
    {
        return;
    }

    bTrackingFall = false;
    const float FallDistance = FallStartZ - LandHeight;

    if (FallDistance >= FatalFallHeight)
    {
        HandleFatalFall();
    }
    else
    {
        FallOverThresholdTime = 0.0f;
    }
}

void ABPA_PlayerCharacter::HandleFatalFall()
{
    // Disable player input and movement upon fatal fall.
    AController* const OwnerController = GetController();

    if (OwnerController != nullptr)
    {
        OwnerController->DisableInput(nullptr);
    }

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        MoveComp->DisableMovement();
    }

    // Force rope reset to avoid stale states.
    if (RopeComponent != nullptr)
    {
        RopeComponent->ForceReset();
    }

    // Schedule respawn after configured delay.
    FTimerHandle RespawnTimer;
    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ABPA_PlayerCharacter::Respawn, RespawnDelay, false);
}

void ABPA_PlayerCharacter::Respawn()
{
    // Teleport to respawn location and reset rotation.
    SetActorLocation(RespawnLocation, false);
    SetActorRotation(FRotator::ZeroRotator);

    // Re-enable input and movement after respawn.
    if (AController* const OwnerController = GetController())
    {
        OwnerController->EnableInput(nullptr);
    }

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Walking);
    }
}
#pragma endregion Fall Handling

#pragma region Timer And Completion
void ABPA_PlayerCharacter::CompleteLevel()
{
    // Stop timer and lock controls on level completion.
    bTimerActive = false;

    if (AController* const OwnerController = GetController())
    {
        OwnerController->DisableInput(nullptr);
    }
}

void ABPA_PlayerCharacter::UpdateRopeSwingInput()
{
    // Forward cached movement input as swing control when hanging.
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
    {
        RopeComponent->ApplySwingInput(FVector2D(CachedRightInput, CachedForwardInput));
    }
}

void ABPA_PlayerCharacter::TickLevelTimer(const float DeltaSeconds)
{
    // Accumulate level timer only while active.
    if (!bTimerActive)
    {
        return;
    }

    LevelTimerSeconds += DeltaSeconds;
}
#pragma endregion Timer And Completion
#pragma endregion Methods
