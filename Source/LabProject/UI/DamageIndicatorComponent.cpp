#include "UI/DamageIndicatorComponent.h"

#include "Character/PdCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/DamageIndicatorActor.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageIndicatorComponent)

UDamageIndicatorComponent::UDamageIndicatorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UDamageIndicatorComponent::ShowDamageIndicator(float DamageAmount, FVector WorldLocation, bool bCriticalHit)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !DamageIndicatorActorClass)
	{
		return;
	}

	const float DisplayDamageAmount = FMath::Max(DamageAmount, 0.0f);

	FVector CameraUp = FVector::UpVector;
	FVector CameraRight = GetOwner() ? GetOwner()->GetActorRightVector() : FVector::RightVector;
	ResolveCameraAxes(CameraUp, CameraRight);

	const FVector StartLocation =
		WorldLocation
		+ CameraUp * InitialCameraUpOffset
		+ CameraRight * InitialCameraRightOffset;

	const float EndUpOffset = FMath::FRandRange(EndCameraUpOffsetMin, EndCameraUpOffsetMax);
	const float EndRightOffset = FMath::FRandRange(EndCameraRightOffsetMin, EndCameraRightOffsetMax);
	const FVector EndLocation =
		StartLocation
		+ CameraUp * EndUpOffset
		+ CameraRight * EndRightOffset;

	const FTransform SpawnTransform(FRotator::ZeroRotator, StartLocation);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.Instigator = Cast<APawn>(GetOwner());
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADamageIndicatorActor* DamageIndicatorActor =
		World->SpawnActorDeferred<ADamageIndicatorActor>(DamageIndicatorActorClass, SpawnTransform, SpawnParameters.Owner, SpawnParameters.Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!DamageIndicatorActor)
	{
		return;
	}

	FDamageIndicatorPayload Payload;
	Payload.DamageAmount = DisplayDamageAmount;
	Payload.bCriticalHit = bCriticalHit;
	Payload.StartLocation = StartLocation;
	Payload.EndLocation = EndLocation;
	Payload.DamagedActor = GetOwner();

	DamageIndicatorActor->InitializeDamageIndicator(Payload);
	DamageIndicatorActor->FinishSpawning(SpawnTransform);
}

FVector UDamageIndicatorComponent::ResolveDamageIndicatorWorldLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	const APdCharacterBase* CharacterOwner = Cast<APdCharacterBase>(Owner);
	const USkeletalMeshComponent* MeshComponent = CharacterOwner ? CharacterOwner->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (MeshComponent && !AnchorSocketName.IsNone() && MeshComponent->DoesSocketExist(AnchorSocketName))
	{
		return MeshComponent->GetSocketLocation(AnchorSocketName);
	}

	if (const UCapsuleComponent* CapsuleComponent = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : Owner->FindComponentByClass<UCapsuleComponent>())
	{
		return Owner->GetActorLocation() + FVector(0.0f, 0.0f, CapsuleComponent->GetScaledCapsuleHalfHeight() + 40.0f);
	}

	return Owner->GetActorLocation() + FallbackWorldOffset;
}

void UDamageIndicatorComponent::SetDamageIndicatorActorClass(TSubclassOf<ADamageIndicatorActor> InActorClass)
{
	DamageIndicatorActorClass = InActorClass;
}

bool UDamageIndicatorComponent::ResolveCameraAxes(FVector& OutCameraUp, FVector& OutCameraRight) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FRotationMatrix CameraRotationMatrix(CameraRotation);
	OutCameraUp = CameraRotationMatrix.GetUnitAxis(EAxis::Z);
	OutCameraRight = CameraRotationMatrix.GetUnitAxis(EAxis::Y);
	return true;
}
