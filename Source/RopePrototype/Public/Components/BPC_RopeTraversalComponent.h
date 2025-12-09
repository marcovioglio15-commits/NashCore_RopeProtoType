// Summary: Component handling rope aiming, throw, hang, swing, climb, and recall behavior.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BPC_RopeTraversalComponent.generated.h"

class ACharacter;

UENUM(BlueprintType)
enum class ERopeState : uint8
{
    Idle,
    Aiming,
    Airborne,
    Attached,
    Hanging,
    Recalling
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UBPC_RopeTraversalComponent : public UActorComponent
{
    GENERATED_BODY()

public:
#pragma region Methods
    // Summary: Builds defaults and sets tick off by default.
    UBPC_RopeTraversalComponent();

    // Summary: Initializes owner references.
    virtual void BeginPlay() override;

    // Summary: Tick used only for aiming, hanging, or recall feedback.
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Summary: Starts aim logic and enables preview updates.
    void StartAim();

    // Summary: Stops aim logic and clears preview data.
    void StopAim();

    // Summary: Throws rope toward current aim, attaches if valid.
    void ThrowRope();

    // Summary: Drops or grabs rope when near.
    void ToggleHoldRequest();

    // Summary: Begins rope recall timing.
    void BeginRecall();

    // Summary: Cancels rope recall timing.
    void CancelRecall();

    // Summary: Drives swing acceleration from directional input while hanging.
    void ApplySwingInput(const FVector2D InputAxis);

    // Summary: Begins climb up movement.
    void BeginClimbUp();

    // Summary: Begins climb down movement.
    void BeginClimbDown();

    // Summary: Stops climb input.
    void StopClimb();

    // Summary: Releases rope, optionally applying jump launch.
    void ReleaseRope(const bool bJumpRelease);

    // Summary: Returns whether rope is currently attached.
    bool IsAttached() const;

    // Summary: Returns whether player is hanging and holding.
    bool IsHanging() const;

    // Summary: Gets normalized recall progress.
    float GetRecallAlpha() const;

    // Summary: Updates rope when owning character dies or respawns.
    void ForceReset();

    // Summary: Provides anchor location for debug draw.
    FVector GetAnchorLocation() const;

    // Summary: Returns whether aim preview hit is valid.
    bool HasValidPreview() const;

    // Summary: Returns last aim preview location.
    FVector GetPreviewLocation() const;

    // Summary: Returns whether preview is within rope reach.
    bool IsPreviewWithinRange() const;

    // Summary: Returns whether rope is currently recalling.
    bool IsRecalling() const;

    // Summary: Returns whether rope is mid-flight toward anchor.
    bool IsRopeInFlight() const;

    // Summary: Returns current rope length used for simulation.
    float GetCurrentRopeLength() const;
#pragma endregion Methods

protected:
#pragma region Variables And Properties
#pragma region Serialized Fields
    // Summary: Maximum allowed rope length in cm.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Maximum rope reach in centimeters", AllowPrivateAccess="true"))
    float MaxRopeLength;

    // Summary: Minimum rope length when climbing near anchor.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Shortest rope length allowed while climbing in centimeters", AllowPrivateAccess="true"))
    float MinRopeLength;

    // Summary: Speed at which rope arc travels toward target.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Projectile speed for rope throw in centimeters per second", AllowPrivateAccess="true"))
    float ThrowSpeed;

    // Summary: Time the recall input must be held.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Hold duration in seconds before the rope returns to the player", AllowPrivateAccess="true"))
    float RecallHoldSeconds;

    // Summary: Retraction speed used while recalling rope.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Speed used to retract rope while recalling in centimeters per second", AllowPrivateAccess="true"))
    float RecallRetractSpeed;

    // Summary: Acceleration applied while swinging.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Swing acceleration tangential to the rope in centimeters per second squared", AllowPrivateAccess="true"))
    float SwingAcceleration;

    // Summary: Damping applied to swing to avoid runaway energy.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Friction damping applied to swing velocity each second", AllowPrivateAccess="true"))
    float SwingDamping;

    // Summary: Speed of climbing input along rope.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Climb speed along the rope in centimeters per second", AllowPrivateAccess="true"))
    float ClimbSpeed;

    // Summary: Angle tolerance for ledge climb detection.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Cosine tolerance for detecting valid ledge normals when climbing off the rope", AllowPrivateAccess="true"))
    float LedgeNormalDotThreshold;

    // Summary: Distance required to snap onto held rope.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Maximum distance allowed to grab the rope loose end", AllowPrivateAccess="true"))
    float GrabDistance;

    // Summary: Radius used to sample ledge when near anchor.
    UPROPERTY(EditDefaultsOnly, Category="Rope", meta=(ToolTip="Probe radius for detecting a climbable ledge near the rope anchor", AllowPrivateAccess="true"))
    float LedgeProbeRadius;
#pragma endregion Serialized Fields

#pragma region State
    // Summary: Owning character cached for movement access.
    TWeakObjectPtr<ACharacter> OwningCharacter;

    // Summary: Rope anchor location in world space.
    FVector AnchorLocation;

    // Summary: Rope anchor normal for ledge detection.
    FVector AnchorNormal;

    // Summary: Current rope length clamped during climbing.
    float CurrentRopeLength;

    // Summary: Whether rope end is attached to world.
    bool bRopeAttached;

    // Summary: Whether player is holding rope end.
    bool bHoldingRope;

    // Summary: Whether player is currently hanging.
    bool bHanging;

    // Summary: Swing input cached for this frame.
    FVector2D PendingSwingInput;

    // Summary: Active rope state.
    ERopeState RopeState;

    // Summary: Stopwatch for recall progress.
    float RecallAccumulated;

    // Summary: Climb direction input, 1 for up, -1 for down.
    int32 ClimbInputSign;

    // Summary: Cached gravity scale before entering hanging.
    float SavedGravityScale;

    // Summary: Whether preview trace has a valid hit.
    bool bHasPreview;

    // Summary: Whether preview is within rope reach.
    bool bPreviewWithinRange;

    // Summary: Cached preview impact point.
    FVector PreviewImpactPoint;

    // Summary: Cached preview impact normal.
    FVector PreviewImpactNormal;

    // Summary: Rope flight progress elapsed seconds.
    float RopeFlightElapsed;

    // Summary: Rope flight duration seconds.
    float RopeFlightDuration;

    // Summary: Rope flight start location.
    FVector RopeFlightStart;

    // Summary: Rope flight target location.
    FVector RopeFlightTarget;
#pragma endregion State
#pragma endregion Variables And Properties

private:
#pragma region Methods
#pragma region Helpers
    // Summary: Updates aim trace and preview.
    void UpdateAimPreview();

    // Summary: Advances rope flight arc toward anchor.
    void TickRopeFlight(float DeltaTime);

    // Summary: Finalizes rope flight and attaches.
    void CompleteRopeFlight();

    // Summary: Begins hanging state if allowed.
    void EnterHanging();

    // Summary: Exits hanging and restores walking movement.
    void ExitHanging();

    // Summary: Advances swing simulation and length adjustments.
    void TickHanging(float DeltaTime);

    // Summary: Applies ground tether constraint while holding the rope.
    void TickTether(float DeltaTime);

    // Summary: Attempts to climb ledge near anchor.
    void TryClimbToLedge();

    // Summary: Chooses hang or tether mode when grabbing rope.
    void EngageHoldConstraint();

    // Summary: Clears anchor and resets rope.
    void ClearRope();
#pragma endregion Helpers
#pragma endregion Methods
};
