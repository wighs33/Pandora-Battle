#include "Component/Player/PlayerInteractionComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Common/CollisionChannels.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerInteractionComponent)

namespace
{
	void ConfigureInteractionSensorCollision(UPrimitiveComponent& InteractionSensor)
	{
		InteractionSensor.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		// 상호작용 센서가 피격 메시보다 먼저 투사체·스킬 검사를 막지 않도록 전용 채널을 쓴다.
		InteractionSensor.SetCollisionObjectType(LabCollisionChannels::OverlapBox());
		InteractionSensor.SetCollisionResponseToAllChannels(ECR_Overlap);
		InteractionSensor.SetCollisionResponseToChannel(
			LabCollisionChannels::Projectile(),
			ECR_Ignore);
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

	// Blueprint에 저장된 과거 충돌 설정이 스킬 투사체를 막지 않도록 역직렬화 후 보정한다.
	ConfigureInteractionSensorCollision(*this);
}

// 월드 종료나 컴포넌트 제거 이후에는 이전 상호작용 대상을 보관하지 않는다.
void UPlayerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CurrentInteractActors.Reset();
	ActiveInteractionMontage = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UPlayerInteractionComponent::ApplySettings(const FPlayerInteractionSettings& Settings)
{
	ServerValidationDistance = FMath::Max(0.0f, Settings.ServerValidationDistance);
}

AActor* UPlayerInteractionComponent::GetCurrentInteractActor() const
{
	const APdPlayer* Player = GetPlayerOwner();
	if (!Player)
	{
		return nullptr;
	}

	for (const TScriptInterface<IInteractableInterface>& Entry : CurrentInteractActors)
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

	const bool bAlreadyTracked = CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (!bAlreadyTracked)
	{
		CurrentInteractActors.Add(InteractableActor);
	}
}

void UPlayerInteractionComponent::HandleEndOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/)
{
	// 다른 충돌 부위가 센서 안에 남아 있으면 같은 액터를 목록에서 빼지 않는다.
	if (IsValid(OtherActor) && IsOverlappingActor(OtherActor))
	{
		return;
	}
	CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return !IsValid(Entry.GetObject()) || Entry.GetObject() == OtherActor;
		});
}
