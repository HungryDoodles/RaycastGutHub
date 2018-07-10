
#include "Capbot.h"
#include "CapbotMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"

ACapbot::ACapbot() 
{
	capbotMovement = CreateDefaultSubobject<UCapbotMovementComponent>("capbotMovementComponent");

	movementComponent = CreateDefaultSubobject<UCapsuleComponent>("movementComponent");
	SetRootComponent(movementComponent);
	cameraComponent = CreateDefaultSubobject<UCameraComponent>("cameraComponent");
	cameraComponent->AttachToComponent(movementComponent, FAttachmentTransformRules::KeepRelativeTransform);


	bReplicates = true;
	bReplicateMovement = false;
}

void ACapbot::BeginPlay() 
{
	Super::BeginPlay();
	cameraVerticalAngle = cameraComponent->GetComponentRotation().Yaw;
	capbotMovement->SetEnabled(true);
	capbotMovement->SetMovementMode(ECapbotMovementModes::CMM_Default);
	//cameraComponent->SetActive(true);
	capbotMovement->SetUpdatedComponent(movementComponent);
}

void ACapbot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const 
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
void ACapbot::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) 
{
	PlayerInputComponent->BindAxis(FName("Forward"), this, &ACapbot::ForwardInput);
	PlayerInputComponent->BindAxis(FName("Right"), this, &ACapbot::RightInput);
	PlayerInputComponent->BindAction(FName("Jump"), EInputEvent::IE_Pressed, this, &ACapbot::JumpInput);
	PlayerInputComponent->BindAxis(FName("LookUp"), this, &ACapbot::LookUpInput);
	PlayerInputComponent->BindAxis(FName("LookRight"), this, &ACapbot::LookRightInput);
}
void ACapbot::PossessedBy(AController * newController)
{
	Super::PossessedBy(newController);
}
void ACapbot::UnPossessed() 
{
	Super::UnPossessed();
}
void ACapbot::ForwardInput(float amount) 
{
	if (capbotMovement && movementComponent)
		capbotMovement->AddMoveInput(movementComponent->GetForwardVector() * amount);
}
void ACapbot::RightInput(float amount) 
{
	if (capbotMovement && movementComponent)
		capbotMovement->AddMoveInput(movementComponent->GetRightVector() * amount);
}
void ACapbot::MoveInput(FVector inputVector)
{
	if (capbotMovement)
		capbotMovement->AddMoveInput(inputVector);
}
void ACapbot::JumpInput() 
{
	capbotMovement->DoJumpInput();
}
void ACapbot::LookRightInput(float amount) 
{
	capbotMovement->AddLookInput(FVector(0, 0, amount));
}
void ACapbot::LookUpInput(float amount) 
{
	cameraVerticalAngle = FMath::Clamp(cameraVerticalAngle + amount, -90.f, 90.f);
	// Temporally: Instantly update camera
	cameraComponent->SetRelativeRotation(FRotator(cameraVerticalAngle, 0, 0));
}
void ACapbot::LookInput(FVector input) 
{
	LookRightInput(input.X);
	LookUpInput(input.Y);
}