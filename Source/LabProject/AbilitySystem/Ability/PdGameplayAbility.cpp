#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Character/PdCharacterBase.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

// 로그 카테고리 정의입니다.
DEFINE_LOG_CATEGORY(PdGameplayAbilityLog);

/** 공용 GameplayAbility 기본 설정을 초기화합니다. */
UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === Ability 기본 실행 정책 설정

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

/** 현재 AvatarActor를 프로젝트 캐릭터 타입으로 반환합니다. */
APdCharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<APdCharacterBase>(GetAvatarActorFromActorInfo());
}

/** 현재 ActorInfo의 PlayerController를 프로젝트 타입으로 반환합니다. */
APdPlayerController* UPdGameplayAbility::GetPdPlayerControllerFromActorInfo() const
{
	return Cast<APdPlayerController>(GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr);
}

/** 현재 ActorInfo 기준 PlayerState를 프로젝트 타입으로 반환합니다. */
APdPlayerState* UPdGameplayAbility::GetPdPlayerStateFromActorInfo() const
{
	// =================================================================================================================
	// === Character 기준 PlayerState 우선 조회

	if (const APdCharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		return Character->GetPlayerState<APdPlayerState>();
	}

	// OwningActor가 곧 PlayerState인 경우를 보조 처리합니다.
	return Cast<APdPlayerState>(GetOwningActorFromActorInfo());
}

/** 현재 ActorInfo의 ASC를 프로젝트 타입으로 반환합니다. */
UPdAbilitySystemComponent* UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

/** 전달된 태그를 가진 Ability들의 활성화를 시도합니다. */
bool UPdGameplayAbility::TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, bool bAllowRemoteActivation) const
{
	// =================================================================================================================
	// === ASC 및 입력 태그 검사

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || InAbilityTags.IsEmpty())
	{
		return false;
	}

	return ASC->TryActivateAbilitiesByTag(InAbilityTags, bAllowRemoteActivation);
}

/** 가장 가까운 적을 찾고 좌우 방향 정보를 반환합니다. */
bool UPdGameplayAbility::GetClosestEnemy(AActor*& ClosestEnemy, bool& bLeftOrRight, float SearchRadius, float ForwardOffset) const
{
	// =================================================================================================================
	// === 반환값 초기화

	ClosestEnemy = nullptr;
	bLeftOrRight = false;

	// =================================================================================================================
	// === 소유자와 탐색 반경 유효성 검사

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || SearchRadius <= 0.f)
	{
		return false;
	}

	// =================================================================================================================
	// === 소유자 진영 정보 확인

	const APdCharacterBase* OwnerCharacter = Cast<APdCharacterBase>(AvatarActor);
	if (!OwnerCharacter)
	{
		return false;
	}

	// =================================================================================================================
	// === 월드 및 탐색 기준 위치 설정

	UWorld* World = AvatarActor->GetWorld();
	if (!World)
	{
		return false;
	}

	const int32 OwnerFaction = OwnerCharacter->GetFactionId();
	const FVector AvatarLocation = AvatarActor->GetActorLocation();
	const FVector SearchOrigin = AvatarLocation + (AvatarActor->GetActorForwardVector() * ForwardOffset);

	// =================================================================================================================
	// === Pawn 대상 구형 오버랩 설정

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(NAME_None, false, AvatarActor);
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SearchRadius);

	// =================================================================================================================
	// === 주변 Pawn 오버랩 탐색

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(OverlapResults, SearchOrigin, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams))
	{
		return false;
	}

	// =================================================================================================================
	// === 가장 가까운 적 탐색

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
	// === 목표의 좌우 방향 계산

	const FVector ToEnemy = ClosestEnemy->GetActorLocation() - AvatarLocation;
	bLeftOrRight = FVector::DotProduct(AvatarActor->GetActorRightVector(), ToEnemy) < 0.f;
	return true;
}

/** 전달된 Ability 클래스들을 중복 없이 부여합니다. */
int32 UPdGameplayAbility::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses, int32 AbilityLevel)
{
	// =================================================================================================================
	// === ASC 및 입력 배열 검사

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || AbilityClasses.IsEmpty())
	{
		return 0;
	}

	// =================================================================================================================
	// === 서버 권한 검사

	if (!ensure(ASC->IsOwnerActorAuthoritative()))
	{
		return 0;
	}

	// =================================================================================================================
	// === 중복 없이 Ability 부여

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

/** 전달된 GameplayEffect 클래스들을 순서대로 적용합니다. */
int32 UPdGameplayAbility::ApplyGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses, float EffectLevel, int32 StackCount)
{
	// =================================================================================================================
	// === Effect 배열 순차 적용

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

/** 단일 GameplayEffect를 적용합니다. */
bool UPdGameplayAbility::ApplyGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	return ApplyGameplayEffectHandle(GameplayEffectClass, EffectLevel, StackCount).WasSuccessfullyApplied();
}

/** 단일 GameplayEffect를 핸들 형태로 적용합니다. */
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel, int32 StackCount)
{
	const FGameplayTagContainer DynamicGrantedTags;
	return ApplyGameplayEffectHandle(GameplayEffectClass, DynamicGrantedTags, EffectLevel, StackCount);
}

/** 동적 부여 태그를 포함한 GameplayEffect를 핸들 형태로 적용합니다. */
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTagContainer& DynamicGrantedTags, float EffectLevel, int32 StackCount)
{
	// =================================================================================================================
	// === ASC 및 GameplayEffect 클래스 검사

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === 권한 또는 예측 키 검사

	if (!ensure(ActorInfo) || !HasAuthorityOrPredictionKey(ActorInfo, &CurrentActivationInfo))
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === GameplayEffectSpec 생성

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GameplayEffectClass, FMath::Max(EffectLevel, 1.f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	// =================================================================================================================
	// === 스택 수와 동적 부여 태그 설정 후 자기 자신에게 적용

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	SpecHandle.Data->DynamicGrantedTags.AppendTags(DynamicGrantedTags);
	return ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

/** 지정한 GameplayEffect가 현재 활성 상태인지 확인합니다. */
bool UPdGameplayAbility::HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	// =================================================================================================================
	// === ASC 및 GameplayEffect 클래스 검사

	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return false;
	}

	// =================================================================================================================
	// === Effect 정의 기준 활성 여부 조회

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return ASC->GetActiveEffects(Query).Num() > 0;
}

/** 전달된 GameplayEffect 클래스들을 순서대로 제거합니다. */
int32 UPdGameplayAbility::RemoveGameplayEffects(const TArray<TSubclassOf<UGameplayEffect>>& GameplayEffectClasses)
{
	// =================================================================================================================
	// === Effect 배열 순차 제거

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

/** 단일 GameplayEffect를 제거합니다. */
bool UPdGameplayAbility::RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	// =================================================================================================================
	// === ASC 및 GameplayEffect 클래스 검사

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !GameplayEffectClass)
	{
		return false;
	}

	// =================================================================================================================
	// === 서버 권한 검사

	if (!ensure(ActorInfo) || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	// =================================================================================================================
	// === Effect 정의 기준 활성 Effect 제거

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return ASC->RemoveActiveEffects(Query) > 0;
}

/** 지정 태그를 부여한 활성 GameplayEffect들을 제거합니다. */
int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags)
{
	// =================================================================================================================
	// === ASC 및 입력 태그 검사

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || GrantedTags.IsEmpty())
	{
		return 0;
	}

	// =================================================================================================================
	// === 서버 권한 검사

	if (!ensure(ActorInfo) || !HasAuthority(&CurrentActivationInfo))
	{
		return 0;
	}

	return ASC->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

/** 지정한 Ability가 없을 때만 새로 부여합니다. */
bool UPdGameplayAbility::GrantAbilityIfMissing(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel)
{
	// =================================================================================================================
	// === ASC 및 Ability 클래스 검사

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	// =================================================================================================================
	// === 서버 권한 및 중복 부여 여부 검사

	if (!ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	if (HasGrantedAbility(AbilityClass))
	{
		return false;
	}

	// =================================================================================================================
	// === AbilitySpec 생성 후 Ability 부여

	FGameplayAbilitySpec AbilitySpec(AbilityClass, FMath::Max(AbilityLevel, 1), INDEX_NONE, GetAvatarActorFromActorInfo());
	ASC->GiveAbility(AbilitySpec);
	return true;
}

/** 지정한 Ability가 이미 부여되어 있는지 확인합니다. */
bool UPdGameplayAbility::HasGrantedAbility(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	// =================================================================================================================
	// === ASC 및 Ability 클래스 검사

	const UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponentFromActorInfo();
	const UClass* AbilityClassType = AbilityClass.Get();
	if (!ASC || !AbilityClassType)
	{
		return false;
	}

	// =================================================================================================================
	// === 활성화 가능 Ability 목록에서 동일 클래스 검색

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClassType)
		{
			return true;
		}
	}

	return false;
}
