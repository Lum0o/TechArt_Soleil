// Fill out your copyright notice in the Description page of Project Settings.


#include "Rock.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ARock::ARock()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	SetRootComponent(Mesh);
	
	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->Deactivate();
}
// Called when the game starts or when spawned
void ARock::BeginPlay()
{
	Super::BeginPlay();
}

void ARock::Throw(const float ThrowForce)
{
	MovementComponent->Activate();
    const FVector Force = GetActorForwardVector() * ThrowForce;
    MovementComponent->AddForce(Force);
	UE_LOG(LogTemp, Display, TEXT("Rock : %f ; %f ; %f"), Force.X, Force.Y, Force.Z);
}

