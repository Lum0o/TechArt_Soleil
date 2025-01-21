#include "LuminescentObject.h"

#include "Engine/Canvas.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

ALuminescentObject::ALuminescentObject()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALuminescentObject::BeginPlay()
{
	Super::BeginPlay();

	MeshComponent = GetComponentByClass<UStaticMeshComponent>();
	if (!MeshComponent)
		return;

	MeshComponent->OnComponentHit.AddDynamic(this, &ALuminescentObject::OnHit);
	Material = MeshComponent->CreateDynamicMaterialInstance(0, LuminescentMaterial);

	// Set initial propagation speed value
	Material->SetScalarParameterValue(TEXT("PropagationSpeed"), PropagationSpeed);

	// Set max propagation distance
	Material->SetScalarParameterValue(TEXT("MaxPropagationDistance"), PropagationDistance);

	// Set brightness
	Material->SetScalarParameterValue(TEXT("Brightness"), IntensityRatio);

	// Compute the time it will take to finish the propagation
	TotalPropagationTime = PropagationDistance / PropagationSpeed;

	// Ratio between the total propagation time, and the fade out duration
	FadeOutTimeRatio = TotalPropagationTime / FadeOutDuration;

	SetupRenderTarget();
}

void ALuminescentObject::Tick(const float DeltaTime)
{
	UE_LOG(LogTemp, Display, TEXT("This shouldn't exist"));
	Super::Tick(DeltaTime);

	if (!Material)
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
	
	Material->SetTextureParameterValue("PointsArray", PointsTexture);
	Material->SetTextureParameterValue("TimesArray", TimesTexture);
}

void ALuminescentObject::OnHit(
	UPrimitiveComponent* const,
	AActor* const OtherActor,
	UPrimitiveComponent* const,
	const FVector,
	const FHitResult& Hit
)
{
	if (IgnoreCollision)
		return;
	
	FVector BodyPoint;
	MeshComponent->GetClosestPointOnCollision(Hit.Location, BodyPoint);

	const float MaxRange = OtherActor->GetTransform().GetTranslation().Length() * IntensityRatio;
	
	TryStartPropagation(BodyPoint, MaxRange);
	IgnoreCollision = true;

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
		[this]() -> void { IgnoreCollision = false; },
	IgnoreCollisionTimer, false);

	TArray<AActor*> LuminescentObjects;
	
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectType;
	ObjectType.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic));
	ObjectType.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_PhysicsBody));
	UKismetSystemLibrary::SphereOverlapActors(this, BodyPoint, MaxRange, ObjectType, StaticClass(), {this}, LuminescentObjects);

	for (AActor* const Actor : LuminescentObjects)
	{
		ALuminescentObject* const LuminescentObject = Cast<ALuminescentObject>(Actor);
		LuminescentObject->AddPropagationPoint(BodyPoint, MaxRange);
	}

}

void ALuminescentObject::SetupRenderTarget()
{
	// Allocate a texture big enough to hold our max number of points
	PointsTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, MaxNumberPropagationPoints, 1, RTF_RGBA32f); 
	TimesTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, MaxNumberPropagationPoints, 1, RTF_RGBA32f);
}

void ALuminescentObject::SendPointsToShader()
{
	SendToShader(PointsTexture, [](const FPropagationPointStatus& Point) -> FLinearColor
	{
		constexpr FLinearColor ColorEmpty = FLinearColor(-1.f, -1.f, -1.f, -1.f);

		return Point.Stage != EPropagationStage::Inactive
			? FLinearColor(Point.HitPoint.X, Point.HitPoint.Y, Point.HitPoint.Z, 1.0f)
			: ColorEmpty;
	});
}

void ALuminescentObject::SendTimesToShader()
{
	SendToShader(TimesTexture, [](const FPropagationPointStatus& Point) -> FLinearColor
	{
		constexpr FLinearColor ColorEmpty = FLinearColor(-1.f, -1.f, -1.f, -1.f);

		return Point.Stage != EPropagationStage::Inactive
			? FLinearColor(Point.TimeToSend, Point.FadeOutIntensity, Point.PropagationDistance, 0.0f)
			: ColorEmpty;
	});
}

void ALuminescentObject::SendToShader(UTextureRenderTarget2D* const Texture, const std::function<FLinearColor(const FPropagationPointStatus&)>& Lambda)
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

void ALuminescentObject::AddPropagationPoint(const FVector& Point, const float MaxRange)
{
	TryStartPropagation(Point, MaxRange);
	IgnoreCollision = true;

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
		[this]() -> void { IgnoreCollision = false; },
	IgnoreCollisionTimer, false);
}

void ALuminescentObject::TryStartPropagation(const FVector& StartPoint, const float MaxRange)
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

void ALuminescentObject::SetupPropagationPoint(const FVector& StartPoint, FPropagationPointStatus& Point, const float MaxRange) const
{
	Point.Stage = EPropagationStage::Active;
	Point.PropagationTime = 0.f;
	Point.HitPoint = StartPoint;
	Point.PropagationDistance = MaxRange;
}

void ALuminescentObject::ProcessPropagation(FPropagationPointStatus& Point, const float DeltaTime) const
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

void ALuminescentObject::ProcessFadeOut(FPropagationPointStatus& Point, const float DeltaTime) const
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
