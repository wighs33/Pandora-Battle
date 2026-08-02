#include "AbilitySystem/EffectActors/EffectAreaBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Map/TransientActorRegistrySubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EffectAreaBase)

namespace
{
	float GetPandoraLoadoutDamageBonusPercent(
		const UBasicAttributeSet* AttributeSet,
		const EEnum_Direction LoadoutDirection)
	{
		if (!AttributeSet)
		{
			return 0.0f;
		}

		switch (LoadoutDirection)
		{
		case EEnum_Direction::Left:
			return FMath::Max(AttributeSet->GetFirstPandora(), 0.0f);
		case EEnum_Direction::Up:
			return FMath::Max(AttributeSet->GetSecondPandora(), 0.0f);
		case EEnum_Direction::Right:
			return FMath::Max(AttributeSet->GetThirdPandora(), 0.0f);
		case EEnum_Direction::Center:
		case EEnum_Direction::Down:
		default:
			return 0.0f;
		}
	}
}

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

void AEffectAreaBase::SetSourceActor(AActor* InSourceActor)
{
	SourceActor = InSourceActor;
	if (InSourceActor)
	{
		SetOwner(InSourceActor);
		if (APawn* SourcePawn = Cast<APawn>(InSourceActor))
		{
			SetInstigator(SourcePawn);
		}
	}

	if (HasActorBegunPlay())
	{
		if (UWorld* World = GetWorld())
		{
			if (UTransientActorRegistrySubsystem* Registry =
				World->GetSubsystem<UTransientActorRegistrySubsystem>())
			{
				Registry->RegisterTransientActor(this, InSourceActor);
			}
		}
	}
}

void AEffectAreaBase::SetSourcePandoraLoadoutDirection(const EEnum_Direction InLoadoutDirection)
{
	SourcePandoraLoadoutDirection = InLoadoutDirection;
}

void AEffectAreaBase::SetIgnoreSourceActor(const bool bInIgnoreSourceActor)
{
	bIgnoreSourceActor = bInIgnoreSourceActor;
}

void AEffectAreaBase::SetAffectEnemiesOnly(const bool bInAffectEnemiesOnly)
{
	bAffectEnemiesOnly = bInAffectEnemiesOnly;
}

void AEffectAreaBase::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UTransientActorRegistrySubsystem* Registry =
			World->GetSubsystem<UTransientActorRegistrySubsystem>())
		{
			Registry->RegisterTransientActor(this, ResolveSourceActor());
		}
	}

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
	if (UWorld* World = GetWorld())
	{
		if (UTransientActorRegistrySubsystem* Registry =
			World->GetSubsystem<UTransientActorRegistrySubsystem>())
		{
			Registry->UnregisterTransientActor(this);
		}
	}

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

	if (!ShouldApplyEffectToActor(TargetActor))
	{

		return;
	}

	UAbilitySystemComponent* TargetAbilitySystemComponent = GetTargetAbilitySystemComponent(TargetActor);
	if (!TargetAbilitySystemComponent)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* SpecAbilitySystemComponent = SourceAbilitySystemComponent ? SourceAbilitySystemComponent : TargetAbilitySystemComponent;
	FGameplayEffectContextHandle EffectContext = SpecAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddInstigator(GetInstigator(), this);
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = SpecAbilitySystemComponent->MakeOutgoingSpec(
		EffectClass,
		FMath::Max(EffectLevel, 1.0f),
		EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{

		return;
	}

	if (EffectMagnitudeDataTag.IsValid())
	{
		const UBasicAttributeSet* SourceAttributeSet = SourceAbilitySystemComponent
			? SourceAbilitySystemComponent->GetSet<UBasicAttributeSet>()
			: nullptr;
		const float IntelligenceDamagePercent = EffectMagnitudeDataTag.MatchesTag(LabGameplayTags::Data_Damage) && SourceAttributeSet
			? FMath::Max(SourceAttributeSet->GetIntelligence(), 0.0f)
			: 0.0f;
		const float LoadoutDamagePercent = EffectMagnitudeDataTag.MatchesTag(LabGameplayTags::Data_Damage) && SourceAttributeSet
			? GetPandoraLoadoutDamageBonusPercent(SourceAttributeSet, SourcePandoraLoadoutDirection)
			: 0.0f;
		const float AttackDamageBonusPercent = IntelligenceDamagePercent + LoadoutDamagePercent;
		const double IntelligenceMultiplier = 1.0 + (static_cast<double>(AttackDamageBonusPercent) * 0.01);
		const float FinalMagnitudeValue = FMath::Max(
			static_cast<float>(static_cast<double>(FMath::Max(EffectMagnitudeValue, 0.0f)) * IntelligenceMultiplier),
			0.0f);
		SpecHandle.Data->SetSetByCallerMagnitude(EffectMagnitudeDataTag, FinalMagnitudeValue);

	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent
			? SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetAbilitySystemComponent)
			: TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
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

UAbilitySystemComponent* AEffectAreaBase::GetSourceAbilitySystemComponent() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ResolveSourceActor());
}

AActor* AEffectAreaBase::ResolveSourceActor() const
{
	if (AActor* ExplicitSourceActor = SourceActor.Get())
	{
		return ExplicitSourceActor;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor;
	}

	return GetInstigator();
}

bool AEffectAreaBase::ShouldApplyEffectToActor(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return false;
	}

	AActor* CurrentSourceActor = ResolveSourceActor();
	if (bIgnoreSourceActor && CurrentSourceActor && TargetActor == CurrentSourceActor)
	{
		return false;
	}

	if (bAffectEnemiesOnly)
	{
		const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(CurrentSourceActor);
		const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
		if (!SourceCharacter || !TargetCharacter)
		{
			return false;
		}

		return SourceCharacter->CanDamageCharacterByTeam(TargetCharacter);
	}

	return true;
}
