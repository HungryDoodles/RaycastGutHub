// Fill out your copyright notice in the Description page of Project Settings.

#include "CapbotMovementComponent.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine.h"
#include "UnrealNetwork.h"

DEFINE_LOG_CATEGORY(CapbotMovementComponentLog);

void FCapbotMovementState_Server::ApplyMovementState(const FCapbotMovementState& movementState, float timeStamp) 
{
	FCapbotMovementState_Server::movementState = movementState;
	FCapbotMovementState_Server::timeStamp = timeStamp;
}
FCapbotMovementState_Server FCapbotMovementState_Server::Make(const FCapbotMovementState& movementState, float timeStamp) 
{
	FCapbotMovementState_Server newState;
	newState.movementState = movementState;
	newState.timeStamp = timeStamp;

	return newState;
}
FCapbotMovementState FCapbotMovementState_Server::Interpolate(const FCapbotMovementState_Server& a, const FCapbotMovementState_Server& b, float time)
{
	if (time <= a.timeStamp)
	{
		return a.movementState;
	}
	else if (time >= b.timeStamp) 
	{
		return b.movementState;
	}
	else 
	{
		float alpha = (time - a.timeStamp) / (b.timeStamp - a.timeStamp);
		FCapbotMovementState newState;
		newState.ground = a.movementState.ground;
		newState.bIsLanded = a.movementState.bIsLanded;
		newState.location = FMath::Lerp(a.movementState.location, b.movementState.location, alpha);
		newState.rotation = FMath::Lerp(a.movementState.rotation, b.movementState.rotation, alpha);
		newState.velocity = FMath::Lerp(a.movementState.velocity, b.movementState.velocity, alpha);
		newState.mode = a.movementState.mode;

		return newState;
	}
}

UCapbotMovementComponent::UCapbotMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
}


void UCapbotMovementComponent::BeginPlay()
{
	Super::BeginPlay();
}
void UCapbotMovementComponent::NormalizeInput() 
{
	accumulatedInput.moveInput.Normalize();
}
void UCapbotMovementComponent::ResetInput() 
{
	accumulatedInput = FCapbotMovementInput();
}
void UCapbotMovementComponent::SetEnabled(bool bNewEnabled) 
{
	if (bNewEnabled == bEnabled)
		return;

	bEnabled = bNewEnabled;
}
void UCapbotMovementComponent::SetMovementMode(ECapbotMovementModes newMode) 
{
	currentMovementState.mode = newMode;
}

void UCapbotMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bEnabled && UpdatedComponent)
	{
		bool bLocallyControlled = false;
		bool bHasAuthority = GetOwner()->HasAuthority();
		if (APawn * pawn = Cast<APawn>(GetOwner()))
			if (pawn->IsLocallyControlled())
				bLocallyControlled = true;

		if (bLocallyControlled && bHasAuthority) 
		{
			TickServerOwner(DeltaTime);
		}
		else if (bHasAuthority) 
		{
			//if (lastServerInputTimeStamp != GetWorld()->TimeSeconds) TickServerRemote(DeltaTime);
		}
		else if (bLocallyControlled) 
		{
			TickClientOwner(DeltaTime);
		}
		else 
		{
			TickClientRemote(DeltaTime);
		}
	}
}

void UCapbotMovementComponent::TickServerOwner(float DeltaTime)
{
	NormalizeInput();
	PerformMovement(accumulatedInput, DeltaTime);
	MulticastSendMoveResult(currentMovementState, accumulatedInput);
	ResetInput();
}
void UCapbotMovementComponent::TickServerRemote(float DeltaTime, bool bSyncTimeStamp)
{
	/*if (bInputReceived)
	{
		bInputReceived = false;
		return;
	}*/
	NormalizeInput();

	PerformMovement(accumulatedInput, accumulatedInput.deltaTime);
	MulticastSendMoveResult(currentMovementState, accumulatedInput);

	if (bSyncTimeStamp)
		clientInputTime = accumulatedInput.timeStamp;
	//else
	//	clientInputTime += DeltaTime;

	if (serverMovementSaved.Num() < maxSavedMovementSize)
		serverMovementSaved.Push(FCapbotMovementState_Server::Make(currentMovementState, clientInputTime));
}
void UCapbotMovementComponent::TickClientOwner(float DeltaTime)
{
	NormalizeInput();

	accumulatedInput.timeStamp = GetWorld()->TimeSeconds;
	accumulatedInput.deltaTime = DeltaTime;

	ServerSendMoveResult(currentMovementState, GetWorld()->TimeSeconds);
	ServerSendInput(accumulatedInput);

	PerformMovement(accumulatedInput, DeltaTime);
	if (clientInputSaved.Num() < maxSavedInputSize)
		clientInputSaved.Push(accumulatedInput);
	ResetInput();
}
void UCapbotMovementComponent::TickClientRemote(float DeltaTime)
{
	PerformMovement(accumulatedInput, DeltaTime);
}

bool UCapbotMovementComponent::PerformMovement(const FCapbotMovementInput& input, float deltaTime)
{
	switch (currentMovementState.mode)
	{
	case ECapbotMovementModes::CMM_Default:
		DefaultMove(input, deltaTime);
		return true;
	default:
		return false;
	}
}
void UCapbotMovementComponent::DefaultMove(const FCapbotMovementInput& input, float deltaTime)
{
	UWorld * world = GetWorld();
	
	/* Perform acceleration routine */
	// Standart movement
	if (!input.moveInput.IsNearlyZero())
	{
		currentMovementState.velocity += input.moveInput * acceleration * deltaTime;
	}
	else if (currentMovementState.bIsLanded)
	{
		float decelerationValue = deceleration * deltaTime;
		if (currentMovementState.velocity.SizeSquared() < decelerationValue * decelerationValue)
			currentMovementState.velocity = FVector::ZeroVector;
		else
			currentMovementState.velocity -= currentMovementState.velocity.GetUnsafeNormal() * decelerationValue;
	}
	// Gravity
	if (world) if (AWorldSettings * settings = world->GetWorldSettings()) 
		currentMovementState.velocity += FVector(0, 0, settings->GetGravityZ() * deltaTime);
	// Jump
	if (currentMovementState.bIsLanded) if ((input.flags & ECapbotMovementInputFlags::CMI_Jump) > 0)
		currentMovementState.velocity += FVector(0, 0, jumpVelocity);
	//Look
	currentMovementState.rotation += FRotator::MakeFromEuler(input.lookInput);

	FVector positionDelta = currentMovementState.velocity * deltaTime;

	currentMovementState.bIsLanded = false;
	bPositionCorrected = false;
	if (!positionDelta.IsNearlyZero(1e-6f))
	{
		currentMovementState.location = UpdatedComponent->GetComponentLocation();

		FHitResult hit(1.f);
		SafeMoveUpdatedComponent(positionDelta, currentMovementState.rotation, true, hit);

		if (hit.IsValidBlockingHit())
		{
			currentMovementState.velocity -= hit.Normal * FVector::DotProduct(currentMovementState.velocity, hit.Normal);
			currentMovementState.bIsLanded = true;
			HandleImpact(hit, deltaTime, positionDelta);
			SlideAlongSurface(positionDelta, 1.f - hit.Time, hit.Normal, hit, true);
			if (hit.IsValidBlockingHit()) 
				currentMovementState.velocity -= hit.Normal * FVector::DotProduct(currentMovementState.velocity, hit.Normal);
		}

		if (!bPositionCorrected)
		{
			const FVector NewLocation = UpdatedComponent->GetComponentLocation();
			Velocity = ((NewLocation - currentMovementState.location) / deltaTime);
		}

		currentMovementState.location = UpdatedComponent->GetComponentLocation();
	}
}
bool UCapbotMovementComponent::TracePrimitiveDefault(FHitResult& hit, FCapbotMovementState& fromState, float deltaTime)
{
	UWorld * world = GetWorld();

	if (!world)
		return false;
	if (!UpdatedPrimitive)
		return false;

	FVector startPoint = fromState.location;
	FVector endPoint = fromState.location + fromState.velocity * deltaTime;
	
	FCollisionQueryParams q;
	q.AddIgnoredActor(GetOwner());
	q.bTraceComplex = true;

	return world->SweepSingleByProfile(hit, startPoint, endPoint, FQuat::Identity, FName("Pawn"), UpdatedPrimitive->GetCollisionShape(), q);
}
void UCapbotMovementComponent::ApplyMovementState(const FCapbotMovementState& newState) 
{
	if (!bEnabled || !UpdatedComponent)
		return;

	currentMovementState = newState;

	UpdatedComponent->SetWorldLocationAndRotation(currentMovementState.location, currentMovementState.rotation);
}
bool UCapbotMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) 
{
	bPositionCorrected |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
	return bPositionCorrected;
}

void UCapbotMovementComponent::AddMoveInput(FVector input)
{
	accumulatedInput.moveInput += input;
}
void UCapbotMovementComponent::AddLookInput(FVector input) 
{
	accumulatedInput.lookInput += input;
}
void UCapbotMovementComponent::DoJumpInput() 
{
	accumulatedInput.flags |= (int8)ECapbotMovementInputFlags::CMI_Jump;
}
void UCapbotMovementComponent::AddInput(FCapbotMovementInput input) 
{
	accumulatedInput.moveInput += input.moveInput;
	accumulatedInput.lookInput += input.lookInput;
	accumulatedInput.flags |= input.flags;
	accumulatedInput.timeStamp = FMath::Max(accumulatedInput.timeStamp, input.timeStamp);
}

void UCapbotMovementComponent::CompensateSeconds(float amount) 
{

}
void UCapbotMovementComponent::RevertCompensation() 
{

}

bool UCapbotMovementComponent::ServerSendInput_Validate(FCapbotMovementInput input)
{
	return true; // Any movement is valid
}
void UCapbotMovementComponent::ServerSendInput_Implementation(FCapbotMovementInput input)
{
	accumulatedInput = input;
	// Perform movement in-place
	if (APawn * pawn = Cast<APawn>(GetOwner()))
		if (!pawn->IsLocallyControlled()) // Check if it's not Server's own pawn
		{
			//bInputReceived = false; // For multiple ticks
			TickServerRemote(FMath::Max(0.0f, accumulatedInput.deltaTime), true /*Sync server timestamp*/);
			//bInputReceived = true;
			lastServerInputTimeStamp = GetWorld()->TimeSeconds;
		}
}
bool UCapbotMovementComponent::ServerSendMoveResult_Validate(FCapbotMovementState result, float timeStamp)
{ return true; }
void UCapbotMovementComponent::ServerSendMoveResult_Implementation(FCapbotMovementState result, float timeStamp)
{
	if (timeStamp >= serverLastClientMovement.timeStamp) 
	{
		int savedNum = serverMovementSaved.Num();
		if (savedNum == 0)
			return;

		serverLastClientMovement = FCapbotMovementState_Server::Make(result, timeStamp);
		bClientMoveReceived = true;

		DrawDebugCapsule(GetWorld(), result.location, 56.f, 30.f, FQuat::Identity, FColor::Red);

		if (savedNum == 1)
		{
			if (!FMath::IsNearlyEqual(serverLastClientMovement.timeStamp, timeStamp, 0.001f))
				return;

			float offset = (serverMovementSaved[0].movementState.location - result.location).Size();
			DrawDebugCapsule(GetWorld(), serverMovementSaved[0].movementState.location, 56.f, 30.f, FQuat::Identity, FColor::Yellow);
			if (offset > maxAcceptableOffset)
			{
				ClientCorrectMove(serverMovementSaved[0].movementState, timeStamp);
				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, "Correct move (" + FString::SanitizeFloat(offset) + "): " + FString::SanitizeFloat(timeStamp));
			}
			else
			{
				ClientAckGoodMove(timeStamp);
				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, "Ack good move(" + FString::SanitizeFloat(offset) + "): " + FString::SanitizeFloat(timeStamp));
			}

			serverMovementSaved.RemoveAll([timeStamp](const FCapbotMovementState_Server& elem) { return elem.timeStamp < timeStamp; });
			return;
		}

		int i;
		for (i = savedNum - 2; i > 0; --i)
			if (serverMovementSaved[i].timeStamp <= timeStamp)
				break;

		FCapbotMovementState interpolatedState = FCapbotMovementState_Server::Interpolate(serverMovementSaved[i], serverMovementSaved[i + 1], timeStamp);
		DrawDebugCapsule(GetWorld(), interpolatedState.location, 56.f, 30.f, FQuat::Identity, FColor::Blue);

		float offset = (interpolatedState.location - result.location).Size();
		if (offset > maxAcceptableOffset)
		{
			ClientCorrectMove(interpolatedState, timeStamp);
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, "Correct move (" + FString::SanitizeFloat(offset) + "): " + FString::SanitizeFloat(timeStamp));
		}
		else
		{
			ClientAckGoodMove(timeStamp);
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, "Ack good move(" + FString::SanitizeFloat(offset) + "): " + FString::SanitizeFloat(timeStamp));
		}

		serverMovementSaved.RemoveAll([timeStamp](const FCapbotMovementState_Server& elem) { return elem.timeStamp < timeStamp; });
	}
}
void UCapbotMovementComponent::ClientSendMoveResult_Implementation(FCapbotMovementState result)
{
	ApplyMovementState(result);
}
void UCapbotMovementComponent::ClientAckGoodMove_Implementation(float timeStamp) 
{
	clientInputSaved.RemoveAll([timeStamp](const FCapbotMovementInput& elem) { return elem.timeStamp < timeStamp; });
}
void UCapbotMovementComponent::ClientCorrectMove_Implementation(FCapbotMovementState newState, float timeStamp)
{
	if (APawn * pawn = Cast<APawn>(GetOwner()))
		if (!pawn->IsLocallyControlled())
			return; // Nop for not-my-pawn

	int index = clientInputSaved.Num() - 1;
	for (; index >= 0; --index)
		if (clientInputSaved[index].timeStamp <= timeStamp) // Condition == is important AF
		{
			ApplyMovementState(newState);

			for (; index < clientInputSaved.Num(); ++index)
				PerformMovement(clientInputSaved[index], clientInputSaved[index].deltaTime);

			clientInputSaved.Empty();

			return;
		}

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, "No replay data");
}
void UCapbotMovementComponent::MulticastSendMoveResult_Implementation(FCapbotMovementState result, FCapbotMovementInput input)
{
	if (APawn * pawn = Cast<APawn>(GetOwner()))
		if (!pawn->IsLocallyControlled())
	{
		ApplyMovementState(result);
		accumulatedInput = input;
	}
}