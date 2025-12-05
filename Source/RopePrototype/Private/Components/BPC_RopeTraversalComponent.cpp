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

    // Process recall hold timer while player is pulling rope back.
    if (RopeState == ERopeState::Recalling)
    {
        RecallAccumulated += DeltaTime;

        // Clear rope if recall timer completes.
        if (RecallAccumulated >= RecallHoldSeconds)
        {
            ClearRope();
        }

        return;
    }

    // Simulate hanging swing and climb only while in hanging state.
    if (RopeState == ERopeState::Hanging)
    {
        TickHanging(DeltaTime);
    }
}
#pragma endregion Tick

#pragma region Aim And Throw
void UBPC_RopeTraversalComponent::StartAim()
{
    // Enter aiming mode and enable ticking for preview.
    RopeState = ERopeState::Aiming;
    SetComponentTickEnabled(true);
}

void UBPC_RopeTraversalComponent::StopAim()
{
    // Return to idle or attached based on rope anchor state.
    if (RopeState == ERopeState::Aiming)
    {
        RopeState = bRopeAttached ? ERopeState::Attached : ERopeState::Idle;
    }

    // Keep tick alive only if hanging requires simulation.
    if (RopeState == ERopeState::Idle || RopeState == ERopeState::Attached)
    {
        const bool bNeedsTick = bHanging;
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

    // Gather current view origin and rotation for trace direction.
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

    // Trace forward up to max rope length to find anchor.
    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + ViewRotation.Vector() * MaxRopeLength;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwningCharacter.Get());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

    // Abort if no surface is hit.
    if (!bHit)
    {
        RopeState = ERopeState::Idle;
        bRopeAttached = false;
        return;
    }

    // Store anchor data and clamp rope length to distance.
    AnchorLocation = HitResult.ImpactPoint;
    AnchorNormal = HitResult.ImpactNormal;

    const float DistanceToAnchor = FVector::Distance(OwningCharacter->GetActorLocation(), AnchorLocation);
    CurrentRopeLength = FMath::Clamp(DistanceToAnchor, MinRopeLength, MaxRopeLength);
    bRopeAttached = true;
    bHoldingRope = DistanceToAnchor <= MaxRopeLength;

    // Auto-enter hanging if within rope length, otherwise stay attached.
    if (bHoldingRope)
    {
        EnterHanging();
    }
    else
    {
        RopeState = ERopeState::Attached;
    }
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
        EnterHanging();
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
    }

    // Disable tick when nothing requires simulation.
    if (!bHanging)
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
    // Mark climb input direction upward.
    ClimbInputSign = 1;
}

void UBPC_RopeTraversalComponent::BeginClimbDown()
{
    // Mark climb input direction downward.
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
#pragma endregion Release And Query

#pragma region Helpers
void UBPC_RopeTraversalComponent::UpdateAimPreview()
{
    // Skip when no owner exists.
    if (!OwningCharacter.IsValid())
    {
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
        return;
    }

    // Pick preview color based on reachability.
    const bool bWithinRange = FVector::Distance(OwningCharacter->GetActorLocation(), HitResult.ImpactPoint) <= MaxRopeLength;

    const FColor PreviewColor = bWithinRange ? FColor::Green : FColor::Red;
    DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 8.0f, 12, PreviewColor, false, 0.016f);
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

    // Switch movement to flying with zero gravity to simulate rope hang.
    UCharacterMovementComponent* const MoveComp = OwningCharacter->GetCharacterMovement();

    if (MoveComp == nullptr)
    {
        return;
    }

    SavedGravityScale = MoveComp->GravityScale;
    MoveComp->SetMovementMode(MOVE_Flying);
    MoveComp->GravityScale = 0.0f;
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
        MoveComp->SetMovementMode(MOVE_Falling);
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

    // Compute rope vector to anchor for constraint calculations.
    FVector RopeVector = OwningCharacter->GetActorLocation() - AnchorLocation;
    const float Distance = RopeVector.Size();

    // Avoid division by zero when extremely close.
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FVector RopeDir = RopeVector / Distance;
    // Update current rope length based on climb input.
    CurrentRopeLength = FMath::Clamp(CurrentRopeLength - ClimbInputSign * ClimbSpeed * DeltaTime, MinRopeLength, MaxRopeLength);

    // Build tangential acceleration from cached swing input relative to rope.
    FVector TangentAccel = OwningCharacter->GetActorForwardVector() * PendingSwingInput.Y + OwningCharacter->GetActorRightVector() * PendingSwingInput.X;
    TangentAccel = TangentAccel - FVector::DotProduct(TangentAccel, RopeDir) * RopeDir;

    // Apply swing acceleration and remove radial velocity to keep on rope.
    MoveComp->Velocity += TangentAccel * SwingAcceleration * DeltaTime;
    MoveComp->Velocity -= FVector::DotProduct(MoveComp->Velocity, RopeDir) * RopeDir;
    // Dampen swing to avoid infinite energy gain.
    MoveComp->Velocity *= FMath::Clamp(1.0f - SwingDamping * DeltaTime, 0.0f, 1.0f);

    // Place character at constrained position along rope.
    const FVector TargetLocation = AnchorLocation + RopeDir * CurrentRopeLength;
    OwningCharacter->SetActorLocation(TargetLocation, false);

    // Clear swing input after applying.
    PendingSwingInput = FVector2D::ZeroVector;

    // Attempt ledge climb when near anchor and climbing upward.
    if (ClimbInputSign > 0 && CurrentRopeLength <= MinRopeLength + 5.0f)
    {
        TryClimbToLedge();
    }
}

void UBPC_RopeTraversalComponent::TryClimbToLedge()
{
    // Do nothing without a valid character.
    if (!OwningCharacter.IsValid())
    {
        return;
    }

    // Sweep upward near anchor normal to find a landing ledge.
    const FVector ProbeStart = AnchorLocation + AnchorNormal * LedgeProbeRadius + FVector::UpVector * 20.0f;
    const FVector ProbeEnd = ProbeStart - FVector::UpVector * 200.0f;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwningCharacter.Get());

    const bool bHit = GetWorld()->SweepSingleByChannel(HitResult, ProbeStart, ProbeEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(LedgeProbeRadius), Params);

    // Exit if no ledge is detected.
    if (!bHit)
    {
        return;
    }

    // Reject ledges facing away from anchor normal.
    const float NormalDot = FVector::DotProduct(HitResult.ImpactNormal, AnchorNormal);

    if (NormalDot < LedgeNormalDotThreshold)
    {
        return;
    }

    // Compute placement location using character collision height.
    const float CapsuleHalfHeight = OwningCharacter->GetSimpleCollisionHalfHeight();
    const FVector TargetLocation = HitResult.ImpactPoint + FVector::UpVector * CapsuleHalfHeight;

    // Move character onto ledge and release rope.
    OwningCharacter->SetActorLocation(TargetLocation, false);
    ReleaseRope(false);
}

void UBPC_RopeTraversalComponent::ClearRope()
{
    // Reset all runtime rope flags and timers.
    bRopeAttached = false;
    bHoldingRope = false;
    bHanging = false;
    RopeState = ERopeState::Idle;
    RecallAccumulated = 0.0f;
    ClimbInputSign = 0;
    PendingSwingInput = FVector2D::ZeroVector;
    SetComponentTickEnabled(false);
}
#pragma endregion Helpers
#pragma endregion Methods
