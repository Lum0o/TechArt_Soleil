#pragma once

#include <array>
#include <functional>

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Runtime/Foliage/Public/InstancedFoliageActor.h"
#include "ABioluminescentManager.generated.h"

/**
 * 
 */
UCLASS()
class TECH_ART_SOLEIL_API ABioluminescentManager : public AActor
{

	GENERATED_BODY()
private:
	enum class EPropagationStage final : size_t
	{
		// No propagation currently active
		Inactive,
		// Currently propagating
		Active,
		// Delay before the fade out
		WaitingForFadeOut,
		// Currently fading out
		FadeOut
	};

	struct FPropagationPointStatus final
	{
		// Propagation stage
		EPropagationStage Stage = EPropagationStage::Inactive;

		// Where the hit point was
		FVector HitPoint;

		// Elapsed time during the propagation
		float PropagationTime = 0.f;
		float PropagationEndTime = 0.f;
		float TimeToSend;
		float FadeOutTimer;
		float FadeOutIntensity;
		float PropagationDistance;
	};

	protected:
	virtual void BeginPlay() override;

public:
	ABioluminescentManager();
	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	static constexpr size_t MaxNumberPropagationPoints = 10;

	UPROPERTY(EditAnywhere)
	UClass* MushroomClass;

	// Ignores collision for a certain amount of time after one happened
	UPROPERTY(EditAnywhere)
	float IgnoreCollisionTimer = 0.1f;
	
	// How far the bioluminescence will propagate
	UPROPERTY(EditAnywhere)
	float PropagationDistance = 600.f;

	// The speed at which the bioluminescence propagates
	UPROPERTY(EditAnywhere)
	float PropagationSpeed = 150.f;

	// The intensity ratio of the light
	UPROPERTY(EditAnywhere)
	float IntensityRatio = 1.f;

	// Delay before the fade out, in seconds
	UPROPERTY(EditAnywhere)
	float FadeOutDelay = 0.f;

	// Duration of the fade out after the propagation, in seconds
	UPROPERTY(EditAnywhere)
	float FadeOutDuration = 1.f;

	UPROPERTY(BlueprintReadWrite)
	TArray<FVector> ConcernedVertices;

private:

	
	void SetupRenderTarget();
	void SendPointsToShader();
	void SendTimesToShader();
	void SendToShader(UTextureRenderTarget2D* Texture, const std::function<FLinearColor(const FPropagationPointStatus&)>& Lambda);
	
	void TryStartPropagation(const FVector& StartPoint, const float MaxRange);
	void SetupPropagationPoint(const FVector& StartPoint, FPropagationPointStatus& Point, const float MaxRange) const;

	void ProcessPropagation(FPropagationPointStatus& Point, float DeltaTime) const;
	void ProcessFadeOut(FPropagationPointStatus& Point, float DeltaTime) const;

	// Array of propagation points, fixed length
	std::array<FPropagationPointStatus, MaxNumberPropagationPoints> PropagationPoints = {};

	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> Materials = {};
	
	// Texture holding the points coordinates, this is sent to the shader
	UPROPERTY()
	UTextureRenderTarget2D* PointsTexture = nullptr;

	// Texture holding the time of each point, this is sent to the shader
	UPROPERTY()
	UTextureRenderTarget2D* TimesTexture = nullptr;

	// Canvas to draw to the texture
	UPROPERTY()
	UCanvas* PointsCanvas = nullptr;

	UPROPERTY()
	UCanvas* TimesCanvas = nullptr;

	// The total time needed to finish the propagation, based on the distance and speed
	float TotalPropagationTime = 0.f;

	// Time ratio to modify the delta time when fading out in order to make it slower or faster
	float FadeOutTimeRatio = 1.f;

	bool IgnoreCollision = false;
	
};
