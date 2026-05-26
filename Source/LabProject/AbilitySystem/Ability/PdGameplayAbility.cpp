#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

// ë¡œê·¸ ì¹´í…Œê³ ë¦¬ ?•ì˜?…ë‹ˆ??
DEFINE_LOG_CATEGORY(PdGameplayAbilityLog);

/** ê³µìš© GameplayAbility ê¸°ë³¸ ?¤ì •??ì´ˆê¸°?”í•©?ˆë‹¤. */
namespace
{
	bool IsDeadCharacter(const AActor* Actor)
	{
		const APdCharacterBase* Character = Cast<APdCharacterBase>(Actor);
		const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
		return ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
	}


	int32 GetPandoraSkillIndexFromAbilitySpec(const FGameplayAbilitySpec* AbilitySpec)
	{
		if (!AbilitySpec)
		{
			return INDEX_NONE;
		}

		const FGameplayTagContainer& SourceTags = AbilitySpec->GetDynamicSpecSourceTags();
		if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1))
		{
			return 0;
		}

		if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2))
		{
			return 1;
		}

		if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3))
		{
			return 2;
		}

		if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4))
		{
			return 3;
		}

		return INDEX_NONE;
	}
}

UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === Ability ê¸°ë³¸ ?¤í–‰ ?•ì±… ?¤ì •

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Dead);
}

/** ?„ì¬ AvatarActorë¥??„ë¡œ?íŠ¸ ìºë¦­???€?…ìœ¼ë¡?ë°˜í™˜?©ë‹ˆ?? */
APdCharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<APdCharacterBase>(GetAvatarActorFromActorInfo());
}

/** ?„ì¬ ActorInfo??PlayerControllerë¥??„ë¡œ?íŠ¸ ?€?…ìœ¼ë¡?ë°˜í™˜?©ë‹ˆ?? */
APdPlayerController* UPdGameplayAbility::GetPdPlayerControllerFromActorInfo() const
{
	return Cast<APdPlayerController>(GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr);
}

/** ?„ì¬ ActorInfo ê¸°ì? PlayerStateë¥??„ë¡œ?íŠ¸ ?€?…ìœ¼ë¡?ë°˜í™˜?©ë‹ˆ?? */
APdPlayerState* UPdGameplayAbility::GetPdPlayerStateFromActorInfo() const
{
	// =================================================================================================================
	// === Character ê¸°ì? PlayerState ?°ì„  ì¡°íšŒ

	if (const APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		return Character->GetPlayerState<APdPlayerState>();
	}

	// OwningActorê°€ ê³?PlayerState??ê²½ìš°ë¥?ë³´ì¡° ì²˜ë¦¬?©ë‹ˆ??
	return Cast<APdPlayerState>(GetOwningActorFromActorInfo());
}

/** ?„ì¬ ActorInfo??ASCë¥??„ë¡œ?íŠ¸ ?€?…ìœ¼ë¡?ë°˜í™˜?©ë‹ˆ?? */
UPdAbilitySystemComponent* UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

/** ?„ë‹¬???œê·¸ë¥?ê°€ì§?Ability?¤ì˜ ?œì„±?”ë? ?œë„?©ë‹ˆ?? */
bool UPdGameplayAbility::TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation) const
{
	// =================================================================================================================
	// === ASC ë°??…ë ¥ ?œê·¸ ê²€??
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || InAbilityTags.IsEmpty())
	{
		return false;
	}

	return ASC->TryActivateAbilitiesByTag(InAbilityTags, bAllowRemoteActivation);
}

/** ê°€??ê°€ê¹Œìš´ ?ì„ ì°¾ê³  ì¢Œìš° ë°©í–¥ ?•ë³´ë¥?ë°˜í™˜?©ë‹ˆ?? */
bool UPdGameplayAbility::GetClosestEnemy(AActor*& ClosestEnemy, bool& bLeftOrRight, float SearchRadius, float ForwardOffset) const
{
	// =================================================================================================================
	// === ë°˜í™˜ê°?ì´ˆê¸°??
	ClosestEnemy = nullptr;
	bLeftOrRight = false;

	// =================================================================================================================
	// === ?Œìœ ?ì? ?ìƒ‰ ë°˜ê²½ ? íš¨??ê²€??
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || SearchRadius <= 0.f)
	{
		return false;
	}

	// =================================================================================================================
	// === ?Œìœ ??ì§„ì˜ ?•ë³´ ?•ì¸

	const APdCharacterBase* OwnerCharacter = Cast<APdCharacterBase>(AvatarActor);
	if (!OwnerCharacter)
	{
		return false;
	}

	// =================================================================================================================
	// === ?”ë“œ ë°??ìƒ‰ ê¸°ì? ?„ì¹˜ ?¤ì •

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return false;
	}

	const int32 OwnerFaction = OwnerCharacter->GetFactionId();
	const FVector AvatarLocation = AvatarActor->GetActorLocation();
	const FVector SearchOrigin = AvatarLocation + (AvatarActor->GetActorForwardVector() * ForwardOffset);

	// =================================================================================================================
	// === Pawn ?€??êµ¬í˜• ?¤ë²„???¤ì •

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(NAME_None, false, AvatarActor);
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRadius);

	// =================================================================================================================
	// === ì£¼ë? Pawn ?¤ë²„???ìƒ‰

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(OverlapResults, SearchOrigin, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams))
	{
		return false;
	}

	// =================================================================================================================
	// === ê°€??ê°€ê¹Œìš´ ???ìƒ‰

	double BestDistanceSq = TNumericLimits<double>::Max();

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OtherActor = OverlapResult.GetActor();
		if (!IsValid(OtherActor) || OtherActor == AvatarActor)
		{
			continue;
		}

		const APdCharacterBase* OtherCharacter = Cast<APdCharacterBase>(OtherActor);
		if (!OtherCharacter)
		{
			continue;
		}

		if (IsDeadCharacter(OtherActor))
		{
			continue;
		}

		if (OtherCharacter->GetFactionId() == OwnerFaction)
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

	// =================================================================================================================
	// === ëª©í‘œ??ì¢Œìš° ë°©í–¥ ê³„ì‚°

	const FVector ToEnemy = ClosestEnemy->GetActorLocation() - AvatarLocation;
	bLeftOrRight = FVector::DotProduct(AvatarActor->GetActorRightVector(), ToEnemy) < 0.f;
	return true;
}

bool UPdGameplayAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

AActor* UPdGameplayAbility::GetAttackTargetFromAvatar() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	if (AvatarActor->GetClass()->ImplementsInterface(UTargetingInterface::StaticClass()))
	{
		AActor* AttackTarget = ITargetingInterface::Execute_GetAttackTarget(AvatarActor);
		return IsDeadCharacter(AttackTarget) ? nullptr : AttackTarget;
	}

	return nullptr;
}

USkillDataAsset* UPdGameplayAbility::GetSourceSkillDataAsset() const
{
	if (const UPandoraSkillRuntimeContext* RuntimeContext = GetSourceSkillRuntimeContext())
	{
		return const_cast<USkillDataAsset*>(RuntimeContext->GetSkillDataAsset());
	}

	return Cast<USkillDataAsset>(GetCurrentAbilitySpecSourceObject());
}

UPandoraSkillRuntimeContext* UPdGameplayAbility::GetSourceSkillRuntimeContext() const
{
	if (UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(GetCurrentAbilitySpecSourceObject()))
	{
		return RuntimeContext;
	}

	return ResolveSourceSkillRuntimeContextFromSelectedPandora();
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPdGameplayAbility::GetSourceProjectileImpactEffectAreasForLevel(const int32 Level) const
{
	if (const UPandoraSkillRuntimeContext* RuntimeContext = GetSourceSkillRuntimeContext())
	{
		return RuntimeContext->GetProjectileImpactEffectAreasForLevel(Level);
	}

	if (const USkillDataAsset* SourceSkill = Cast<USkillDataAsset>(GetCurrentAbilitySpecSourceObject()))
	{
		return SourceSkill->GetLegacyProjectileImpactEffectAreasForLevel(Level);
	}

	return TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

const FGameplayAbilitySpec* UPdGameplayAbility::ResolveCurrentAbilitySpec() const
{
	if (const FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec())
	{
		return AbilitySpec;
	}

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle SpecHandle = GetCurrentAbilitySpecHandle();
	return ASC && SpecHandle.IsValid() ? ASC->FindAbilitySpecFromHandle(SpecHandle) : nullptr;
}

UObject* UPdGameplayAbility::GetCurrentAbilitySpecSourceObject() const
{
	const FGameplayAbilitySpec* AbilitySpec = ResolveCurrentAbilitySpec();
	return AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
}

UPandoraSkillRuntimeContext* UPdGameplayAbility::ResolveSourceSkillRuntimeContextFromSelectedPandora() const
{
	const FGameplayAbilitySpec* AbilitySpec = ResolveCurrentAbilitySpec();
	const int32 SkillIndex = GetPandoraSkillIndexFromAbilitySpec(AbilitySpec);
	if (SkillIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const APdPlayerState* PlayerState = GetPdPlayerStateFromActorInfo();
	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const UPandoraDefinition* PandoraDefinition = PandoraComponent ? PandoraComponent->GetCurrentPandoraDefinition() : nullptr;
	if (!PandoraDefinition || !PandoraDefinition->Skill.IsValidIndex(SkillIndex))
	{
		return nullptr;
	}

	const FSkill& Skill = PandoraDefinition->Skill[SkillIndex];
	const USkillDataAsset* SkillDataAsset = Skill.SkillDefinition.Get();
	if (!SkillDataAsset)
	{
		return nullptr;
	}

	const int32 RuntimeLevel = AbilitySpec ? FMath::Max(AbilitySpec->Level, 1) : FMath::Max(GetAbilityLevel(), 1);
	if (CachedResolvedSourceSkillRuntimeContext
		&& CachedResolvedSourceSkillRuntimeContext->GetPandoraDefinition() == PandoraDefinition
		&& CachedResolvedSourceSkillRuntimeContext->GetSkillDataAsset() == SkillDataAsset
		&& CachedResolvedSourceSkillRuntimeContext->GetSkillIndex() == SkillIndex
		&& CachedResolvedSourceSkillRuntimeContext->GetPandoraLevel() == RuntimeLevel)
	{
		return CachedResolvedSourceSkillRuntimeContext.Get();
	}

	UPdGameplayAbility* MutableThis = const_cast<UPdGameplayAbility*>(this);
	MutableThis->CachedResolvedSourceSkillRuntimeContext = NewObject<UPandoraSkillRuntimeContext>(MutableThis);
	MutableThis->CachedResolvedSourceSkillRuntimeContext->Initialize(PandoraDefinition, SkillDataAsset, SkillIndex, RuntimeLevel);
	return MutableThis->CachedResolvedSourceSkillRuntimeContext.Get();
}

/** Granted Ability classes are added without duplicates. */
int32 UPdGameplayAbility::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel)
{
	// =================================================================================================================
	// === ASC ë°??…ë ¥ ë°°ì—´ ê²€??
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || AbilityClasses.IsEmpty())
	{
		return 0;
	}

	// =================================================================================================================
	// === ?œë²„ ê¶Œí•œ ê²€??
	if (!ensure(ASC->IsOwnerActorAuthoritative()))
	{
		return 0;
	}

	return ASC->GrantAbilities(AbilityClasses, AbilityLevel, GetAvatarActorFromActorInfo()).Num();
}

/** ?„ë‹¬??GameplayEffect ?´ë˜?¤ë“¤???œì„œ?€ë¡??ìš©?©ë‹ˆ?? */
int32 UPdGameplayAbility::ApplyGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses, float EffectLevel, int32 StackCount)
{
	// =================================================================================================================
	// === Effect ë°°ì—´ ?œì°¨ ?ìš©

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

/** ?¨ì¼ GameplayEffectë¥??ìš©?©ë‹ˆ?? */
bool UPdGameplayAbility::ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	return ApplyGameplayEffectHandle(GameplayEffectClass, EffectLevel, StackCount).WasSuccessfullyApplied();
}

/** ?¨ì¼ GameplayEffectë¥??¸ë“¤ ?•íƒœë¡??ìš©?©ë‹ˆ?? */
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	const FGameplayTagContainer DynamicGrantedTags;
	return ApplyGameplayEffectHandle(GameplayEffectClass, DynamicGrantedTags, EffectLevel, StackCount);
}

/** ?™ì  ë¶€???œê·¸ë¥??¬í•¨??GameplayEffectë¥??¸ë“¤ ?•íƒœë¡??ìš©?©ë‹ˆ?? */
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTagContainer& DynamicGrantedTags, float EffectLevel, int32 StackCount)
{
	// =================================================================================================================
	// === ASC ë°?GameplayEffect ?´ë˜??ê²€??
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === ê¶Œí•œ ?ëŠ” ?ˆì¸¡ ??ê²€??
	if (!ensure(ActorInfo) || !HasAuthorityOrPredictionKey(ActorInfo, &CurrentActivationInfo))
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === GameplayEffectSpec ?ì„±

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GameplayEffectClass, FMath::Max(EffectLevel, 1.f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === ?¤íƒ ?˜ì? ?™ì  ë¶€???œê·¸ ?¤ì • ???ê¸° ?ì‹ ?ê²Œ ?ìš©

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	SpecHandle.Data->DynamicGrantedTags.AppendTags(DynamicGrantedTags);
	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

/** ì§€?•í•œ GameplayEffectê°€ ?„ì¬ ?œì„± ?íƒœ?¸ì? ?•ì¸?©ë‹ˆ?? */
bool UPdGameplayAbility::HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	// =================================================================================================================
	// === ASC ë°?GameplayEffect ?´ë˜??ê²€??
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return false;
	}

	// =================================================================================================================
	// === Effect ?•ì˜ ê¸°ì? ?œì„± ?¬ë? ì¡°íšŒ

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return ASC->GetActiveEffects(Query).Num() > 0;
}

/** ?„ë‹¬??GameplayEffect ?´ë˜?¤ë“¤???œì„œ?€ë¡??œê±°?©ë‹ˆ?? */
int32 UPdGameplayAbility::RemoveGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses)
{
	// =================================================================================================================
	// === Effect ë°°ì—´ ?œì°¨ ?œê±°

	int32 RemovedCount = 0;
	for (TSubclassOf<UGameplayEffect> GameplayEffectClass : GameplayEffectClasses)
	{
		if (RemoveGameplayEffect(GameplayEffectClass))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}

/** ?¨ì¼ GameplayEffectë¥??œê±°?©ë‹ˆ?? */
bool UPdGameplayAbility::RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	// =================================================================================================================
	// === ASC ë°?GameplayEffect ?´ë˜??ê²€??
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return false;
	}

	// =================================================================================================================
	// === ?œë²„ ê¶Œí•œ ê²€??
	if (!ensure(ActorInfo) || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	// =================================================================================================================
	// === Effect ?•ì˜ ê¸°ì? ?œì„± Effect ?œê±°

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return ASC->RemoveActiveEffects(Query) > 0;
}

/** ì§€???œê·¸ë¥?ë¶€?¬í•œ ?œì„± GameplayEffect?¤ì„ ?œê±°?©ë‹ˆ?? */
int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags)
{
	// =================================================================================================================
	// === ASC ë°??…ë ¥ ?œê·¸ ê²€??
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || GrantedTags.IsEmpty())
	{
		return 0;
	}

	// =================================================================================================================
	// === ?œë²„ ê¶Œí•œ ê²€??
	if (!ensure(ActorInfo) || !HasAuthority(&CurrentActivationInfo))
	{
		return 0;
	}

	return ASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

/** ì§€?•í•œ Abilityê°€ ?†ì„ ?Œë§Œ ?ˆë¡œ ë¶€?¬í•©?ˆë‹¤. */
bool UPdGameplayAbility::GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel)
{
	// =================================================================================================================
	// === ASC ë°?Ability ?´ë˜??ê²€??
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	// =================================================================================================================
	// === ?œë²„ ê¶Œí•œ ë°?ì¤‘ë³µ ë¶€???¬ë? ê²€??
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	if (HasGrantedAbility(AbilityClass))
	{
		return false;
	}

	// =================================================================================================================
	// === AbilitySpec ?ì„± ??Ability ë¶€??
	FGameplayAbilitySpec AbilitySpec(AbilityClass, FMath::Max(AbilityLevel, 1), INDEX_NONE, GetAvatarActorFromActorInfo());
	const UPdGameplayAbility* AbilityCDO = Cast<UPdGameplayAbility>(AbilityClass->GetDefaultObject());
	const bool bShouldAutoActivateWhenGranted = AbilityCDO && AbilityCDO->ShouldAutoActivateWhenGranted();

	const FGameplayAbilitySpecHandle GrantedHandle = ASC->GiveAbility(AbilitySpec);
	if (bShouldAutoActivateWhenGranted && GrantedHandle.IsValid())
	{
		ASC->TryActivateAbility(GrantedHandle);
	}
	return true;
}

/** ì§€?•í•œ Abilityê°€ ?´ë? ë¶€?¬ë˜???ˆëŠ”ì§€ ?•ì¸?©ë‹ˆ?? */
bool UPdGameplayAbility::HasGrantedAbility(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	// =================================================================================================================
	// === ASC ë°?Ability ?´ë˜??ê²€??
	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	const UClass* AbilityClassType = AbilityClass.Get();
	if (!ASC || !AbilityClassType)
	{
		return false;
	}

	// =================================================================================================================
	// === ?œì„±??ê°€??Ability ëª©ë¡?ì„œ ?™ì¼ ?´ë˜??ê²€??
	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClassType)
		{
			return true;
		}
	}

	return false;
}
