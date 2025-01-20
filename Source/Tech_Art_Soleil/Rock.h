// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Rock.generated.h"

UCLASS()
class TECH_ART_SOLEIL_API ARock : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARock();

	void Throw(float ThrowForce);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	UPROPERTY(VisibleAnywhere)
	UProjectileMovementComponent* MovementComponent;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;
	
	// Whether the rock immediately gets destroyed upon making contact with a surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters)
	bool DestroyOnContact = false;

	// How long the rock lives after making contact (DestroyOnContact must be set to false)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters)
	float LifeDuration = 3.f;
};
