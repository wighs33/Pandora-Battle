#include "Component/Player/PlayerInteractionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerInteractionComponent)

namespace
{
	void ConfigureInteractionSensorCollision(UPrimitiveComponent& InteractionSensor)
	{
		InteractionSensor.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		// Interaction sensors have their own object channel. Treating this volume as
		// WorldDynamic makes projectile and skill traces stop in front of the
		// character before they can reach the damage mesh.
		InteractionSensor.SetCollisionObjectType(ECC_GameTraceChannel3); // OverlapBox
		InteractionSensor.SetCollisionResponseToAllChannels(ECR_Overlap);
		InteractionSensor.SetCollisionResponseToChannel(
			ECC_GameTraceChannel2,
			ECR_Ignore); // ArrowProjectile / native skill projectiles
		InteractionSensor.SetGenerateOverlapEvents(true);
		InteractionSensor.SetCanEverAffectNavigation(false);
	}
}

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	InitBoxExtent(FVector(50.0f, 50.0f, 100.0f));
	SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	SetRelativeRotation(FRotator::ZeroRotator);
	ConfigureInteractionSensorCollision(*this);

	OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
}

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Re-apply after Blueprint component defaults have been deserialized so old
	// player Blueprint assets cannot restore the legacy WorldDynamic setting.
	ConfigureInteractionSensorCollision(*this);
}

void UPlayerInteractionComponent::ApplySettings(const FPlayerInteractionSettings& Settings)
{
	const FVector SafeExtent(
		FMath::Max(FMath::Abs(Settings.BoxExtent.X), 1.0f),
		FMath::Max(FMath::Abs(Settings.BoxExtent.Y), 1.0f),
		FMath::Max(FMath::Abs(Settings.BoxExtent.Z), 1.0f));
	SetBoxExtent(SafeExtent, true);
	SetRelativeLocation(Settings.RelativeLocation);
	SetRelativeRotation(Settings.RelativeRotation);
	ServerValidationDistance = FMath::Max(0.0f, Settings.ServerValidationDistance);
}

bool UPlayerInteractionComponent::HasCurrentInteractActors(
	TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	const APdPlayer* Player = GetPlayerOwner();
	OutCurrentInteractActors = Player
		? Player->CurrentInteractActors
		: TArray<TScriptInterface<IInteractableInterface>>();
	return !OutCurrentInteractActors.IsEmpty();
}

AActor* UPlayerInteractionComponent::GetCurrentInteractActor() const
{
	const APdPlayer* Player = GetPlayerOwner();
	if (!Player)
	{
		return nullptr;
	}

	for (const TScriptInterface<IInteractableInterface>& Entry : Player->CurrentInteractActors)
	{
		AActor* InteractableActor = Cast<AActor>(Entry.GetObject());
		if (CanInteractWithActor(InteractableActor))
		{
			return InteractableActor;
		}
	}

	return nullptr;
}

bool UPlayerInteractionComponent::InteractWithCurrentTarget()
{
	APdPlayer* Player = GetPlayerOwner();
	AActor* InteractableActor = GetCurrentInteractActor();
	if (!Player
		|| !IsValid(InteractableActor)
		|| !InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass())
		|| !IInteractableInterface::Execute_CanInteract(InteractableActor, Player))
	{
		return false;
	}

	return IInteractableInterface::Execute_Interact(InteractableActor, Player);
}

bool UPlayerInteractionComponent::CanInteractWithActor(AActor* InteractableActor) const
{
	const APdPlayer* Player = GetPlayerOwner();
	TScriptInterface<IInteractableInterface> InteractableEntry;
	if (!Player || !TryMakeInteractableEntry(InteractableActor, InteractableEntry))
	{
		return false;
	}

	if (IsOverlappingActor(InteractableActor))
	{
		return true;
	}

	if (ServerValidationDistance <= 0.0f)
	{
		return false;
	}

	return FVector::DistSquared(Player->GetActorLocation(), InteractableActor->GetActorLocation())
		<= FMath::Square(ServerValidationDistance);
}

void UPlayerInteractionComponent::PlayInteractionMontage(UAnimMontage* Montage, const float PlayRate)
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player || !Montage)
	{
		return;
	}

	const float SafePlayRate = PlayRate > 0.0f ? PlayRate : 1.0f;
	if (Player->HasAuthority())
	{
		MulticastPlayInteractionMontage(Montage, SafePlayRate);
		return;
	}

	if (Player->PlayAnimMontage(Montage, SafePlayRate) > 0.0f)
	{
		ActiveInteractionMontage = Montage;
	}
}

void UPlayerInteractionComponent::MulticastPlayInteractionMontage_Implementation(
	UAnimMontage* Montage,
	const float PlayRate)
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player || !Montage)
	{
		return;
	}

	const float SafePlayRate = PlayRate > 0.0f ? PlayRate : 1.0f;
	if (Player->PlayAnimMontage(Montage, SafePlayRate) > 0.0f)
	{
		ActiveInteractionMontage = Montage;
	}
}

void UPlayerInteractionComponent::StopInteractionMontage(const float BlendOutTime)
{
	APdPlayer* Player = GetPlayerOwner();
	if (!Player)
	{
		return;
	}

	const float SafeBlendOutTime = FMath::Max(0.0f, BlendOutTime);
	if (!IsInteractionMontagePlaying())
	{
		ActiveInteractionMontage = nullptr;
		return;
	}

	if (Player->HasAuthority())
	{
		MulticastStopInteractionMontage(SafeBlendOutTime);
		return;
	}

	StopInteractionMontageLocally(SafeBlendOutTime);
	ServerStopInteractionMontage(SafeBlendOutTime);
}

bool UPlayerInteractionComponent::IsInteractionMontagePlaying() const
{
	const APdPlayer* Player = GetPlayerOwner();
	if (!Player || !ActiveInteractionMontage)
	{
		return false;
	}

	const USkeletalMeshComponent* MeshComponent = Player->GetMesh();
	const UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	return AnimInstance && AnimInstance->Montage_IsPlaying(ActiveInteractionMontage);
}

void UPlayerInteractionComponent::ServerStopInteractionMontage_Implementation(const float BlendOutTime)
{
	MulticastStopInteractionMontage(FMath::Max(0.0f, BlendOutTime));
}

void UPlayerInteractionComponent::MulticastStopInteractionMontage_Implementation(const float BlendOutTime)
{
	StopInteractionMontageLocally(FMath::Max(0.0f, BlendOutTime));
}

APdPlayer* UPlayerInteractionComponent::GetPlayerOwner() const
{
	return Cast<APdPlayer>(GetOwner());
}

bool UPlayerInteractionComponent::TryMakeInteractableEntry(
	AActor* OtherActor,
	TScriptInterface<IInteractableInterface>& OutInteractableActor) const
{
	const APdPlayer* Player = GetPlayerOwner();
	if (!Player
		|| !IsValid(OtherActor)
		|| OtherActor == Player
		|| !OtherActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	OutInteractableActor.SetObject(OtherActor);
	OutInteractableActor.SetInterface(Cast<IInteractableInterface>(OtherActor));
	return true;
}

bool UPlayerInteractionComponent::StopInteractionMontageLocally(const float BlendOutTime)
{
	APdPlayer* Player = GetPlayerOwner();
	UAnimMontage* MontageToStop = ActiveInteractionMontage.Get();
	if (!Player || !MontageToStop)
	{
		return false;
	}

	USkeletalMeshComponent* MeshComponent = Player->GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		ActiveInteractionMontage = nullptr;
		return false;
	}

	AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);
	ActiveInteractionMontage = nullptr;
	return true;
}

void UPlayerInteractionComponent::HandleBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	APdPlayer* Player = GetPlayerOwner();
	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!Player || !TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	const bool bAlreadyTracked = Player->CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (!bAlreadyTracked)
	{
		Player->CurrentInteractActors.Add(InteractableActor);
	}
}

void UPlayerInteractionComponent::HandleEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	APdPlayer* Player = GetPlayerOwner();
	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!Player || !TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	Player->CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});
}
