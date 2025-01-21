// Fill out your copyright notice in the Description page of Project Settings.


#include "ABioluminescentManager.h"

#include "Landscape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

ABioluminescentManager::ABioluminescentManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ABioluminescentManager::BeginPlay()
{
	Super::BeginPlay();


	//Finding all materials in the scene and create a dynamic instance of them
	//First the mushrooms
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), MushroomClass, Actors);
	for (auto Actor : Actors)
	{
		UStaticMeshComponent* StaticMesh = Actor->GetComponentByClass<UStaticMeshComponent>();
		StaticMesh->OnComponentHit.AddDynamic(this, &ABioluminescentManager::OnHit);

		for(int i = 0; i < StaticMesh->GetNumMaterials(); i++)
			Materials.Add(StaticMesh->CreateDynamicMaterialInstance(i, StaticMesh->GetMaterial(i)));
	}

	//Then we are looking for the rocks
	Actors.Empty();
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AStaticMeshActor::StaticClass(), Actors);
	for (auto Actor : Actors)
	{
		UStaticMeshComponent* StaticMesh = Actor->GetComponentByClass<UStaticMeshComponent>();
		StaticMesh->OnComponentHit.AddDynamic(this, &ABioluminescentManager::OnHit);

		for(int i = 0; i < StaticMesh->GetNumMaterials(); i++)
			Materials.Add(StaticMesh->CreateDynamicMaterialInstance(i, StaticMesh->GetMaterial(i)));
	}

	//The Landscape
	AActor* const Landscape = UGameplayStatics::GetActorOfClass(GetWorld(), ALandscape::StaticClass());
	for (auto ULandscapeComponent : Cast<ALandscape>(Landscape)->LandscapeComponents)
	{
		ULandscapeComponent->OnComponentHit.AddDynamic(this, &ABioluminescentManager::OnHit);
		for(int i = 0; i < ULandscapeComponent->GetNumMaterials(); i++)
			Materials.Add(ULandscapeComponent->CreateDynamicMaterialInstance(i, ULandscapeComponent->GetMaterial(i)));
	} 

	//The foliage
	AActor* const FoliageActor = UGameplayStatics::GetActorOfClass(GetWorld(), AInstancedFoliageActor::StaticClass());
	
	TArray<UFoliageInstancedStaticMeshComponent*> FoliageComponents;
	FoliageActor->GetComponents<UFoliageInstancedStaticMeshComponent>(FoliageComponents);

	for (auto FoliageComponent : FoliageComponents)
	{
		FoliageComponent->OnComponentHit.AddDynamic(this, &ABioluminescentManager::OnHit);

		for(int i = 0; i < FoliageComponent->GetNumMaterials(); i++)
			Materials.Add(FoliageComponent->CreateDynamicMaterialInstance(i, FoliageComponent->GetMaterial(i)));
	}

	UE_LOG(LogTemp, Display, TEXT("FoliageCOmponents : %d"), FoliageComponents.Num());

	//The Player
	UGameplayStatics::GetPlayerPawn(GetWorld(), 0)->GetComponentByClass<UCapsuleComponent>()->OnComponentHit.AddDynamic(this, &ABioluminescentManager::OnHit);
	
	for (auto Material : Materials)
	{
		// Set initial propagation speed value
		Material->SetScalarParameterValue(TEXT("PropagationSpeed"), PropagationSpeed);
	}

	// Compute the time it will take to finish the propagation
	TotalPropagationTime = PropagationDistance / PropagationSpeed;

	// Ratio between the total propagation time, and the fade out duration
	FadeOutTimeRatio = TotalPropagationTime / FadeOutDuration;

	SetupRenderTarget();
}

void ABioluminescentManager::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	AActor* const Landscape = UGameplayStatics::GetActorOfClass(GetWorld(), ALandscape::StaticClass());
	//UE_LOG(LogTemp, Display, TEXT("Landscape : %d"),Cast<ALandscape>(Landscape)->LandscapeComponents[0]->IsGrassMapOutdated());
	
	if (Materials.Num() == 0)
	{
		// Don't want to do anything if the material isn't valid
		return;
	}
	
	for (size_t i = 0; i < PropagationPoints.size(); i++)
	{
		FPropagationPointStatus& p = PropagationPoints[i];
		//UE_LOG(LogTemp, Display, TEXT("Point %llu: Stage : %lld | Timer : %f"),i,p.Stage, p.PropagationTime);
		// Easing functions from https://easings.net/en

		// First update the propagation point
		switch (p.Stage)
		{
			case EPropagationStage::Inactive:
				break;

			case EPropagationStage::Active:
				ProcessPropagation(p, DeltaTime);
				break;

			case EPropagationStage::WaitingForFadeOut:
				p.FadeOutTimer -= DeltaTime;
				if (p.FadeOutTimer <= 0)
					p.Stage = EPropagationStage::FadeOut;
				break;

			case EPropagationStage::FadeOut:
				ProcessFadeOut(p, DeltaTime);
		}

		//UE_LOG(LogTemp, Display, TEXT("Point %d: time : %f Stage : %lld (%f, %f, %f)"), i, p.TimeToSend, p.Stage, p.HitPoint.X, p.HitPoint.Y, p.HitPoint.Z);
	}

	// Send data to the textures
	SendPointsToShader();
	SendTimesToShader();

	for (auto Material : Materials)
	{
		Material->SetTextureParameterValue("PointsArray", PointsTexture);
		Material->SetTextureParameterValue("TimesArray", TimesTexture);
	}
}

void ABioluminescentManager::OnHit(
	UPrimitiveComponent* const,
	AActor* const OtherActor,
	UPrimitiveComponent* const,
	const FVector,
	const FHitResult& Hit
)
{
	if (IgnoreCollision)
		return;

	UE_LOG(LogTemp, Display, TEXT("Hit"));
	
	FVector BodyPoint = Hit.Location;

	const float MaxRange = OtherActor->GetTransform().GetTranslation().Length() * IntensityRatio;
	
	TryStartPropagation(BodyPoint, MaxRange);
	IgnoreCollision = true;

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
		[this]() -> void { IgnoreCollision = false; },
	IgnoreCollisionTimer, false);
}

void ABioluminescentManager::SetupRenderTarget()
{
	// Allocate a texture big enough to hold our max number of points
	PointsTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, MaxNumberPropagationPoints, 1, RTF_RGBA32f); 
	TimesTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, MaxNumberPropagationPoints, 1, RTF_RGBA32f);
}

void ABioluminescentManager::SendPointsToShader()
{
	SendToShader(PointsTexture, [](const FPropagationPointStatus& Point) -> FLinearColor
	{
		constexpr FLinearColor ColorEmpty = FLinearColor(-1.f, -1.f, -1.f, -1.f);

		return Point.Stage != EPropagationStage::Inactive
			? FLinearColor(Point.HitPoint.X, Point.HitPoint.Y, Point.HitPoint.Z, 1.0f)
			: ColorEmpty;
	});
}

void ABioluminescentManager::SendTimesToShader()
{
	SendToShader(TimesTexture, [](const FPropagationPointStatus& Point) -> FLinearColor
	{
		constexpr FLinearColor ColorEmpty = FLinearColor(-1.f, -1.f, -1.f, -1.f);

		return Point.Stage != EPropagationStage::Inactive
			? FLinearColor(Point.TimeToSend, Point.FadeOutIntensity, Point.PropagationDistance, 0.0f)
			: ColorEmpty;
	});
}

void ABioluminescentManager::SendToShader(UTextureRenderTarget2D* const Texture, const std::function<FLinearColor(const FPropagationPointStatus&)>& Lambda)
{
	FVector2D _;
	FDrawToRenderTargetContext Context;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Texture, TimesCanvas, _, Context);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Texture);
	
	for (size_t i = 0; i < PropagationPoints.size(); i++)
	{
		const FPropagationPointStatus& p = PropagationPoints[i];

		if (p.Stage != EPropagationStage::Inactive)
		{
			// Get result data
			const FLinearColor Data = Lambda(p);

			// Write color to texture
			TimesCanvas->K2_DrawBox(FVector2d(i + 0.5f, 0.f), FVector2D::One(), 1.f , Data);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void ABioluminescentManager::TryStartPropagation(const FVector& StartPoint, const float MaxRange)
{
	for (size_t i = 0; i < PropagationPoints.size(); i++)
	{
		if (PropagationPoints[i].Stage == EPropagationStage::Inactive)
		{
			SetupPropagationPoint(StartPoint, PropagationPoints[i], MaxRange);
			break;
		}
	}
}

void ABioluminescentManager::SetupPropagationPoint(const FVector& StartPoint, FPropagationPointStatus& Point, const float MaxRange) const
{
	Point.Stage = EPropagationStage::Active;
	Point.PropagationTime = 0.f;
	Point.HitPoint = StartPoint;
	Point.PropagationDistance = MaxRange;
}

void ABioluminescentManager::ProcessPropagation(FPropagationPointStatus& Point, const float DeltaTime) const
{
	Point.PropagationTime += DeltaTime;

	const float TimeRate = Point.PropagationTime / TotalPropagationTime;
	Point.TimeToSend = UKismetMathLibrary::Ease(0, TotalPropagationTime, TimeRate, EEasingFunc::EaseOut, 3);

	// Hacky fix to the long "pause" at the end due to the values very slowly reaching the max
	// This "interrupts" the fade and jumps straight to the end, ignoring the very subtle changes
	if (Point.TimeToSend >= TotalPropagationTime * .99f)
	{
		Point.PropagationTime = Point.TimeToSend;
		Point.PropagationEndTime = Point.TimeToSend;

		if (FadeOutDelay > 0.f)
		{
			// A fade out timer is necessary, so set up an unreal timer
			Point.FadeOutTimer = FadeOutDelay;
			Point.Stage = EPropagationStage::WaitingForFadeOut;
		}
		else
		{
			// Otherwise, just start the fade out now
			Point.Stage = EPropagationStage::FadeOut;
		}
	}
}

void ABioluminescentManager::ProcessFadeOut(FPropagationPointStatus& Point, const float DeltaTime) const
{
	// The fade out is done by simply doing the propagation in reverse order
	Point.PropagationTime += DeltaTime;
	
	Point.FadeOutIntensity = FMath::Clamp( (Point.PropagationTime - TotalPropagationTime) / FadeOutDuration, 0, 1);

	if (Point.PropagationTime >= TotalPropagationTime + FadeOutDuration)
	{
		Point.PropagationTime = 0.0f;
		Point.FadeOutIntensity = 0.0f;
		Point.TimeToSend = 0.0f;
		Point.Stage = EPropagationStage::Inactive;
	}
}
