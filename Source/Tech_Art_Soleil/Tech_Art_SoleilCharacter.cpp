// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tech_Art_SoleilCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// ATech_Art_SoleilCharacter

ATech_Art_SoleilCharacter::ATech_Art_SoleilCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	static ConstructorHelpers::FClassFinder<ARock> RockClassFinder(TEXT("/Game/Content/Blueprints/BP_Rock"));

	if (RockClassFinder.Class != nullptr)
		RockClass = RockClassFinder.Class;

}

void ATech_Art_SoleilCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
}

//////////////////////////////////////////////////////////////////////////
// Input

void ATech_Art_SoleilCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	const APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	// Add Input Mapping Context
	if (PlayerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATech_Art_SoleilCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATech_Art_SoleilCharacter::Look);

		// Throw
		EnhancedInputComponent->BindAction(ThrowAction, ETriggerEvent::Triggered, this, &ATech_Art_SoleilCharacter::Throw);
		EnhancedInputComponent->BindAction(ThrowAction, ETriggerEvent::Ongoing, this, &ATech_Art_SoleilCharacter::PredictTrajectory);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ATech_Art_SoleilCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATech_Art_SoleilCharacter::Look(const FInputActionValue& Value)
{
	// Input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATech_Art_SoleilCharacter::Throw(const FInputActionValue&)
{
	if (RockClass == nullptr)
		return;

	ARock* const Rock = GetWorld()->SpawnActor<ARock>(RockClass, GetThrowPosition(), Controller->GetControlRotation());
	Rock->Throw(ThrowForce);
}

void ATech_Art_SoleilCharacter::PredictTrajectory(const FInputActionValue&)
{
	// const FVector Force = Controller->GetControlRotation().RotateVector(FVector::ForwardVector) * ThrowForce;
	// // UE_LOG(LogTemp, Display, TEXT("Predict : %f ; %f ; %f"), Force.X, Force.Y, Force.Z);
	// const FPredictProjectilePathParams PredictParams(1.f, GetThrowPosition(), Force, TrajectoryPredictionTime);
	// FPredictProjectilePathResult Results;
	// UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams, Results);
	//
	// // Destroy the previous points, if they existed
	// for (AActor* const TrajectoryPoint : TrajectoryPoints)
	// 	TrajectoryPoint->Destroy();
	//
	// // Empty out the list
	// TrajectoryPoints.Empty();
	//
	// for (const FPredictProjectilePathPointData Point : Results.PathData)
	// {
	// 	const FVector Position = Point.Location;
	// 	// const FRotator Rotation = UKismetMathLibrary::FindLookAtRotation(Position, Point.Velocity);
	// 	const FRotator Rotation = FRotator::ZeroRotator;
	//
	// 	TrajectoryPoints.Add(GetWorld()->SpawnActor<AActor>(TrajectoryPointClass, Position, Rotation));
	// }
}

FVector ATech_Art_SoleilCharacter::GetThrowPosition() const
{
	return GetActorLocation() + GetActorForwardVector() * 50;
}
