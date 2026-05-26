#include "AbilitySystem/EffectActors/EffectAreaBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Common/LabGameplayTags.h"
#include "Components/SphereComponent.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EffectAreaBase)

DEFINE_LOG_CATEGORY_STATIC(PdEffectAreaLog, Log, All);

AEffectAreaBase::AEffectAreaBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	AreaCollision = CreateDefaultSubobject<USphereComponent>(TEXT("AreaCollision"));
	SetRootComponent(AreaCollision);
	AreaCollision->InitSphereRadius(100.0f);
	AreaCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AreaCollision->SetCollisionObjectType(ECC_WorldDynamic);
	AreaCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	AreaCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	AreaCollision->SetGenerateOverlapEvents(true);

	EffectMagnitudeDataTag = LabGameplayTags::Data_Damage;
}

void AEffectAreaBase::BeginPlay()
{
	Super::BeginPlay();

	if (AreaCollision)
	{
		AreaCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleAreaBeginOverlap);
		AreaCollision->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleAreaEndOverlap);

		AreaCollision->UpdateOverlaps();

		TArray<AActor*> OverlappingActors;
		AreaCollision->GetOverlappingActors(OverlappingActors);
		for (AActor* OverlappingActor : OverlappingActors)
		{
			ApplyEffectToActor(OverlappingActor);
		}
	}
}

void AEffectAreaBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	ActiveEffectHandles.GetKeys(Actors);

	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		RemoveEffectFromActor(Actor.Get());
	}

	if (AreaCollision)
	{
		AreaCollision->OnComponentBeginOverlap.RemoveAll(this);
		AreaCollision->OnComponentEndOverlap.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AEffectAreaBase::HandleAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	ApplyEffectToActor(OtherActor);
}

void AEffectAreaBase::HandleAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	RemoveEffectFromActor(OtherActor);
}

void AEffectAreaBase::ApplyEffectToActor(AActor* TargetActor)
{
	if (!TargetActor || TargetActor == this || !EffectClass || ActiveEffectHandles.Contains(TargetActor))
	{
		return;
	}

	if (bApplyOnlyOnAuthority && !HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetTargetAbilitySystemComponent(TargetActor);
	if (!AbilitySystemComponent)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		EffectClass,
		FMath::Max(EffectLevel, 1.0f),
		EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(PdEffectAreaLog, Warning, TEXT("EffectArea '%s' failed to create spec for '%s'."),
			*GetNameSafe(this),
			*GetNameSafe(TargetActor));
		return;
	}

	if (EffectMagnitudeDataTag.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(EffectMagnitudeDataTag, EffectMagnitudeValue);
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		ActiveEffectHandles.Add(TargetActor, AppliedHandle);
	}
}

void AEffectAreaBase::RemoveEffectFromActor(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return;
	}

	FActiveGameplayEffectHandle ActiveHandle;
	if (!ActiveEffectHandles.RemoveAndCopyValue(TargetActor, ActiveHandle) || !ActiveHandle.IsValid())
	{
		return;
	}

	if (bApplyOnlyOnAuthority && !HasAuthority())
	{
		return;
	}

	if (UAbilitySystemComponent* AbilitySystemComponent = GetTargetAbilitySystemComponent(TargetActor))
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveHandle);
	}
}

UAbilitySystemComponent* AEffectAreaBase::GetTargetAbilitySystemComponent(AActor* TargetActor) const
{
	return TargetActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor) : nullptr;
}
