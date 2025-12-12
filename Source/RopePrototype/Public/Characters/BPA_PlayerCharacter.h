
/// Character implementing third-person camera, inertia-based locomotion, jump, rope interaction, fall death, and level timer.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BPA_PlayerCharacter.generated.h"

class UBPC_RopeTraversalComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UInputModifier;
class UInputModifierNegate;
class UInputModifierSwizzleAxis;
class UCableComponent;
class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class USkeletalMesh;
class UUserWidget;
class UCameraShakeBase;
struct FTimerHandle;
enum class EInputAxisSwizzle : uint8;
struct FInputActionValue;

UCLASS()
class ABPA_PlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
#pragma region Methods
    
    /// Sets default component hierarchy and movement defaults.
    ABPA_PlayerCharacter();

    
    /// Tick handles camera interpolation, fall tracking, and rope prompts.
    virtual void Tick(float DeltaSeconds) override;

    
    /// Initializes bindings and runtime state.
    virtual void BeginPlay() override;

    
    /// Binds input axes and actions.
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    
    /// Detects movement mode changes for fall tracking.
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

    
    /// Captures landing to evaluate fatal falls.
    virtual void Landed(const FHitResult& Hit) override;

    
    /// Stops timer and locks controls when level is complete.
    void CompleteLevel();

    
    /// Plays the standard death-style fade for level exit sequences.
    void PlayLevelExitFade();
#pragma endregion Methods

protected:
#pragma region Variables And Properties
#pragma region Components
    
    /// Spring arm used to orbit camera around the character.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(Tooltip="Camera boom positioning the follow camera", AllowPrivateAccess="true"))
    USpringArmComponent* CameraBoom;

    
    /// Player follow camera.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(Tooltip="Third-person follow camera", AllowPrivateAccess="true"))
    UCameraComponent* FollowCamera;

    
    /// Rope traversal component owning rope mechanics.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope", meta=(Tooltip="Rope traversal component handling aiming, throw, and swing", AllowPrivateAccess="true"))
    UBPC_RopeTraversalComponent* RopeComponent;

    
    /// Visual cable component used to render the rope.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope", meta=(Tooltip="Cable component rendering the rope between player and anchor", AllowPrivateAccess="true"))
    UCableComponent* RopeCable;

    
    /// Spline used to procedurally render rope segments.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Spline used to procedurally render rope between player and anchor", AllowPrivateAccess="true"))
    USplineComponent* RopeSpline;

    
    /// Instanced spline mesh pool for rope rendering.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Instanced spline mesh pool for rope rendering", AllowPrivateAccess="true"))
    TArray<USplineMeshComponent*> RopeMeshPool;

    
    /// Static mesh used for rope spline segments.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Static mesh used for rope spline segments", AllowPrivateAccess="true"))
    UStaticMesh* RopeMesh;

    
    /// Material override applied to rope spline segments.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Material override applied to rope spline segments", AllowPrivateAccess="true"))
    UMaterialInterface* RopeMeshMaterial;

    
    /// Preferred rope segment length for spline tessellation.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Preferred rope segment length in centimeters for spline tessellation", AllowPrivateAccess="true"))
    float RopeSegmentLength;

    
    /// Sag ratio applied to rope midpoint based on total length.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Sag ratio applied to rope midpoint as a fraction of rope length", AllowPrivateAccess="true"))
    float RopeSagRatio;

    
    /// Radius scale applied to rope mesh thickness.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope|Visual", meta=(Tooltip="Radius scale applied to rope mesh thickness", AllowPrivateAccess="true"))
    float RopeRadius;

    
    /// Socket used to attach the rope cable to the character mesh.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rope", meta=(DisplayName="Rope Cable Socket", Tooltip="Socket on the character mesh used as rope cable start", AllowPrivateAccess="true"))
    FName RopeCableAttachSocket;
#pragma endregion Components

#pragma region Serialized Fields
    
    /// Max walk speed in centimeters per second.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(Tooltip="Target walk speed in centimeters per second", AllowPrivateAccess="true"))
    float MaxWalkSpeed;

    
    /// Acceleration applied when input is present.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(Tooltip="Acceleration for inertia-driven movement", AllowPrivateAccess="true"))
    float MovementAcceleration;

    
    /// Deceleration applied when no input is present.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(Tooltip="Braking deceleration for inertia-driven stop", AllowPrivateAccess="true"))
    float MovementDeceleration;

    
    /// Scalar applied to jump strength based on current speed.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(Tooltip="Multiplier translating horizontal speed into jump boost", AllowPrivateAccess="true"))
    float JumpSpeedInfluence;

    
    /// Base jump velocity independent from movement.
    UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(Tooltip="Base jump Z velocity in centimeters per second", AllowPrivateAccess="true"))
    float BaseJumpZ;

    
    /// Third-person default camera boom length.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(Tooltip="Default camera boom arm length", AllowPrivateAccess="true"))
    float DefaultArmLength;

    
    /// Aiming camera boom length for zoomed view.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(Tooltip="Shorter boom when aiming the rope", AllowPrivateAccess="true"))
    float AimArmLength;

    
    /// Default camera offset applied to the boom.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(Tooltip="Target offset from boom origin for default camera height/side", AllowPrivateAccess="true"))
    FVector DefaultCameraOffset;

    
    /// Camera offset applied while aiming.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(Tooltip="Target offset from boom origin for aim camera height/side", AllowPrivateAccess="true"))
    FVector AimCameraOffset;

    
    /// Interp speed when switching camera states.
    UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(Tooltip="Interpolation speed for camera boom changes", AllowPrivateAccess="true"))
    float CameraInterpSpeed;

    
    /// Widget class used for aim icon feedback.
    UPROPERTY(EditDefaultsOnly, Category="UI", meta=(Tooltip="Widget class shown when aiming to indicate rope reach", AllowPrivateAccess="true"))
    TSubclassOf<class UUserWidget> AimIconWidgetClass;

    
    /// Instance of aim icon widget added to the viewport.
    UPROPERTY(Transient, meta=(Tooltip="Runtime instance of the aim icon widget"))
    class UUserWidget* AimIconWidget;

    
    /// Fatal fall height threshold.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(Tooltip="Vertical distance that triggers death when landed", AllowPrivateAccess="true"))
    float FatalFallHeight;

    
    /// Delay after death before respawn.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(Tooltip="Seconds to wait after fatal fall before respawn", AllowPrivateAccess="true"))
    float RespawnDelay;

    
    /// Duration of screen fade when dying.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(Tooltip="Seconds the screen remains black after death", AllowPrivateAccess="true"))
    float DeathFadeSeconds;

    
    /// Fall time required to reach maximum camera shake.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(Tooltip="Seconds falling beyond fatal height before max shake", AllowPrivateAccess="true"))
    float FallShakeRampSeconds;

    
    /// Camera shake played while falling beyond fatal height.
    UPROPERTY(EditDefaultsOnly, Category="Health", meta=(Tooltip="Camera shake class used while exceeding fatal fall height", AllowPrivateAccess="true"))
    TSubclassOf<UCameraShakeBase> FallCameraShakeClass;

    
    /// Starting location used for respawn.
    UPROPERTY(EditInstanceOnly, Category="Checkpoint", meta=(Tooltip="Spawn point used when resetting the character", AllowPrivateAccess="true"))
    FVector RespawnLocation;

    
    /// Timer display precision update interval.
    UPROPERTY(EditDefaultsOnly, Category="Timer", meta=(Tooltip="How often to update UI timer in seconds", AllowPrivateAccess="true"))
    float TimerTickRate;

#pragma region Camera Clamp
    /// Pitch cone angle centered on forward alignment in degrees.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(DisplayName="Pitch Cone Angle", Tooltip="Total pitch cone angle centered on forward alignment; clamped symmetrically up and down", AllowPrivateAccess="true"))
    float PitchConeAngleDegrees;

    /// Inverts pitch input for all camera control, including aim.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(DisplayName="Invert Look Pitch", Tooltip="If true, vertical look input is inverted for all camera control, including aim mode", AllowPrivateAccess="true"))
    bool bInvertAimLookPitch;
#pragma endregion Camera Clamp

#pragma region Input Smoothing
    /// Interpolation speed for smoothing movement input while walking.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(DisplayName="Walk Input Interp Speed", Tooltip="Speed used to smooth movement input toward the target vector when walking", AllowPrivateAccess="true"))
    float MovementInputInterpSpeedWalking;

    /// Interpolation speed for smoothing movement input while swinging.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Movement", meta=(DisplayName="Swing Input Interp Speed", Tooltip="Speed used to smooth movement input toward the target vector when swinging on the rope", AllowPrivateAccess="true"))
    float MovementInputInterpSpeedSwinging;
#pragma endregion Input Smoothing

#pragma region Input Assets
    
    /// Mapping context asset containing player input bindings.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Input mapping context asset for player controls", AllowPrivateAccess="true"))
    UInputMappingContext* PlayerInputContext;

    
    /// Input action for 2D locomotion.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action driving forward and right movement", AllowPrivateAccess="true"))
    UInputAction* MoveAction;

    
    /// Input action for yaw rotation.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action controlling yaw look", AllowPrivateAccess="true"))
    UInputAction* TurnAction;

    
    /// Input action for pitch rotation.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action controlling pitch look", AllowPrivateAccess="true"))
    UInputAction* LookUpAction;

    
    /// Input action for jumping.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action for jump start and release", AllowPrivateAccess="true"))
    UInputAction* JumpAction;

    
    /// Input action for rope aiming.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action toggling rope aim mode", AllowPrivateAccess="true"))
    UInputAction* AimAction;

    
    /// Input action for rope throw.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action for rope throw request", AllowPrivateAccess="true"))
    UInputAction* ThrowRopeAction;

    
    /// Input action for rope recall.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action starting and stopping rope recall", AllowPrivateAccess="true"))
    UInputAction* RecallRopeAction;

    
    /// Input action for rope grab or release.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action toggling rope hold state", AllowPrivateAccess="true"))
    UInputAction* ToggleHoldAction;

    
    /// Input action for climbing along the rope.
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(Tooltip="Enhanced Input action providing climb direction along the rope", AllowPrivateAccess="true"))
    UInputAction* ClimbAction;

    /// Whether to build default bindings at runtime instead of relying on asset data.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input", meta=(DisplayName="Build Runtime Defaults", Tooltip="If true, runtime populates classic defaults; if false, uses only the asset bindings", AllowPrivateAccess="true"))
    bool bBuildRuntimeDefaults;
#pragma endregion Input Assets
#pragma endregion Serialized Fields

#pragma region State
    
    /// Tracks whether player is currently aiming.
    bool bIsAiming;

    
    /// Start height of current fall for death detection.
    float FallStartZ;

    
    /// Whether we are tracking a fall segment.
    bool bTrackingFall;

    
    /// Accumulated time spent falling past threshold.
    float FallOverThresholdTime;

    
    /// Accumulated level timer seconds.
    float LevelTimerSeconds;

    
    /// Whether timer is running.
    bool bTimerActive;

    
    /// Cached forward movement input value.
    float CachedForwardInput;

    
    /// Cached right movement input value.
    float CachedRightInput;

    
    /// Runtime duplicate of the mapping context with default bindings.
    UPROPERTY(Transient, meta=(Tooltip="Runtime mapping context instance with default bindings"))
    UInputMappingContext* RuntimeInputContext;

    
    /// Tracks whether runtime mapping has already been configured.
    bool bInputMappingsBuilt;

    
    /// Raw 2D move input captured from Enhanced Input.
    FVector2D RawMoveInput;

    
    /// Smoothed 2D move input applied to locomotion.
    FVector2D SmoothedMoveInput;

    
    /// Neutral control rotation pitch used as clamp center.
    float NeutralPitchDegrees;

    
    /// Last known hang state to detect transitions.
    bool bWasHanging;

    
    /// Suppresses fall distance tracking while attached to a rope.
    bool bIgnoreFallFromRope;

    
    /// Whether a death/reset sequence is active.
    bool bDeathSequenceActive;

    
    /// Active fall shake instance while beyond fatal height.
    TWeakObjectPtr<UCameraShakeBase> ActiveFallShake;

    
    /// Last applied shake scale for fall feedback.
    float LastFallShakeScale;

    
    /// Timer handle used for delayed respawn.
    FTimerHandle RespawnTimerHandle;
#pragma endregion State
#pragma endregion Variables And Properties

private:
#pragma region Methods
#pragma region Input
    
    /// Processes 2D move input and applies world-space movement.
    void HandleMove(const FInputActionValue& Value);

    
    /// Processes yaw input from mouse or stick.
    void HandleLookYaw(const FInputActionValue& Value);

    
    /// Processes pitch input from mouse or stick.
    void HandleLookPitch(const FInputActionValue& Value);

    
    /// Processes climb input axis.
    void HandleClimbInput(const FInputActionValue& Value);

    
    /// Starts jump logic.
    void StartJump();

    
    /// Stops jump logic.
    void StopJump();

    
    /// Begins aiming mode.
    void BeginAim();

    
    /// Ends aiming mode.
    void EndAim();

    
    /// Throws rope while aiming.
    void ThrowRope();

    
    /// Toggles rope hold or drop.
    void ToggleHold();

    
    /// Starts rope recall input.
    void StartRecall();

    
    /// Stops rope recall input.
    void StopRecall();

    
    /// Adds mapping context to enhanced input subsystem.
    void InitializeInputMapping();

    
    /// Builds default mappings for classic controls.
    void ConfigureDefaultMappings(UInputMappingContext& Context) const;

    
    /// Helper to map an input action to a key with optional modifiers.
    void MapActionKey(UInputMappingContext& Context, UInputAction* Action, const FKey Key, const TArray<UInputModifier*>& Modifiers) const;

    
    /// Creates a negate modifier for given axes.
    class UInputModifierNegate* BuildNegateModifier(UInputMappingContext& Context, bool bNegateX, bool bNegateY, bool bNegateZ) const;

    
    /// Creates a swizzle modifier for axis remapping.
    class UInputModifierSwizzleAxis* BuildSwizzleModifier(UInputMappingContext& Context, const EInputAxisSwizzle Swizzle) const;


    /// Applies smoothed movement input to character locomotion.
    void ApplySmoothedMovement(float DeltaSeconds);

    /// Returns pitch input with user inversion applied uniformly across aim and free look.
    float GetAdjustedPitchInput(float RawPitch) const;
#pragma endregion Input

#pragma region Helpers
    
    /// Adjusts camera boom based on aim state.
    void UpdateCamera(float DeltaSeconds);

    
    /// Captures start height when entering falling.
    void BeginFallTrace();

    
    /// Ends fall trace and handles potential death.
    void EndFallTrace(const float LandHeight);

    
    /// Processes fatal fall and schedules respawn.
    void HandleFatalFall();

    
    /// Respawns character at configured location.
    void Respawn();

    
    /// Adds rope swing input mapping.
    void UpdateRopeSwingInput();

    
    /// Ticks and broadcasts timer.
    void TickLevelTimer(float DeltaSeconds);

    
    /// Updates rotation settings based on movement and rope state.
    void UpdateRotationSettings();

    
    /// Updates rope visual cable to follow the current anchor.
    void UpdateRopeVisual(float DeltaSeconds);

    
    /// Regenerates rope spline and mesh segments.
    void UpdateRopeSplineVisual(const FVector& SocketLocation, const FVector& AnchorLocation, float DeltaSeconds);

    
    /// Ensures rope mesh pool matches the desired segment count.
    void EnsureRopeMeshPool(const int32 SegmentCount);

    
    /// Hides all rope spline mesh instances.
    void HideRopeMeshes();

    
    /// Cached rope contact for smoothing visual kinks.
    FVector RopeContactPoint;

    
    /// Whether a rope contact point is cached.
    bool bHasRopeContact;

    
    /// Shows aim icon feedback based on preview validity.
    void UpdateAimIcon();

    
    /// Applies camera shake feedback while falling past the fatal threshold.
    void ApplyFallCameraFeedback();

    
    /// Stops fall camera shake and resets accumulators.
    void StopFallCameraFeedback();

    
    /// Triggers and manages death fade to black.
    void TriggerDeathFade();
#pragma endregion Helpers
#pragma endregion Methods
};
