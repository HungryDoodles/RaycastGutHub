// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "FLagCompensateable.h"
#include "CapbotMovementComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(CapbotMovementComponentLog, Log, All);


UENUM(BlueprintType)
enum class ECapbotMovementModes : uint8
{
	CMM_NONE, CMM_Default
};
enum ECapbotMovementInputFlags : uint8
{
	CMI_Jump = 0x01
};

USTRUCT()
struct FCapbotMovementState
{
	GENERATED_BODY()

	UPROPERTY()
	USceneComponent * ground;

	UPROPERTY()
	FVector location;
	UPROPERTY()
	FRotator rotation;

	UPROPERTY()
	FVector velocity;

	UPROPERTY()
	TEnumAsByte<ECapbotMovementModes> mode;

	UPROPERTY()
	bool bIsLanded;
};

USTRUCT()
struct FCapbotMovementInput 
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 flags;

	UPROPERTY()
	FVector moveInput;

	UPROPERTY()
	FVector lookInput;

	UPROPERTY()
	float timeStamp = -1.f;

	UPROPERTY()
	float deltaTime;

	inline bool IsEmpty() { return timeStamp == -1.f; }
};

USTRUCT()
struct FCapbotMovementState_Server 
{
	GENERATED_BODY()

	UPROPERTY()
	FCapbotMovementState movementState;

	UPROPERTY()
	float timeStamp;

	void ApplyMovementState(const FCapbotMovementState& movementState, float timeStamp);

	static FCapbotMovementState_Server Make(const FCapbotMovementState& movementState, float timeStamp);
	static FCapbotMovementState Interpolate(const FCapbotMovementState_Server& a, const FCapbotMovementState_Server& b, float time);
};
/*
bool operator>(const FCapbotMovementState_Server& a, const FCapbotMovementState_Server& b) 
{
	return a.timeStamp > b.timeStamp;
}
bool operator>=(const FCapbotMovementState_Server& a, const FCapbotMovementState_Server& b)
{
	return a.timeStamp >= b.timeStamp;
}
bool operator==(const FCapbotMovementState_Server& a, const FCapbotMovementState_Server& b)
{
	return a.timeStamp == b.timeStamp;
}
*/
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class RAYCAST_API UCapbotMovementComponent : public UPawnMovementComponent, public FLagCompensateable
{
	GENERATED_BODY()

public:	
	UCapbotMovementComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void NormalizeInput();
	void ResetInput();
	void SetEnabled(bool bNewEnabled);
	void SetMovementMode(ECapbotMovementModes newMode);

	/* 
	* FLagCompensateble 
	*/
	virtual void CompensateSeconds(float amount);
	virtual void RevertCompensation();
	virtual bool AllowCompensation() { return true; };
	virtual UWorld * GetCompensateableWorld() { return GetWorld(); };


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement capabilities")
	float acceleration = 512.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement capabilities")
	float deceleration = 512.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement capabilities")
	float maxMovementSpeed = 128.f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement capabilities")
	float jumpVelocity = 128.f;

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement properties")
	int32 maxIterations = 8;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement properties")
	float maxAcceptableOffset = 25.0f;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Capbot Movement|Movement properties")
	bool bNormalizeInput = true;

	UFUNCTION(BlueprintCallable, Category = "Capbot Movement|Input")
	void AddMoveInput(FVector input);
	UFUNCTION(BlueprintCallable, Category = "Capbot Movement|Input")
	void AddLookInput(FVector input);
	UFUNCTION(BlueprintCallable, Category = "Capbot Movement|Input")
	void DoJumpInput();
	void AddInput(FCapbotMovementInput input);

protected:

	/*
	* Performs any movement. Returns true, if super method has been executed
	*/
	virtual bool PerformMovement(const FCapbotMovementInput& input, float deltaTime);
	virtual void DefaultMove(const FCapbotMovementInput& input, float deltaTime);
	bool TracePrimitiveDefault(FHitResult& hit, FCapbotMovementState& fromState, float deltaTime);
	void ApplyMovementState(const FCapbotMovementState& newState);

	void TickServerOwner(float DeltaTime);
	void TickServerRemote(float DeltaTime, bool bSyncTimeStamp = false);
	void TickClientOwner(float DeltaTime);
	void TickClientRemote(float DeltaTime);

	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

	UPROPERTY(Transient)
	FCapbotMovementState currentMovementState;
	UPROPERTY(Transient)
	FCapbotMovementInput accumulatedInput;
	UPROPERTY(Transient)
	float clientInputTime = 0.f; 
	UPROPERTY(Transient)
	bool bInputReceived = false;
	UPROPERTY(Transient)
	float lastServerInputTimeStamp;

	UPROPERTY(Transient)
	TArray<FCapbotMovementState_Server> serverMovementSaved;
	int32 maxSavedMovementSize = 128;

	UPROPERTY(Transient)
	FCapbotMovementState_Server serverLastClientMovement;
	UPROPERTY(Transient)
	bool bClientMoveReceived = false;

	UPROPERTY(Transient)
	TArray<FCapbotMovementInput> clientInputSaved;
	int32 maxSavedInputSize = 128;

private:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Capbot Movement|Movement properties", meta = (AllowPrivateAccess = "true"))
	bool bEnabled = false;

	UPROPERTY(Transient)
	bool bPositionCorrected;

	UFUNCTION(Server, WithValidation, Unreliable)
	void ServerSendInput(FCapbotMovementInput input);
	UFUNCTION(Server, WithValidation, Unreliable)
	void ServerSendMoveResult(FCapbotMovementState result, float timeStamp);
	UFUNCTION(Client, Unreliable)
	void ClientSendMoveResult(FCapbotMovementState result);
	UFUNCTION(Client, Unreliable)
	void ClientAckGoodMove(float timeStamp);
	UFUNCTION(Client, Unreliable)
	void ClientCorrectMove(FCapbotMovementState newState, float timeStamp);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSendMoveResult(FCapbotMovementState result, FCapbotMovementInput input);
};
