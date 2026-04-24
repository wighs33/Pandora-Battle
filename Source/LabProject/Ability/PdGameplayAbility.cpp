#include "Ability/PdGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Interface/FactionInterface.h"
#include "Mode/PdCharacterBase.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

DEFINE_LOG_CATEGORY(PdGameplayAbilityLog);

UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

APdCharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<APdCharacterBase>(GetAvatarActorFromActorInfo());
}

APdPlayerController* UPdGameplayAbility::GetPdPlayerControllerFromActorInfo() const
{
	return Cast<APdPlayerController>(GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr);
}

APdPlayerState* UPdGameplayAbility::GetPdPlayerStateFromActorInfo() const
{
	if (const APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		return Character->GetPlayerState<APdPlayerState>();
	}

	return Cast<APdPlayerState>(GetOwningActorFromActorInfo());
}

UPdAbilitySystemComponent* UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

bool UPdGameplayAbility::TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation) const
{
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || InAbilityTags.IsEmpty())
	{
		return false;
	}

	return ASC->TryActivateAbilitiesByTag(InAbilityTags, bAllowRemoteActivation);
}

bool UPdGameplayAbility::GetClosestEnemy(AActor*& ClosestEnemy, bool& bLeftOrRight, float SearchRadius, float ForwardOffset) const
{
	ClosestEnemy = nullptr;
	bLeftOrRight = false;

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || SearchRadius <= 0.f)
	{
		return false;
	}

	const IFactionInterface* OwnerFactionInterface = Cast<IFactionInterface>(AvatarActor);
	if (!OwnerFactionInterface)
	{
		UE_LOG(PdGameplayAbilityLog, Warning, TEXT("GetClosestEnemy failed: owner '%s' does not implement FactionInterface."), *GetNameSafe(AvatarActor));
		return false;
	}

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return false;
	}

	const int32 OwnerFaction = OwnerFactionInterface->GetFactionId();
	const FVector AvatarLocation = AvatarActor->GetActorLocation();
	const FVector SearchOrigin = AvatarLocation + (AvatarActor->GetActorForwardVector() * ForwardOffset);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(NAME_None, false, AvatarActor);
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRadius);

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(OverlapResults, SearchOrigin, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams))
	{
		return false;
	}

	double BestDistanceSq = TNumericLimits<double>::Max();

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OtherActor = OverlapResult.GetActor();
		if (!IsValid(OtherActor) || OtherActor == AvatarActor)
		{
			continue;
		}

		const IFactionInterface* OtherFactionInterface = Cast<IFactionInterface>(OtherActor);
		if (!OtherFactionInterface)
		{
			continue;
		}

		if (OtherFactionInterface->GetFactionId() == OwnerFaction)
		{
			continue;
		}

		const double DistanceSq = FVector::DistSquared(SearchOrigin, OtherActor->GetActorLocation());
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			ClosestEnemy = OtherActor;
		}
	}

	if (!ClosestEnemy)
	{
		return false;
	}

	const FVector ToEnemy = ClosestEnemy->GetActorLocation() - AvatarLocation;
	bLeftOrRight = FVector::DotProduct(AvatarActor->GetActorRightVector(), ToEnemy) < 0.f;
	return true;
}

int32 UPdGameplayAbility::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel)
{
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || AbilityClasses.IsEmpty())
	{
		return 0;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		UE_LOG(PdGameplayAbilityLog, Warning, TEXT("GrantAbilities failed: '%s' can only grant abilities on the server."), *GetNameSafe(this));
		return 0;
	}

	int32 GrantedCount = 0;
	for (TSubclassOf<UGameplayAbility> AbilityClass : AbilityClasses)
	{
		if (GrantAbilityIfMissing(AbilityClass, AbilityLevel))
		{
			++GrantedCount;
		}
	}

	return GrantedCount;
}

int32 UPdGameplayAbility::ApplyGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses, float EffectLevel, int32 StackCount)
{
	int32 AppliedCount = 0;
	for (TSubclassOf<UGameplayEffect> GameplayEffectClass : GameplayEffectClasses)
	{
		if (ApplyGameplayEffectHandle(GameplayEffectClass, EffectLevel, StackCount).WasSuccessfullyApplied())
		{
			++AppliedCount;
		}
	}

	return AppliedCount;
}

bool UPdGameplayAbility::ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	return ApplyGameplayEffectHandle(GameplayEffectClass, EffectLevel, StackCount).WasSuccessfullyApplied();
}

FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	const FGameplayTagContainer DynamicGrantedTags;
	return ApplyGameplayEffectHandle(GameplayEffectClass, DynamicGrantedTags, EffectLevel, StackCount);
}

FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTagContainer& DynamicGrantedTags, float EffectLevel, int32 StackCount)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	if (!ActorInfo || !HasAuthorityOrPredictionKey(ActorInfo, &CurrentActivationInfo))
	{
		UE_LOG(PdGameplayAbilityLog, Warning, TEXT("ApplyGameplayEffectToOwnerInCode failed: '%s' requires authority or a valid prediction key."), *GetNameSafe(this));
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GameplayEffectClass, FMath::Max(EffectLevel, 1.f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	SpecHandle.Data->DynamicGrantedTags.AppendTags(DynamicGrantedTags);
	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

int32 UPdGameplayAbility::RemoveGameplayEffects(const TArray<FActiveGameplayEffectHandle>& GameplayEffectHandles)
{
	int32 RemovedCount = 0;
	for (FActiveGameplayEffectHandle GameplayEffectHandle : GameplayEffectHandles)
	{
		if (RemoveGameplayEffect(GameplayEffectHandle))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}

bool UPdGameplayAbility::RemoveGameplayEffect(FActiveGameplayEffectHandle GameplayEffectHandle)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectHandle.IsValid())
	{
		return false;
	}

	if (!ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		UE_LOG(PdGameplayAbilityLog, Warning, TEXT("RemoveGameplayEffect failed: '%s' can only remove gameplay effects on the server."), *GetNameSafe(this));
		return false;
	}

	return ASC->RemoveActiveGameplayEffect(GameplayEffectHandle);
}

int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || GrantedTags.IsEmpty())
	{
		return 0;
	}

	if (!ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		UE_LOG(PdGameplayAbilityLog, Warning, TEXT("RemoveGameplayEffectsWithGrantedTags failed: '%s' can only remove gameplay effects on the server."), *GetNameSafe(this));
		return 0;
	}

	return ASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

bool UPdGameplayAbility::GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel)
{
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	if (!ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	if (HasGrantedAbility(AbilityClass))
	{
		return false;
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, FMath::Max(AbilityLevel, 1), INDEX_NONE, GetAvatarActorFromActorInfo());
	ASC->GiveAbility(AbilitySpec);
	return true;
}

bool UPdGameplayAbility::HasGrantedAbility(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	const UClass* AbilityClassType = AbilityClass.Get();
	if (!ASC || !AbilityClassType)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClassType)
		{
			return true;
		}
	}

	return false;
}
