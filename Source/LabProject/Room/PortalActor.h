#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectKey.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PortalActor.generated.h"

class UArrowComponent;
class APlayerCameraManager;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMovementComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class UPortalDefinition;
class USceneComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

UCLASS(Blueprintable)
class LABPROJECT_API APortalActor : public AActor
{
	GENERATED_BODY()

public:
	APortalActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Portal")
	void NativeTryInitPortalMaterial();

	UFUNCTION(BlueprintCallable, Category = "Portal")
	void NativeUpdateSceneCapture();

	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool NativeTryTeleportOverlappingActor();

	UFUNCTION(BlueprintPure, Category = "Portal")
	FVector GetPortalForward() const;

	UFUNCTION(BlueprintPure, Category = "Portal")
	FVector GetPortalPlaneLocation() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PortalPlaneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArrowComponent> ForwardDirectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> FXComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> PlayerDetectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneCaptureComponent2D> PortalCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoxComponent;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Portal")
	TObjectPtr<APortalActor> LinkedPortal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Teleport")
	TSubclassOf<AActor> TeleportableActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Teleport", meta = (ClampMin = "0.0"))
	float TeleportCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Teleport", meta = (ClampMin = "0.0"))
	float ExitOffset = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Teleport", meta = (ClampMin = "0.0"))
	float CrossingDistanceTolerance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Teleport")
	bool bTeleportOnlyOnAuthority = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture")
	bool bUpdateOnlyWhenOverlapping = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture")
	bool bCopyPlayerCameraFov = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture")
	bool bCopyPlayerCameraPostProcess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture")
	bool bEnablePortalClipPlane = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture")
	float ClipPlaneOffset = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Scene Capture", meta = (ClampMin = "0.01"))
	float InitRetryInterval = 0.1f;

	/** Soft definition referenced by BP_Portal; its asset bundle is validated before cook. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Definition",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UPortalDefinition> PortalDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Material")
	FName TextureParameterName = TEXT("Texture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Material")
	FName OffsetDistanceParameterName = TEXT("OffsetDistance");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|Material")
	float PortalEffectOffsetAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal|VFX")
	FName VortexVectorParameterName = TEXT("VortexVector");

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Portal|Runtime")
	TObjectPtr<UMaterialInstanceDynamic> PortalMat;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Portal|Runtime")
	TObjectPtr<UTextureRenderTarget2D> PortalRT;

private:
	struct FPortalTraversalState
	{
		FVector LastPosition = FVector::ZeroVector;
		bool bLastInFront = false;
		bool bInitialized = false;
		double LastTeleportTime = -BIG_NUMBER;
	};

	UFUNCTION()
	void HandlePortalBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePortalEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool EnsureRenderTargetSize();
	FIntPoint GetDesiredRenderTargetSize() const;
	bool ApplyPortalDefinition();
	bool IsCaptureRateLimitElapsed();
	void ConfigureLinkedCaptureComponent() const;
	void UpdatePortalVisualParameters() const;
	void SetTickEnabledFromOverlaps();
	void ResolvePortalComponents() const;
	void SeedTeleportOverlapCache(UPrimitiveComponent* OverlapComponent, TArray<TWeakObjectPtr<AActor>>& OutActors);
	bool TrackTeleportOverlap(TArray<TWeakObjectPtr<AActor>>& OverlappingActors, AActor* Actor) const;
	void UntrackTeleportOverlap(
		TArray<TWeakObjectPtr<AActor>>& OverlappingActors,
		AActor* Actor,
		const UPrimitiveComponent* OverlapComponent) const;
	bool HasTrackedTeleportOverlap(
		TArray<TWeakObjectPtr<AActor>>& OverlappingActors,
		const UPrimitiveComponent* OverlapComponent) const;
	void RemoveTraversalStateIfNoLongerOverlapping(AActor* Actor);

	bool IsTeleportCandidate(const AActor* Actor) const;
	void ResolveOverlappingTeleportActors(TArray<TWeakObjectPtr<AActor>>& OutActors);
	APortalActor* GetLinkedPortalActor() const;
	APlayerCameraManager* GetCachedPlayerCameraManager() const;
	float GetBlueprintOffsetAmount() const;
	UStaticMeshComponent* GetPortalPlaneComponent() const;
	UBoxComponent* GetBoxComponent() const;
	UBoxComponent* GetPlayerDetectionComponent() const;
	UArrowComponent* GetForwardDirectionComponent() const;
	USceneCaptureComponent2D* GetPortalCameraComponent() const;
	UNiagaraComponent* GetFXComponent() const;
	bool IsPointCrossingPortal(AActor* Actor, const FVector& Point);
	void TeleportActorThroughPortal(AActor* Actor);
	void PrimeTraversalState(AActor* Actor);

	FVector TransformLocationToLinkedPortal(const FVector& WorldLocation) const;
	FVector TransformDirectionToLinkedPortal(const FVector& WorldDirection) const;
	FRotator TransformRotationToLinkedPortal(const FRotator& WorldRotation) const;
	FVector TransformVelocityToLinkedPortal(const FVector& WorldVelocity) const;
	FTransform GetPortalReferenceTransform() const;

	FTimerHandle InitMaterialTimerHandle;
	UPROPERTY(Transient)
	TObjectPtr<UPortalDefinition> LoadedPortalDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PortalMaterialParent;

	double LastSceneCaptureTime = -BIG_NUMBER;
	TMap<TObjectKey<AActor>, FPortalTraversalState> TraversalStates;
	TArray<TWeakObjectPtr<AActor>> PortalOverlappingTeleportActors;
	TArray<TWeakObjectPtr<AActor>> DetectedTeleportActors;
	mutable TWeakObjectPtr<APlayerCameraManager> CachedPlayerCameraManager;
	mutable TWeakObjectPtr<UStaticMeshComponent> CachedPortalPlane;
	mutable TWeakObjectPtr<UBoxComponent> CachedBox;
	mutable TWeakObjectPtr<UBoxComponent> CachedPlayerDetection;
	mutable TWeakObjectPtr<UArrowComponent> CachedForwardDirection;
	mutable TWeakObjectPtr<USceneCaptureComponent2D> CachedPortalCamera;
	mutable TWeakObjectPtr<UNiagaraComponent> CachedFX;
};
