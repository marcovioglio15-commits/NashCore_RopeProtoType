
/// Implements third-person character with inertia-based movement, rope interactions, fall safety, and timer tracking.
#include "Characters/BPA_PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BPC_RopeTraversalComponent.h"
#include "CableComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"

#pragma region Methods
#pragma region Lifecycle

/// Builds default components, movement tuning, and input asset references.
ABPA_PlayerCharacter::ABPA_PlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    RopeComponent = CreateDefaultSubobject<UBPC_RopeTraversalComponent>(TEXT("RopeComponent"));
    RopeCable = CreateDefaultSubobject<UCableComponent>(TEXT("RopeCable"));
    RopeCable->SetupAttachment(GetMesh());
    RopeCable->CableWidth = 4.0f;
    RopeCable->NumSegments = 12;
    RopeCable->CableLength = 1200.0f;
    RopeCable->SetVisibility(false);
    RopeCable->SetUsingAbsoluteRotation(true);
    RopeCable->SetUsingAbsoluteScale(true);
    RopeSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RopeSpline"));
    RopeSpline->SetupAttachment(GetRootComponent());
    RopeSpline->SetUsingAbsoluteLocation(true);
    RopeSpline->SetUsingAbsoluteRotation(true);

    MaxWalkSpeed = 800.0f;
    MovementAcceleration = 2400.0f;
    MovementDeceleration = 1200.0f;
    JumpSpeedInfluence = 0.35f;
    BaseJumpZ = 420.0f;
    DefaultArmLength = 400.0f;
    AimArmLength = 60.0f;
    DefaultCameraOffset = FVector(0.0f, 30.0f, 60.0f);
    AimCameraOffset = FVector(0.0f, 40.0f, 10.0f);
    CameraInterpSpeed = 6.0f;
    FatalFallHeight = 1200.0f;
    RespawnDelay = 1.75f;
    DeathFadeSeconds = 1.0f;
    FallShakeRampSeconds = 0.65f;
    FallCameraShakeClass = nullptr;
    TimerTickRate = 0.05f;
    PitchConeAngleDegrees = 90.0f;
    bInvertAimLookPitch = false;
    MovementInputInterpSpeedWalking = 8.0f;
    MovementInputInterpSpeedSwinging = 4.0f;
    bBuildRuntimeDefaults = true;
    RopeCableAttachSocket = TEXT("HandGrip_R");
    AimIconWidgetClass = nullptr;
    AimIconWidget = nullptr;
    RopeMesh = nullptr;
    RopeMeshMaterial = nullptr;
    RopeSegmentLength = 140.0f;
    RopeSagRatio = 0.12f;
    RopeRadius = 1.0f;
    RopeContactPoint = FVector::ZeroVector;
    bHasRopeContact = false;

    bUseControllerRotationYaw = false;
    bIsAiming = false;
    bTrackingFall = false;
    FallStartZ = 0.0f;
    FallOverThresholdTime = 0.0f;
    LevelTimerSeconds = 0.0f;
    bTimerActive = true;
    CachedForwardInput = 0.0f;
    CachedRightInput = 0.0f;
    RuntimeInputContext = nullptr;
    bInputMappingsBuilt = false;
    RawMoveInput = FVector2D::ZeroVector;
    SmoothedMoveInput = FVector2D::ZeroVector;
    NeutralPitchDegrees = GetActorRotation().Pitch;
    bWasHanging = false;
    bDeathSequenceActive = false;
    LastFallShakeScale = 0.0f;
    ActiveFallShake = nullptr;
    PlayerInputContext = nullptr;
    MoveAction = nullptr;
    TurnAction = nullptr;
    LookUpAction = nullptr;
    JumpAction = nullptr;
    AimAction = nullptr;
    ThrowRopeAction = nullptr;
    RecallRopeAction = nullptr;
    ToggleHoldAction = nullptr;
    ClimbAction = nullptr;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> PlayerContextAsset(TEXT("/Game/Programming/Input/IMC/IMC_PlayerControls.IMC_PlayerControls"));
    if (PlayerContextAsset.Succeeded())
        PlayerInputContext = PlayerContextAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> MoveAsset(TEXT("/Game/Programming/Input/IA/IA_Move.IA_Move"));
    if (MoveAsset.Succeeded())
        MoveAction = MoveAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> TurnAsset(TEXT("/Game/Programming/Input/IA/IA_Turn.IA_Turn"));
    if (TurnAsset.Succeeded())
        TurnAction = TurnAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> LookUpAsset(TEXT("/Game/Programming/Input/IA/IA_LookUp.IA_LookUp"));
    if (LookUpAsset.Succeeded())
        LookUpAction = LookUpAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> JumpAsset(TEXT("/Game/Programming/Input/IA/IA_Jump.IA_Jump"));
    if (JumpAsset.Succeeded())
        JumpAction = JumpAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> AimAsset(TEXT("/Game/Programming/Input/IA/IA_Aim.IA_Aim"));
    if (AimAsset.Succeeded())
        AimAction = AimAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> ThrowRopeAsset(TEXT("/Game/Programming/Input/IA/IA_ThrowRope.IA_ThrowRope"));
    if (ThrowRopeAsset.Succeeded())
        ThrowRopeAction = ThrowRopeAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> RecallRopeAsset(TEXT("/Game/Programming/Input/IA/IA_RecallRope.IA_RecallRope"));
    if (RecallRopeAsset.Succeeded())
        RecallRopeAction = RecallRopeAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> ToggleHoldAsset(TEXT("/Game/Programming/Input/IA/IA_ToggleHold.IA_ToggleHold"));
    if (ToggleHoldAsset.Succeeded())
        ToggleHoldAction = ToggleHoldAsset.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> ClimbAsset(TEXT("/Game/Programming/Input/IA/IA_Climb.IA_Climb"));
    if (ClimbAsset.Succeeded())
        ClimbAction = ClimbAsset.Object;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> RopeMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (RopeMeshAsset.Succeeded())
        RopeMesh = RopeMeshAsset.Object;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RopeMaterialAsset(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (RopeMaterialAsset.Succeeded())
        RopeMeshMaterial = RopeMaterialAsset.Object;

    static ConstructorHelpers::FClassFinder<UUserWidget> AimIconAsset(TEXT("/Game/Programming/UI/Widget/WB_AimIcon.WB_AimIcon_C"));
    if (AimIconAsset.Succeeded())
        AimIconWidgetClass = AimIconAsset.Class;

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Default/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (MannyMesh.Succeeded() && GetMesh() != nullptr)
    {
        GetMesh()->SetSkeletalMesh(MannyMesh.Object);
        GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
        GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    }

    static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnimBP(TEXT("/Game/Default/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
    if (UnarmedAnimBP.Succeeded() && GetMesh() != nullptr)
    {
        GetMesh()->SetAnimInstanceClass(UnarmedAnimBP.Class);
    }

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


/// Captures spawn data and prepares input mapping.
void ABPA_PlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (RespawnLocation.IsNearlyZero())
        RespawnLocation = GetActorLocation();

    InitializeInputMapping();
    if (RopeCable != nullptr && GetMesh() != nullptr)
    {
        RopeCable->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, RopeCableAttachSocket);
        RopeCable->SetRelativeLocation(FVector::ZeroVector);
        RopeCable->SetRelativeRotation(FRotator::ZeroRotator);
        RopeCable->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        RopeCable->SetUsingAbsoluteLocation(true);
    }

    NeutralPitchDegrees = 0.0f;

    if (AimIconWidgetClass != nullptr)
    {
        AimIconWidget = CreateWidget<UUserWidget>(GetWorld(), AimIconWidgetClass);

        if (AimIconWidget != nullptr)
        {
            AimIconWidget->AddToViewport();
            AimIconWidget->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}
#pragma endregion Lifecycle

#pragma region Tick

/// Updates camera interpolation, swing input propagation, timer accumulation, and fall tracking.
void ABPA_PlayerCharacter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateCamera(DeltaSeconds);
    ApplySmoothedMovement(DeltaSeconds);
    UpdateRotationSettings();
    UpdateRopeVisual(DeltaSeconds);
    UpdateAimIcon();
    UpdateRopeSwingInput();
    TickLevelTimer(DeltaSeconds);

    if (bTrackingFall)
    {
        const float CurrentFallDistance = FallStartZ - GetActorLocation().Z;

        if (CurrentFallDistance > FatalFallHeight)
        {
            FallOverThresholdTime += DeltaSeconds;
            ApplyFallCameraFeedback();
        }
        else
        {
            FallOverThresholdTime = 0.0f;
            StopFallCameraFeedback();
        }
    }
    else
    {
        StopFallCameraFeedback();
    }
}
#pragma endregion Tick

#pragma region Input Binding

/// Binds enhanced input actions to gameplay handlers.
void ABPA_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* const PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* const EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (EnhancedInput == nullptr)
        return;

    if (MoveAction != nullptr)
    {
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABPA_PlayerCharacter::HandleMove);
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABPA_PlayerCharacter::HandleMove);
    }

    if (TurnAction != nullptr)
        EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ABPA_PlayerCharacter::HandleLookYaw);

    if (LookUpAction != nullptr)
        EnhancedInput->BindAction(LookUpAction, ETriggerEvent::Triggered, this, &ABPA_PlayerCharacter::HandleLookPitch);

    if (JumpAction != nullptr)
    {
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ABPA_PlayerCharacter::StartJump);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABPA_PlayerCharacter::StopJump);
    }

    if (AimAction != nullptr)
    {
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ABPA_PlayerCharacter::BeginAim);
        EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ABPA_PlayerCharacter::EndAim);
    }

    if (ThrowRopeAction != nullptr)
        EnhancedInput->BindAction(ThrowRopeAction, ETriggerEvent::Started, this, &ABPA_PlayerCharacter::ThrowRope);

    if (ToggleHoldAction != nullptr)
        EnhancedInput->BindAction(ToggleHoldAction, ETriggerEvent::Started, this, &ABPA_PlayerCharacter::ToggleHold);

    if (RecallRopeAction != nullptr)
    {
        EnhancedInput->BindAction(RecallRopeAction, ETriggerEvent::Started, this, &ABPA_PlayerCharacter::StartRecall);
        EnhancedInput->BindAction(RecallRopeAction, ETriggerEvent::Completed, this, &ABPA_PlayerCharacter::StopRecall);
    }

    if (ClimbAction != nullptr)
    {
        EnhancedInput->BindAction(ClimbAction, ETriggerEvent::Triggered, this, &ABPA_PlayerCharacter::HandleClimbInput);
        EnhancedInput->BindAction(ClimbAction, ETriggerEvent::Completed, this, &ABPA_PlayerCharacter::HandleClimbInput);
    }

    InitializeInputMapping();
}


/// Adds mapping context and classic bindings into the enhanced input subsystem.
void ABPA_PlayerCharacter::InitializeInputMapping()
{
    if (RuntimeInputContext == nullptr)
    {
        if (PlayerInputContext != nullptr)
            RuntimeInputContext = DuplicateObject(PlayerInputContext, this);
        else
            RuntimeInputContext = NewObject<UInputMappingContext>(this);
    }

    if (RuntimeInputContext == nullptr)
        return;

    if (!bInputMappingsBuilt && bBuildRuntimeDefaults)
    {
        ConfigureDefaultMappings(*RuntimeInputContext);
        bInputMappingsBuilt = true;
    }

    APlayerController* const PlayerController = Cast<APlayerController>(Controller);

    if (PlayerController == nullptr)
        return;

    ULocalPlayer* const LocalPlayer = PlayerController->GetLocalPlayer();

    if (LocalPlayer == nullptr)
        return;

    UEnhancedInputLocalPlayerSubsystem* const InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();

    if (InputSubsystem == nullptr)
        return;

    InputSubsystem->RemoveMappingContext(RuntimeInputContext);
    InputSubsystem->AddMappingContext(RuntimeInputContext, 0);
}


/// Builds default PC and gamepad bindings mirroring classic Unreal layout.
void ABPA_PlayerCharacter::ConfigureDefaultMappings(UInputMappingContext& Context) const
{
    UInputModifierSwizzleAxis* SwizzleY = nullptr;
    UInputModifierNegate* NegateX = nullptr;
    UInputModifierNegate* NegateY = nullptr;

    if (MoveAction != nullptr)
    {
        SwizzleY = BuildSwizzleModifier(Context, EInputAxisSwizzle::YXZ);
        NegateX = BuildNegateModifier(Context, true, false, false);
        NegateY = BuildNegateModifier(Context, false, true, false);

        MapActionKey(Context, MoveAction, EKeys::D, {});
        MapActionKey(Context, MoveAction, EKeys::A, {NegateX});
        MapActionKey(Context, MoveAction, EKeys::W, {SwizzleY});
        MapActionKey(Context, MoveAction, EKeys::S, {SwizzleY, NegateY});
        MapActionKey(Context, MoveAction, EKeys::Gamepad_LeftX, {});
        MapActionKey(Context, MoveAction, EKeys::Gamepad_LeftY, {SwizzleY});
    }

    if (TurnAction != nullptr)
    {
        MapActionKey(Context, TurnAction, EKeys::MouseX, {});
        MapActionKey(Context, TurnAction, EKeys::Gamepad_RightX, {});
    }

    UInputModifierNegate* NegateAxis = nullptr;

    if (LookUpAction != nullptr)
    {
        if (NegateAxis == nullptr)
            NegateAxis = BuildNegateModifier(Context, true, false, false);

        MapActionKey(Context, LookUpAction, EKeys::MouseY, {NegateAxis});
        MapActionKey(Context, LookUpAction, EKeys::Gamepad_RightY, {});
    }

    if (JumpAction != nullptr)
    {
        MapActionKey(Context, JumpAction, EKeys::SpaceBar, {});
        MapActionKey(Context, JumpAction, EKeys::Gamepad_FaceButton_Bottom, {});
    }

    if (AimAction != nullptr)
    {
        MapActionKey(Context, AimAction, EKeys::RightMouseButton, {});
        MapActionKey(Context, AimAction, EKeys::Gamepad_LeftTrigger, {});
    }

    if (ThrowRopeAction != nullptr)
    {
        MapActionKey(Context, ThrowRopeAction, EKeys::LeftMouseButton, {});
        MapActionKey(Context, ThrowRopeAction, EKeys::Gamepad_RightTrigger, {});
    }

    if (ToggleHoldAction != nullptr)
    {
        MapActionKey(Context, ToggleHoldAction, EKeys::E, {});
        MapActionKey(Context, ToggleHoldAction, EKeys::Gamepad_RightShoulder, {});
    }

    if (RecallRopeAction != nullptr)
    {
        MapActionKey(Context, RecallRopeAction, EKeys::R, {});
        MapActionKey(Context, RecallRopeAction, EKeys::Gamepad_LeftShoulder, {});
    }

    if (ClimbAction != nullptr)
    {
        if (NegateAxis == nullptr)
            NegateAxis = BuildNegateModifier(Context, true, false, false);

        MapActionKey(Context, ClimbAction, EKeys::LeftShift, {});
        MapActionKey(Context, ClimbAction, EKeys::LeftControl, {NegateAxis});
        MapActionKey(Context, ClimbAction, EKeys::Gamepad_FaceButton_Top, {});
        MapActionKey(Context, ClimbAction, EKeys::Gamepad_FaceButton_Right, {NegateAxis});
    }
}


/// Adds a key mapping with optional modifiers to the provided context.
void ABPA_PlayerCharacter::MapActionKey(UInputMappingContext& Context, UInputAction* const Action, const FKey Key, const TArray<UInputModifier*>& Modifiers) const
{
    if (Action == nullptr)
        return;

    FEnhancedActionKeyMapping& Mapping = Context.MapKey(Action, Key);

    for (UInputModifier* const Modifier : Modifiers)
    {
        if (Modifier != nullptr)
            Mapping.Modifiers.Add(Modifier);
    }
}


/// Creates a negate modifier affecting specified axes.
UInputModifierNegate* ABPA_PlayerCharacter::BuildNegateModifier(UInputMappingContext& Context, const bool bNegateX, const bool bNegateY, const bool bNegateZ) const
{
    UInputModifierNegate* const NegateModifier = NewObject<UInputModifierNegate>(&Context);
    NegateModifier->bX = bNegateX;
    NegateModifier->bY = bNegateY;
    NegateModifier->bZ = bNegateZ;
    return NegateModifier;
}


/// Creates a swizzle modifier to remap axis ordering.
UInputModifierSwizzleAxis* ABPA_PlayerCharacter::BuildSwizzleModifier(UInputMappingContext& Context, const EInputAxisSwizzle Swizzle) const
{
    UInputModifierSwizzleAxis* const SwizzleModifier = NewObject<UInputModifierSwizzleAxis>(&Context);
    SwizzleModifier->Order = Swizzle;
    return SwizzleModifier;
}
#pragma endregion Input Binding

#pragma region Movement And Rotation

/// Tracks fall state transitions and evaluates landing.
void ABPA_PlayerCharacter::OnMovementModeChanged(const EMovementMode PrevMovementMode, const uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        if (MoveComp->MovementMode == MOVE_Falling)
            BeginFallTrace();
        else if (bTrackingFall)
            EndFallTrace(GetActorLocation().Z);
    }
    else if (bTrackingFall)
    {
        EndFallTrace(GetActorLocation().Z);
    }
}


/// Captures land height for fall evaluation.
void ABPA_PlayerCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    EndFallTrace(Hit.Location.Z);
}


/// Applies 2D move input to character locomotion and caches swing axes.
void ABPA_PlayerCharacter::HandleMove(const FInputActionValue& Value)
{
    const FVector2D MoveInput = Value.Get<FVector2D>();
    CachedForwardInput = MoveInput.Y;
    CachedRightInput = MoveInput.X;
    RawMoveInput = MoveInput;
}


/// Feeds yaw input from mouse or gamepad into controller rotation.
void ABPA_PlayerCharacter::HandleLookYaw(const FInputActionValue& Value)
{
    const float YawInput = Value.Get<float>();

    if (YawInput == 0.0f)
        return;

    AddControllerYawInput(YawInput);
}


/// Feeds pitch input from mouse or gamepad into controller rotation.
void ABPA_PlayerCharacter::HandleLookPitch(const FInputActionValue& Value)
{
    const float PitchInput = Value.Get<float>();

    if (PitchInput == 0.0f)
        return;

    const float AdjustedPitchInput = GetAdjustedPitchInput(PitchInput);

    AController* const OwnerController = GetController();

    if (OwnerController == nullptr)
        return;

    FRotator ControlRotation = OwnerController->GetControlRotation();
    const float HalfCone = PitchConeAngleDegrees * 0.5f;
    const float DesiredPitch = ControlRotation.Pitch + AdjustedPitchInput;
    const float TargetPitch = FMath::ClampAngle(DesiredPitch, NeutralPitchDegrees - HalfCone, NeutralPitchDegrees + HalfCone);
    ControlRotation.Pitch = TargetPitch;
    OwnerController->SetControlRotation(ControlRotation);
}
#pragma endregion Movement And Rotation

#pragma region Jump And Aim

/// Starts jump logic and routes hanging jump into ledge climb.
void ABPA_PlayerCharacter::StartJump()
{
    if (RopeComponent != nullptr)
    {
        if (RopeComponent->RequestLedgeClimbFromJump())
            return;

        if (RopeComponent->IsHanging())
        {
            // Ignore jump input while attached to the rope to keep the tether intact.
            return;
        }
    }

    UCharacterMovementComponent* const MoveComp = GetCharacterMovement();

    if (MoveComp != nullptr)
    {
        const float SpeedAlpha = MoveComp->Velocity.Size2D() / MaxWalkSpeed;
        MoveComp->JumpZVelocity = BaseJumpZ + SpeedAlpha * JumpSpeedInfluence * BaseJumpZ;
    }

    Jump();
}


/// Stops jump hold for variable height.
void ABPA_PlayerCharacter::StopJump()
{
    StopJumping();
}


/// Begins aiming mode and disables orient-to-movement.
void ABPA_PlayerCharacter::BeginAim()
{
    bIsAiming = true;

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
        MoveComp->bOrientRotationToMovement = false;

    if (RopeComponent != nullptr)
        RopeComponent->StartAim();

    UpdateRotationSettings();
}


/// Ends aiming mode and restores orient-to-movement.
void ABPA_PlayerCharacter::EndAim()
{
    bIsAiming = false;

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
        MoveComp->bOrientRotationToMovement = true;

    if (RopeComponent != nullptr)
        RopeComponent->StopAim();

    UpdateRotationSettings();
}
#pragma endregion Jump And Aim

#pragma region Rope Actions

/// Forwards rope throw request.
void ABPA_PlayerCharacter::ThrowRope()
{
    if (RopeComponent != nullptr)
        RopeComponent->ThrowRope();
}


/// Toggles rope hold or grab.
void ABPA_PlayerCharacter::ToggleHold()
{
    if (RopeComponent != nullptr)
        RopeComponent->ToggleHoldRequest();
}


/// Begins rope recall.
void ABPA_PlayerCharacter::StartRecall()
{
    if (RopeComponent != nullptr)
        RopeComponent->BeginRecall();
}


/// Stops rope recall.
void ABPA_PlayerCharacter::StopRecall()
{
    if (RopeComponent != nullptr)
        RopeComponent->CancelRecall();
}


/// Routes climb input into rope traversal.
void ABPA_PlayerCharacter::HandleClimbInput(const FInputActionValue& Value)
{
    if (RopeComponent == nullptr)
        return;

    const float ClimbValue = Value.Get<float>();

    if (ClimbValue > KINDA_SMALL_NUMBER)
        RopeComponent->BeginClimbUp();
    else if (ClimbValue < -KINDA_SMALL_NUMBER)
        RopeComponent->BeginClimbDown();
    else
        RopeComponent->StopClimb();
}
#pragma endregion Rope Actions

#pragma region Camera

/// Interpolates camera boom length based on aim state.
void ABPA_PlayerCharacter::UpdateCamera(const float DeltaSeconds)
{
    if (CameraBoom == nullptr)
        return;

    const float TargetArm = bIsAiming ? AimArmLength : DefaultArmLength;
    const float NewArm = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, CameraInterpSpeed);
    CameraBoom->TargetArmLength = NewArm;

    const FVector TargetOffset = bIsAiming ? AimCameraOffset : DefaultCameraOffset;
    const FVector NewOffset = FMath::VInterpTo(CameraBoom->TargetOffset, TargetOffset, DeltaSeconds, CameraInterpSpeed);
    CameraBoom->TargetOffset = NewOffset;
}


/// Updates rotation settings based on movement and rope state.
void ABPA_PlayerCharacter::UpdateRotationSettings()
{
    const bool bHanging = RopeComponent != nullptr && RopeComponent->IsHanging();
    UCharacterMovementComponent* const MoveComp = GetCharacterMovement();

    if (MoveComp != nullptr)
    {
        const bool bWalking = MoveComp->MovementMode == MOVE_Walking;
        const bool bHasMoveInput = !SmoothedMoveInput.IsNearlyZero();
        MoveComp->bOrientRotationToMovement = bWalking && bHasMoveInput && !bHanging;
    }

    bUseControllerRotationYaw = bHanging || bIsAiming;

    if (bHanging != bWasHanging)
    {
        if (bHanging)
            NeutralPitchDegrees = GetActorRotation().Pitch;

        bWasHanging = bHanging;
    }
}


/// Updates rope visual cable to follow the current anchor.
void ABPA_PlayerCharacter::UpdateRopeVisual(const float DeltaSeconds)
{
    if (RopeComponent == nullptr)
        return;

    const bool bRender = RopeComponent->IsAttached() || RopeComponent->IsRopeInFlight() || RopeComponent->IsRecalling();

    if (RopeCable != nullptr)
        RopeCable->SetVisibility(false);

    if (!bRender || RopeSpline == nullptr)
    {
        HideRopeMeshes();
        return;
    }

    const FVector SocketLocation = GetMesh() != nullptr ? GetMesh()->GetSocketLocation(RopeCableAttachSocket) : GetActorLocation();
    const FVector Anchor = RopeComponent->GetAnchorLocation();
    FVector RenderAnchor = Anchor;

    if (RopeComponent->IsRecalling())
    {
        const FVector Dir = (Anchor - SocketLocation).GetSafeNormal();
        RenderAnchor = SocketLocation + Dir * FMath::Max(RopeComponent->GetCurrentRopeLength(), 0.0f);
    }

    UpdateRopeSplineVisual(SocketLocation, RenderAnchor, DeltaSeconds);
}

/// Regenerates spline control points and meshes for rope rendering.
void ABPA_PlayerCharacter::UpdateRopeSplineVisual(const FVector& SocketLocation, const FVector& AnchorLocation, const float DeltaSeconds)
{
    if (RopeSpline == nullptr || RopeMesh == nullptr)
    {
        HideRopeMeshes();
        return;
    }

    const float Distance = FVector::Distance(SocketLocation, AnchorLocation);

    if (Distance <= KINDA_SMALL_NUMBER)
    {
        HideRopeMeshes();
        return;
    }

    RopeSpline->ClearSplinePoints(false);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(RopeSplineTrace), false, this);
    Params.AddIgnoredActor(this);

    for (USplineMeshComponent* const SplineMeshComp : RopeMeshPool)
    {
        if (SplineMeshComp != nullptr)
            Params.AddIgnoredComponent(SplineMeshComp);
    }

    TArray<FVector> ControlPoints;
    ControlPoints.Add(SocketLocation);

    FVector TraceStart = SocketLocation;
    FVector TraceEnd = AnchorLocation;
    const float SweepRadius = FMath::Max(RopeRadius * 4.0f, 8.0f);
    FHitResult Hit;
    int32 ContactCount = 0;

    while (ContactCount < 2 && GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SweepRadius), Params))
    {
        FVector Contact = Hit.ImpactPoint + Hit.ImpactNormal * SweepRadius * 0.5f;

        if (bHasRopeContact)
            Contact = FMath::VInterpTo(RopeContactPoint, Contact, DeltaSeconds, 12.0f);

        RopeContactPoint = Contact;
        bHasRopeContact = true;
        ControlPoints.Add(Contact);
        TraceStart = Contact + Hit.ImpactNormal * 2.0f;

        if (FVector::Distance(TraceStart, TraceEnd) < 10.0f)
            break;

        ++ContactCount;
    }

    if (ContactCount == 0)
        bHasRopeContact = false;

    ControlPoints.Add(AnchorLocation);

    int32 SplineIndex = 0;
    RopeSpline->AddSplinePoint(ControlPoints[0], ESplineCoordinateSpace::World, false);
    ++SplineIndex;

    for (int32 PointIndex = 0; PointIndex + 1 < ControlPoints.Num(); ++PointIndex)
    {
        const FVector Start = ControlPoints[PointIndex];
        const FVector End = ControlPoints[PointIndex + 1];
        const float SpanLength = FVector::Distance(Start, End);
        const FVector MidPoint = FMath::Lerp(Start, End, 0.5f) + FVector::DownVector * (SpanLength * RopeSagRatio);
        RopeSpline->AddSplinePoint(MidPoint, ESplineCoordinateSpace::World, false);
        RopeSpline->SetSplinePointType(SplineIndex, ESplinePointType::Curve, false);
        ++SplineIndex;
        RopeSpline->AddSplinePoint(End, ESplineCoordinateSpace::World, false);
        RopeSpline->SetSplinePointType(SplineIndex, ESplinePointType::Curve, false);
        ++SplineIndex;
    }

    RopeSpline->UpdateSpline();

    const float SplineLength = RopeSpline->GetSplineLength();
    const float SegmentTarget = RopeSegmentLength > KINDA_SMALL_NUMBER ? RopeSegmentLength : 100.0f;
    const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(SplineLength / SegmentTarget), 1, 64);
    EnsureRopeMeshPool(SegmentCount);

    const float SegmentDistance = SplineLength / SegmentCount;

    for (int32 Index = 0; Index < RopeMeshPool.Num(); ++Index)
    {
        USplineMeshComponent* const SplineMeshComp = RopeMeshPool[Index];

        if (SplineMeshComp == nullptr)
            continue;

        if (Index >= SegmentCount)
        {
            SplineMeshComp->SetVisibility(false);
            SplineMeshComp->SetHiddenInGame(true);
            continue;
        }

        const float StartDistance = SegmentDistance * Index;
        const float EndDistance = SegmentDistance * (Index + 1);
        const FVector StartPos = RopeSpline->GetLocationAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World);
        const FVector EndPos = RopeSpline->GetLocationAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::World);
        const FVector StartTangent = RopeSpline->GetTangentAtDistanceAlongSpline(StartDistance, ESplineCoordinateSpace::World);
        const FVector EndTangent = RopeSpline->GetTangentAtDistanceAlongSpline(EndDistance, ESplineCoordinateSpace::World);

        SplineMeshComp->SetStaticMesh(RopeMesh);

        if (RopeMeshMaterial != nullptr)
            SplineMeshComp->SetMaterial(0, RopeMeshMaterial);

        SplineMeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent);
        SplineMeshComp->SetStartScale(FVector2D(RopeRadius, RopeRadius));
        SplineMeshComp->SetEndScale(FVector2D(RopeRadius, RopeRadius));
        SplineMeshComp->SetVisibility(true);
        SplineMeshComp->SetHiddenInGame(false);
    }
}

/// Grows rope mesh pool to desired size.
void ABPA_PlayerCharacter::EnsureRopeMeshPool(const int32 SegmentCount)
{
    if (RopeSpline == nullptr)
        return;

    while (RopeMeshPool.Num() < SegmentCount)
    {
        USplineMeshComponent* const NewMesh = NewObject<USplineMeshComponent>(this);

        if (NewMesh == nullptr)
            return;

        NewMesh->SetMobility(EComponentMobility::Movable);
        NewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NewMesh->SetCastShadow(false);
        NewMesh->SetForwardAxis(ESplineMeshAxis::X);
        NewMesh->AttachToComponent(RopeSpline, FAttachmentTransformRules::KeepWorldTransform);
        NewMesh->RegisterComponent();
        RopeMeshPool.Add(NewMesh);
    }
}

/// Hides spline mesh instances when rope is not rendered.
void ABPA_PlayerCharacter::HideRopeMeshes()
{
    for (USplineMeshComponent* const SplineMeshComp : RopeMeshPool)
    {
        if (SplineMeshComp != nullptr)
        {
            SplineMeshComp->SetVisibility(false);
            SplineMeshComp->SetHiddenInGame(true);
        }
    }
    bHasRopeContact = false;
}


/// Drives aim icon visibility and tint based on preview reachability.
void ABPA_PlayerCharacter::UpdateAimIcon()
{
    if (AimIconWidget == nullptr)
        return;

    if (!bIsAiming)
    {
        AimIconWidget->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    AimIconWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    const bool bHasPreview = RopeComponent != nullptr && RopeComponent->HasValidPreview();
    const bool bWithinRange = bHasPreview && RopeComponent->IsPreviewWithinRange();
    const FLinearColor IconColor = bHasPreview && bWithinRange ? FLinearColor(0.8f, 1.0f, 0.8f, 0.8f) : FLinearColor(1.0f, 0.25f, 0.25f, 0.8f);
    AimIconWidget->SetColorAndOpacity(IconColor);
}
#pragma endregion Camera

#pragma region Fall Handling

/// Applies screen shake while falling beyond the fatal threshold.
void ABPA_PlayerCharacter::ApplyFallCameraFeedback()
{
    if (FallCameraShakeClass == nullptr || Controller == nullptr)
        return;

    const float RampSeconds = FMath::Max(FallShakeRampSeconds, KINDA_SMALL_NUMBER);
    const float ShakeScale = FMath::Clamp(FallOverThresholdTime / RampSeconds, 0.0f, 1.0f);

    if (ShakeScale <= KINDA_SMALL_NUMBER)
    {
        StopFallCameraFeedback();
        return;
    }

    APlayerController* const PC = Cast<APlayerController>(Controller);

    if (PC == nullptr || PC->PlayerCameraManager == nullptr)
        return;

    if (ActiveFallShake.IsValid())
    {
        PC->PlayerCameraManager->StopCameraShake(ActiveFallShake.Get(), false);
    }

    ActiveFallShake = PC->PlayerCameraManager->StartCameraShake(FallCameraShakeClass, ShakeScale);
    LastFallShakeScale = ShakeScale;
}

/// Stops any active fall shake and resets metrics.
void ABPA_PlayerCharacter::StopFallCameraFeedback()
{
    if (!ActiveFallShake.IsValid() && LastFallShakeScale <= KINDA_SMALL_NUMBER)
        return;

    if (APlayerController* const PC = Cast<APlayerController>(Controller))
    {
        if (PC->PlayerCameraManager != nullptr && ActiveFallShake.IsValid())
        {
            PC->PlayerCameraManager->StopCameraShake(ActiveFallShake.Get(), true);
        }
    }

    ActiveFallShake = nullptr;
    LastFallShakeScale = 0.0f;
}

/// Fades the screen to black to cover respawn.
void ABPA_PlayerCharacter::TriggerDeathFade()
{
    if (APlayerController* const PC = Cast<APlayerController>(Controller))
    {
        if (APlayerCameraManager* const CameraManager = PC->PlayerCameraManager)
        {
            CameraManager->StartCameraFade(0.0f, 1.0f, 0.25f, FLinearColor::Black, false, true);
        }
    }
}


/// Starts tracking fall distance when entering falling mode.
void ABPA_PlayerCharacter::BeginFallTrace()
{
    if (bDeathSequenceActive)
        return;

    if (!bTrackingFall)
    {
        bTrackingFall = true;
        FallStartZ = GetActorLocation().Z;
        FallOverThresholdTime = 0.0f;
    }
}


/// Ends fall tracking and evaluates fatal height.
void ABPA_PlayerCharacter::EndFallTrace(const float LandHeight)
{
    if (!bTrackingFall)
        return;

    bTrackingFall = false;
    StopFallCameraFeedback();
    const float FallDistance = FallStartZ - LandHeight;

    if (FallDistance >= FatalFallHeight)
        HandleFatalFall();
    else
        FallOverThresholdTime = 0.0f;
}


/// Disables control and schedules respawn after fatal fall.
void ABPA_PlayerCharacter::HandleFatalFall()
{
    if (bDeathSequenceActive)
        return;

    bDeathSequenceActive = true;
    FallOverThresholdTime = 0.0f;
    StopFallCameraFeedback();

    AController* const OwnerController = GetController();

    if (OwnerController != nullptr)
        OwnerController->DisableInput(nullptr);

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
        MoveComp->DisableMovement();

    if (RopeComponent != nullptr)
        RopeComponent->ForceReset();

    TriggerDeathFade();

    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
    const float RespawnTime = FMath::Max(RespawnDelay, DeathFadeSeconds);
    GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &ABPA_PlayerCharacter::Respawn, RespawnTime, false);
}


/// Teleports character to respawn point and re-enables movement.
void ABPA_PlayerCharacter::Respawn()
{
    StopFallCameraFeedback();
    GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
    SetActorLocation(RespawnLocation, false);
    SetActorRotation(FRotator::ZeroRotator);

    if (AController* const OwnerController = GetController())
        OwnerController->EnableInput(nullptr);

    if (UCharacterMovementComponent* const MoveComp = GetCharacterMovement())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->SetMovementMode(MOVE_Walking);
    }

    bDeathSequenceActive = false;
    bTrackingFall = false;
    FallOverThresholdTime = 0.0f;

    if (APlayerController* const PC = Cast<APlayerController>(GetController()))
    {
        if (APlayerCameraManager* const CameraManager = PC->PlayerCameraManager)
        {
            CameraManager->StartCameraFade(1.0f, 0.0f, 0.35f, FLinearColor::Black, false, false);
        }
    }
}
#pragma endregion Fall Handling

#pragma region Timer And Completion

/// Stops the timer and locks controls on level completion.
void ABPA_PlayerCharacter::CompleteLevel()
{
    bTimerActive = false;

    if (AController* const OwnerController = GetController())
        OwnerController->DisableInput(nullptr);
}


/// Forwards cached movement to rope swing input when hanging.
void ABPA_PlayerCharacter::UpdateRopeSwingInput()
{
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
        RopeComponent->ApplySwingInput(FVector2D(CachedRightInput, CachedForwardInput));
}


/// Accumulates timer only when active.
void ABPA_PlayerCharacter::TickLevelTimer(const float DeltaSeconds)
{
    if (!bTimerActive)
        return;

    LevelTimerSeconds += DeltaSeconds;
}
#pragma endregion Timer And Completion
#pragma endregion Methods
/// Applies smoothed movement input to character locomotion.
void ABPA_PlayerCharacter::ApplySmoothedMovement(const float DeltaSeconds)
{
    if (RopeComponent != nullptr && RopeComponent->IsHanging())
    {
        SmoothedMoveInput = FMath::Vector2DInterpTo(SmoothedMoveInput, FVector2D::ZeroVector, DeltaSeconds, MovementInputInterpSpeedSwinging);
        return;
    }

    SmoothedMoveInput = FMath::Vector2DInterpTo(SmoothedMoveInput, RawMoveInput, DeltaSeconds, MovementInputInterpSpeedWalking);

    if (SmoothedMoveInput.IsNearlyZero())
        return;

    const FRotator ControlRotation = GetControlRotation();
    const FRotator YawOnlyRotation(0.0f, ControlRotation.Yaw, 0.0f);
    const FVector ForwardDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::X);
    const FVector RightDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDirection, SmoothedMoveInput.Y);
    AddMovementInput(RightDirection, SmoothedMoveInput.X);
}

/// Applies unified pitch inversion independent of aim state.
float ABPA_PlayerCharacter::GetAdjustedPitchInput(const float RawPitch) const
{
    return bInvertAimLookPitch ? -RawPitch : RawPitch;
}
