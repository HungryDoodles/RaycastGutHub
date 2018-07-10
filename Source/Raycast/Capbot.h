#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Pawn.h"
#include "Capbot.generated.h"

class UCapbotMovementComponent;
class UCapsuleComponent;
class UCameraComponent;

UCLASS(Blueprintable, BlueprintType)
class RAYCAST_API ACapbot : public APawn 
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

public:
	ACapbot();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController * newController) override;
	virtual void UnPossessed() override;

	UCapbotMovementComponent * GetCapbotMovementComponent() { return capbotMovement; }
	UCapsuleComponent * GetCapsuleComponent() { return movementComponent; }

	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void ForwardInput(float amount);
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void RightInput(float amount);
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void MoveInput(FVector inputVector);
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void JumpInput();
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void LookRightInput(float amount);
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void LookUpInput(float amount);
	UFUNCTION(BlueprintCallable, Category = "Capbot|Input")
	void LookInput(FVector input);

private:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, meta=(AllowPrivateAccess="true"))
	UCapbotMovementComponent * capbotMovement; 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, meta=(AllowPrivateAccess = "true"))
	UCapsuleComponent * movementComponent;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, meta=(AllowPrivateAccess = "true"))
	UCameraComponent * cameraComponent;

	float cameraVerticalAngle;
};