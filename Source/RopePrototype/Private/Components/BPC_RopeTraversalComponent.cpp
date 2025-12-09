// Summary: Implements rope traversal logic including aiming, throwing, hanging, swinging, climbing, and recall.
#include "Components/BPC_RopeTraversalComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"

#pragma region Methods
#pragma region Lifecycle
UBPC_RopeTraversalComponent::UBPC_RopeTraversalComponent()
{
    // Configure ticking to run after physics for rope constraints.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    // Initialize serialized defaults for rope behavior tuning.
    MaxRopeLength = 1200.0f;
    MinRopeLength = 200.0f;
    ThrowSpeed = 2400.0f;
    RecallHoldSeconds = 1.0f;
    RecallRetractSpeed = 2600.0f;
    SwingAcceleration = 600.0f;
    SwingDamping = 0.05f;
    ClimbSpeed = 200.0f;
    LedgeNormalDotThreshold = 0.45f;
    GrabDistance = 200.0f;
    LedgeProbeRadius = 60.0f;

    // Seed runtime state for rope status and timers.
    CurrentRopeLength = MaxRopeLength;
    bRopeAttached = false;
    bHoldingRope = false;
    bHanging = false;
    RopeState = ERopeState::Idle;
    RecallAccumulated = 0.0f;
    ClimbInputSign = 0;
    SavedGravityScale = 1.0f;
    bHasPreview = false;
    bPreviewWithinRange = false;
    PreviewImpactPoint = FVector::ZeroVector;
    PreviewImpactNormal = FVector::ZeroVector;
    RopeFlightElapsed = 0.0f;
    RopeFlightDuration = 0.0f;
    RopeFlightStart = FVector::ZeroVector;
    RopeFlightTarget = FVector::ZeroVector;
    bAimPreviewWhileAttached = false;
}

void UBPC_RopeTraversalComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache owning character for movement and controller access.
    OwningCharacter = Cast<ACharacter>(GetOwner());
    SetComponentTickEnabled(false);
}
#pragma endregion Lifecycle

#pragma region Tick
void UBPC_RopeTraversalComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* const ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Live update aiming preview while RMB held.
    if (RopeState == ERopeState::Aiming)
    {
        UpdateAimPreview();
        return;
    }

    if (RopeState == ERopeState::Airborne)
    {
        TickRopeFlight(DeltaTime);
        return;
    }

    // Process recall hold timer while player is pulling rope back.
    if (RopeState == ERopeState::Recalling)
    {
        RecallAccumulated += DeltaTime;
        CurrentRopeLength = FMath::Max(CurrentRopeLength - RecallRetractSpeed * DeltaTime, 0.0f);

        // Clear rope if recall timer completes.
        if (RecallAccumulated >= RecallHoldSeconds || CurrentRopeLength <= KINDA_SMALL_NUMBER)
        {
            ClearRope();
        }

        return;
    }

    // Simulate hanging swing and climb only while in hanging state.
    if (RopeState == ERopeState::Hanging)
    {
        TickHanging(DeltaTime);
        return;
    }

    // Maintain ground tether when holding rope on foot.
    if (RopeState == ERopeState::Attached && bHoldingRope)
    {
        TickTether(DeltaTime);
    }
}
#pragma endregion Tick

#pragma region Aim And Throw
void UBPC_RopeTraversalComponent::StartAim()
{
    // Enter aiming mode and enable ticking for preview.
    if (bRopeAttached)
    {
        bAimPreviewWhileAttached = true;
        bHasPreview = true;
        bPreviewWithinRange = false;
        PreviewImpactPoint = AnchorLocation;
        PreviewImpactNormal = AnchorNormal;
        SetComponentTickEnabled(true);
        return;
    }

    bAimPreviewWhileAttached = false;
    bHasPreview = false;
    bPreviewWithinRange = false;
    PreviewImpactPoint = FVector::ZeroVector;
    PreviewImpactNormal = FVector::ZeroVector;
    RopeState = ERopeState::Aiming;
    SetComponentTickEnabled(true);
}

void UBPC_RopeTraversalComponent::StopAim()
{
    // Return to idle or attached based on rope anchor state.
    bAimPreviewWhileAttached = false;

    if (RopeState == ERopeState::Aiming)
    {
        RopeState = bRopeAttached ? ERopeState::Attached : ERopeState::Idle;
    }

    // Keep tick alive only if hanging requires simulation.
    if (RopeState == ERopeState::Idle || RopeState == ERopeState::Attached)
    {
        const bool bNeedsTick = bHanging || bHoldingRope;
        SetComponentTickEnabled(bNeedsTick);
    }
}

void UBPC_RopeTraversalComponent::ThrowRope()
{
    // Guard against missing owner.
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    // Block throwing a new rope while one is already attached.
    if (bRopeAttached)
    {
        return;
    }

    const bool bHadPreview = bHasPreview;
    const FVector SavedPreviewPoint = PreviewImpactPoint;
    const FVector SavedPreviewNormal = PreviewImpactNormal;
    const bool bSavedPreviewRange = bPreviewWithinRange;

    if (bRopeAttached || bHoldingRope || bHanging)
    {
        ClearRope();

        if (bHadPreview)
        {
            bHasPreview = true;
            PreviewImpactPoint = SavedPreviewPoint;
            PreviewImpactNormal = SavedPreviewNormal;
            bPreviewWithinRange = bSavedPreviewRange;
            RopeState = ERopeState::Aiming;
            SetComponentTickEnabled(true);
        }
    }

    if (bHanging)
        ExitHanging();

    bRopeAttached = false;
    bHoldingRope = false;
    bHanging = false;

    if (!bHasPreview)
    {
        RopeState = RopeState == ERopeState::Aiming ? ERopeState::Aiming : ERopeState::Idle;
        bRopeAttached = false;
        return;
    }

    RopeFlightStart = OwningCharacter->GetActorLocation();
    RopeFlightTarget = PreviewImpactPoint;
    const float Distance = FVector::Distance(RopeFlightStart, RopeFlightTarget);
    RopeFlightDuration = Distance > KINDA_SMALL_NUMBER ? Distance / ThrowSpeed : 0.0f;

    if (RopeFlightDuration <= KINDA_SMALL_NUMBER)
    {
        AnchorLocation = RopeFlightTarget;
        AnchorNormal = PreviewImpactNormal;
        CurrentRopeLength = FMath::Clamp(Distance, MinRopeLength, MaxRopeLength);
        bRopeAttached = true;
        bHoldingRope = Distance <= MaxRopeLength;
        if (bHoldingRope)
            EngageHoldConstraint();
        else
            RopeState = ERopeState::Attached;
        return;
    }

    RopeFlightElapsed = 0.0f;
    RopeState = ERopeState::Airborne;
    SetComponentTickEnabled(true);
}
#pragma endregion Aim And Throw

#pragma region Hold And Recall
void UBPC_RopeTraversalComponent::ToggleHoldRequest()
{
    // Ignore when no rope anchor exists.
    if (!bRopeAttached)
    {
        return;
    }

    // Drop rope if currently holding.
    if (bHoldingRope)
    {
        ReleaseRope(false);
        BeginRecall();
        return;
    }

    // Guard against missing owner when trying to grab.
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    // Allow grab if player is close enough to loose end.
    const float Distance = FVector::Distance(OwningCharacter->GetActorLocation(), AnchorLocation);

    if (Distance <= GrabDistance)
    {
        bHoldingRope = true;
        EngageHoldConstraint();
    }
}

void UBPC_RopeTraversalComponent::BeginRecall()
{
    // Only recall when rope exists in world.
    if (!bRopeAttached)
    {
        return;
    }

    // Reset timer and enter recall state to monitor hold duration.
    RecallAccumulated = 0.0f;
    bHoldingRope = false;
    if (bHanging)
    {
        ExitHanging();
    }
    RopeState = ERopeState::Recalling;
    SetComponentTickEnabled(true);
}

void UBPC_RopeTraversalComponent::CancelRecall()
{
    // Restore previous state if recall is aborted.
    if (RopeState == ERopeState::Recalling)
    {
        RopeState = bHanging ? ERopeState::Hanging : ERopeState::Attached;
        RecallAccumulated = 0.0f;
        CurrentRopeLength = FMath::Max(CurrentRopeLength, MinRopeLength);
    }

    // Disable tick when nothing requires simulation.
    if (!bHanging && !bHoldingRope)
    {
        SetComponentTickEnabled(false);
    }
}
#pragma endregion Hold And Recall

#pragma region Input Helpers
void UBPC_RopeTraversalComponent::ApplySwingInput(const FVector2D InputAxis)
{
    // Cache swing input to apply during hanging tick.
    PendingSwingInput = InputAxis;
}

void UBPC_RopeTraversalComponent::BeginClimbUp()
{
    // Register upward climb input only while hanging.
    if (!bHanging && bRopeAttached)
    {
        if (OwningCharacter.IsValid())
        {
            if (UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement())
            {
                // Force a hop off the ground before entering hanging to avoid sliding poses.
                if (MoveComp->IsMovingOnGround())
                {
                    OwningCharacter->Jump();
                    MoveComp->SetMovementMode(MOVE_Falling);
                }
            }
        }

        EnterHanging();
    }

    if (CanProcessClimbInput())
        ClimbInputSign = 1;
}

void UBPC_RopeTraversalComponent::BeginClimbDown()
{
    // Register downward climb input only while hanging.
    if (!bHanging && bRopeAttached)
    {
        EnterHanging();
    }

    if (CanProcessClimbInput())
        ClimbInputSign = -1;
}

void UBPC_RopeTraversalComponent::StopClimb()
{
    // Clear climb input when key released.
    ClimbInputSign = 0;
}
#pragma endregion Input Helpers

#pragma region Release And Query
void UBPC_RopeTraversalComponent::ReleaseRope(const bool bJumpRelease)
{
    // Reset when owner is missing.
    if (!OwningCharacter.IsValid())
    {
        ClearRope();
        return;
    }

    // If hanging, restore movement and apply jump impulse if requested.
    if (bHanging)
    {
        ExitHanging();

        if (bJumpRelease)
        {
            UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

            if (MoveComp != nullptr)
            {
                const FVector LaunchDir = MoveComp->Velocity.GetSafeNormal();
                const FVector LaunchStrength = LaunchDir * 200.0f + OwningCharacter->GetActorForwardVector() * 200.0f;
                OwningCharacter->LaunchCharacter(LaunchStrength, true, true);
            }
        }
    }

    bHoldingRope = false;
    RopeState = bRopeAttached ? ERopeState::Attached : ERopeState::Idle;

    if (!bHanging && (RopeState == ERopeState::Idle || RopeState == ERopeState::Attached))
        SetComponentTickEnabled(false);
}

bool UBPC_RopeTraversalComponent::IsAttached() const
{
    // Report if rope end is anchored.
    return bRopeAttached;
}

bool UBPC_RopeTraversalComponent::IsHanging() const
{
    // Report if character is currently hanging.
    return bHanging;
}

float UBPC_RopeTraversalComponent::GetRecallAlpha() const
{
    if (RecallHoldSeconds <= 0.0f)
    {
        return 1.0f;
    }

    // Normalized recall progress for UI feedback.
    return FMath::Clamp(RecallAccumulated / RecallHoldSeconds, 0.0f, 1.0f);
}

void UBPC_RopeTraversalComponent::ForceReset()
{
    // External reset helper (death/respawn).
    ClearRope();
}

FVector UBPC_RopeTraversalComponent::GetAnchorLocation() const
{
    // Provide anchor location for debug purposes.
    return AnchorLocation;
}

bool UBPC_RopeTraversalComponent::HasValidPreview() const
{
    return bHasPreview;
}

FVector UBPC_RopeTraversalComponent::GetPreviewLocation() const
{
    return PreviewImpactPoint;
}

bool UBPC_RopeTraversalComponent::IsPreviewWithinRange() const
{
    return bPreviewWithinRange;
}

bool UBPC_RopeTraversalComponent::IsRecalling() const
{
    return RopeState == ERopeState::Recalling;
}

bool UBPC_RopeTraversalComponent::IsRopeInFlight() const
{
    return RopeState == ERopeState::Airborne;
}

float UBPC_RopeTraversalComponent::GetCurrentRopeLength() const
{
    return CurrentRopeLength;
}

bool UBPC_RopeTraversalComponent::RequestLedgeClimbFromJump()
{
    // Jump-triggered ledge climb only fires when hanging near the anchor.
    if (!CanProcessClimbInput() || CurrentRopeLength > MinRopeLength + 8.0f)
    {
        return false;
    }

    return TryClimbToLedge();
}
#pragma endregion Release And Query

#pragma region Helpers
void UBPC_RopeTraversalComponent::UpdateAimPreview()
{
    // Skip when no owner exists.
    if (!OwningCharacter.IsValid())
    {
        bHasPreview = false;
        return;
    }

    // Read camera viewpoint to align aim trace.
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;

    if (APlayerController* const PC = Cast<APlayerController>(OwningCharacter->GetController()))
    {
        PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        ViewLocation = OwningCharacter->GetActorLocation();
        ViewRotation = OwningCharacter->GetActorRotation();
    }

    // Perform line trace for preview impact point.
    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + ViewRotation.Vector() * MaxRopeLength;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwningCharacter.Get());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

    // Exit if nothing hit to preview.
    if (!bHit)
    {
        bHasPreview = false;
        bPreviewWithinRange = false;
        return;
    }

    bHasPreview = true;
    PreviewImpactPoint = HitResult.ImpactPoint;
    PreviewImpactNormal = HitResult.ImpactNormal;
    bPreviewWithinRange = FVector::Distance(OwningCharacter->GetActorLocation(), HitResult.ImpactPoint) <= MaxRopeLength;
}

void UBPC_RopeTraversalComponent::TickRopeFlight(const float DeltaTime)
{
    RopeFlightElapsed += DeltaTime;
    const float Alpha = RopeFlightDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(RopeFlightElapsed / RopeFlightDuration, 0.0f, 1.0f) : 1.0f;
    const FVector FlatPosition = FMath::Lerp(RopeFlightStart, RopeFlightTarget, Alpha);
    const float Distance = FVector::Distance(RopeFlightStart, RopeFlightTarget);
    const float ArcHeight = FMath::Clamp(Distance * 0.25f, 120.0f, 600.0f);
    const float VerticalOffset = FMath::Sin(Alpha * PI) * ArcHeight;
    AnchorLocation = FlatPosition + FVector::UpVector * VerticalOffset;

    if (Alpha >= 1.0f - KINDA_SMALL_NUMBER)
        CompleteRopeFlight();
}

void UBPC_RopeTraversalComponent::CompleteRopeFlight()
{
    RopeFlightElapsed = 0.0f;
    RopeState = ERopeState::Attached;
    AnchorLocation = RopeFlightTarget;
    AnchorNormal = PreviewImpactNormal;
    CurrentRopeLength = FMath::Clamp(FVector::Distance(OwningCharacter.IsValid() ? OwningCharacter->GetActorLocation() : RopeFlightStart, AnchorLocation), MinRopeLength, MaxRopeLength);
    bRopeAttached = true;
    bHoldingRope = bPreviewWithinRange;
    if (bHoldingRope)
        EngageHoldConstraint();
    else
        SetComponentTickEnabled(false);
}

void UBPC_RopeTraversalComponent::EnterHanging()
{
    // Ensure owner is valid before changing movement.
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    // Avoid duplicate transitions.
    if (bHanging)
    {
        return;
    }

    // Switch to falling while keeping gravity for natural rope tension.
    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp == nullptr)
    {
        return;
    }

    SavedGravityScale = MoveComp->GravityScale;
    MoveComp->SetMovementMode(MOVE_Falling);
    MoveComp->GravityScale = SavedGravityScale;
    const FVector AnchorToActor = OwningCharacter->GetActorLocation() - AnchorLocation;
    const float Distance = AnchorToActor.Size();
    if (Distance > KINDA_SMALL_NUMBER)
    {
        const FVector RopeDir = AnchorToActor / Distance;
        const float RadialSpeed = FVector::DotProduct(MoveComp->Velocity, RopeDir);
        MoveComp->Velocity -= RopeDir * RadialSpeed;
    }
    bHanging = true;
    RopeState = ERopeState::Hanging;
    SetComponentTickEnabled(true);
}

void UBPC_RopeTraversalComponent::ExitHanging()
{
    // Restore movement mode and gravity when leaving rope hang.
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp != nullptr)
    {
        MoveComp->GravityScale = SavedGravityScale;
        MoveComp->SetMovementMode(MoveComp->IsMovingOnGround() ? MOVE_Walking : MOVE_Falling);
    }

    bHanging = false;
    ClimbInputSign = 0;
    PendingSwingInput = FVector2D::ZeroVector;

    // Disable tick if rope no longer needs simulation.
    if (!bRopeAttached)
    {
        SetComponentTickEnabled(false);
    }
}

void UBPC_RopeTraversalComponent::TickHanging(const float DeltaTime)
{
    // Abort and clear rope if owner is gone.
    if (!OwningCharacter.IsValid())
    {
        ClearRope();
        return;
    }

    // Require movement component for positional updates.
    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp == nullptr)
    {
        ClearRope();
        return;
    }

    if (MoveComp->IsMovingOnGround() && MoveComp->CurrentFloor.bBlockingHit && MoveComp->CurrentFloor.HitResult.ImpactNormal.Z >= 0.85f)
    {
        ExitHanging();
        RopeState = bRopeAttached ? ERopeState::Attached : ERopeState::Idle;
        return;
    }

    const FVector ActorLocation = OwningCharacter->GetActorLocation();
    FVector RopeVector = ActorLocation - AnchorLocation;
    const float Distance = RopeVector.Size();

    // Avoid division by zero when extremely close.
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Update current rope length based on climb input.
    ApplyClimbLengthChange(DeltaTime);

    const FVector RopeDir = RopeVector / Distance;
    // Build tangential acceleration from cached swing input relative to rope.
    FVector TangentAccel = OwningCharacter->GetActorForwardVector() * PendingSwingInput.Y + OwningCharacter->GetActorRightVector() * PendingSwingInput.X;
    TangentAccel = TangentAccel - FVector::DotProduct(TangentAccel, RopeDir) * RopeDir;

    const FVector Gravity = FVector(0.0f, 0.0f, GetWorld()->GetGravityZ() * MoveComp->GravityScale);
    const FVector TangentGravity = Gravity - FVector::DotProduct(Gravity, RopeDir) * RopeDir;

    // Apply tangential swing input and gravity while keeping rope length.
    MoveComp->Velocity += (TangentAccel * SwingAcceleration + TangentGravity) * DeltaTime;
    const float RadialSpeed = FVector::DotProduct(MoveComp->Velocity, RopeDir);
    MoveComp->Velocity -= RopeDir * RadialSpeed;

    const float DampingScale = PendingSwingInput.IsNearlyZero() ? SwingDamping * 2.0f : SwingDamping;
    MoveComp->Velocity *= FMath::Clamp(1.0f - DampingScale * DeltaTime, 0.0f, 1.0f);

    // Place character at constrained position along rope with collision support.
    const FVector TargetLocation = AnchorLocation + RopeDir * CurrentRopeLength;
    const FVector Delta = TargetLocation - ActorLocation;

    if (MoveComp->UpdatedComponent != nullptr)
    {
        FHitResult Hit;
        MoveComp->SafeMoveUpdatedComponent(Delta, OwningCharacter->GetActorRotation(), true, Hit);
    }
    else
    {
        OwningCharacter->SetActorLocation(TargetLocation, false);
    }

    // Snap to walking as soon as ground contact occurs while climbing down.
    if (MoveComp->CurrentFloor.bBlockingHit && MoveComp->CurrentFloor.HitResult.ImpactNormal.Z >= 0.85f)
    {
        ExitHanging();
        RopeState = bRopeAttached ? ERopeState::Attached : ERopeState::Idle;

        if (UCharacterMovementComponent* const PostMoveComp = OwningCharacter->GetCharacterMovement())
        {
            PostMoveComp->SetMovementMode(MOVE_Walking);
        }

        return;
    }

    // Clear swing input after applying.
    PendingSwingInput = FVector2D::ZeroVector;

    // Attempt ledge climb when ascending near the anchor.
    if (ClimbInputSign > 0 && CurrentRopeLength <= MinRopeLength + 10.0f)
    {
        TryClimbToLedge();
    }
}

void UBPC_RopeTraversalComponent::TickTether(const float DeltaTime)
{
    if (!OwningCharacter.IsValid())
    {
        ClearRope();
        return;
    }

    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp == nullptr)
    {
        ClearRope();
        return;
    }

    CurrentRopeLength = FMath::Clamp(CurrentRopeLength, MinRopeLength, MaxRopeLength);

    const FVector ActorLocation = OwningCharacter->GetActorLocation();
    const FVector RopeVector = ActorLocation - AnchorLocation;
    const float Distance = RopeVector.Size();

    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector RopeDir = RopeVector / Distance;
    const bool bBeyondLength = Distance > CurrentRopeLength;

    if (bBeyondLength)
    {
        const FVector TargetLocation = AnchorLocation + RopeDir * CurrentRopeLength;
        OwningCharacter->SetActorLocation(TargetLocation, false);
        const FVector OutwardVelocity = FVector::DotProduct(MoveComp->Velocity, RopeDir) * RopeDir;
        const float DampingAlpha = FMath::Clamp(1.0f - SwingDamping * DeltaTime, 0.0f, 1.0f);
        MoveComp->Velocity = (MoveComp->Velocity - OutwardVelocity) * DampingAlpha;
    }

    const float EffectiveDistance = bBeyondLength ? CurrentRopeLength : Distance;
    const bool bTensioned = EffectiveDistance >= CurrentRopeLength - 1.5f;

    if (bTensioned && !MoveComp->IsMovingOnGround())
    {
        EnterHanging();
    }
}

void UBPC_RopeTraversalComponent::EngageHoldConstraint()
{
    if (!OwningCharacter.IsValid() || !bRopeAttached)
    {
        return;
    }

    bHoldingRope = true;
    RopeState = ERopeState::Attached;

    const float Distance = FVector::Distance(OwningCharacter->GetActorLocation(), AnchorLocation);
    CurrentRopeLength = FMath::Clamp(Distance, MinRopeLength, MaxRopeLength);

    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp != nullptr && !MoveComp->IsMovingOnGround() && Distance >= CurrentRopeLength - 1.0f)
    {
        EnterHanging();
        return;
    }

    SetComponentTickEnabled(true);
}

bool UBPC_RopeTraversalComponent::CanProcessClimbInput() const
{
    // Climb input is only relevant while the rope is attached and the player is engaged with it.
    return bRopeAttached && (bHanging || bHoldingRope);
}

void UBPC_RopeTraversalComponent::ApplyClimbLengthChange(const float DeltaTime)
{
    if (!CanProcessClimbInput() || ClimbInputSign == 0)
    {
        return;
    }

    const bool bClimbingDown = ClimbInputSign < 0;
    const bool bAtMaxExtension = CurrentRopeLength >= MaxRopeLength - 0.5f;

    if (bClimbingDown && bAtMaxExtension)
    {
        ClimbInputSign = 0;
        CurrentRopeLength = MaxRopeLength;
        return;
    }

    const float TargetLength = CurrentRopeLength - ClimbInputSign * ClimbSpeed * DeltaTime;
    CurrentRopeLength = FMath::Clamp(TargetLength, MinRopeLength, MaxRopeLength);

    if (bClimbingDown && CurrentRopeLength >= MaxRopeLength - 0.5f)
    {
        CurrentRopeLength = MaxRopeLength;
        ClimbInputSign = 0;
    }
}

bool UBPC_RopeTraversalComponent::TryClimbToLedge()
{
    // Do nothing without a valid character.
    if (!OwningCharacter.IsValid() || !bRopeAttached || !bHanging)
    {
        return false;
    }

    const bool bNearAnchor = CurrentRopeLength <= MinRopeLength + 8.0f;

    if (!bNearAnchor)
    {
        return false;
    }

    // Sweep upward near anchor normal to find a landing ledge.
    const FVector ProbeStart = AnchorLocation + AnchorNormal * LedgeProbeRadius + FVector::UpVector * 20.0f;
    const FVector ProbeEnd = ProbeStart - FVector::UpVector * 200.0f;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwningCharacter.Get());

    const bool bHit = GetWorld()->SweepSingleByChannel(HitResult, ProbeStart, ProbeEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(LedgeProbeRadius), Params);

    const float CapsuleHalfHeight = OwningCharacter->GetSimpleCollisionHalfHeight();
    const FVector FallbackTarget = AnchorLocation + FVector::UpVector * CapsuleHalfHeight;
    FVector TargetLocation = FallbackTarget;

    if (bHit)
    {
        const float NormalDot = FVector::DotProduct(HitResult.ImpactNormal, AnchorNormal);

        if (NormalDot >= LedgeNormalDotThreshold)
        {
            TargetLocation = HitResult.ImpactPoint + FVector::UpVector * CapsuleHalfHeight + AnchorNormal * 18.0f;
        }
    }

    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp != nullptr && MoveComp->UpdatedComponent != nullptr)
    {
        FHitResult MoveHit;
        const FVector Delta = TargetLocation - OwningCharacter->GetActorLocation();
        MoveComp->SafeMoveUpdatedComponent(Delta, OwningCharacter->GetActorRotation(), true, MoveHit);
        MoveComp->SetMovementMode(MOVE_Walking);
    }
    else
    {
        OwningCharacter->SetActorLocation(TargetLocation, false);
    }

    ExitHanging();
    RopeState = ERopeState::Attached;
    bHoldingRope = true;
    CurrentRopeLength = FMath::Clamp(FVector::Distance(OwningCharacter->GetActorLocation(), AnchorLocation), MinRopeLength, MaxRopeLength);
    SetComponentTickEnabled(true);

    return true;
}

void UBPC_RopeTraversalComponent::ClearRope()
{
    // Reset all runtime rope flags and timers.
    bRopeAttached = false;
    bHoldingRope = false;
    bHanging = false;
    bAimPreviewWhileAttached = false;
    RopeState = ERopeState::Idle;
    RecallAccumulated = 0.0f;
    ClimbInputSign = 0;
    PendingSwingInput = FVector2D::ZeroVector;
    bHasPreview = false;
    bPreviewWithinRange = false;
    PreviewImpactPoint = FVector::ZeroVector;
    PreviewImpactNormal = FVector::ZeroVector;
    RopeFlightElapsed = 0.0f;
    RopeFlightDuration = 0.0f;
    RopeFlightStart = FVector::ZeroVector;
    RopeFlightTarget = FVector::ZeroVector;
    CurrentRopeLength = MaxRopeLength;
    SetComponentTickEnabled(false);
}
#pragma endregion Helpers
#pragma endregion Methods
