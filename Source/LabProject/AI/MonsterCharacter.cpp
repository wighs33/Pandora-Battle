#include "AI/MonsterCharacter.h"

#include "AI/MonsterAIController.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Common/EquipmentAbilityData.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/RewardDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogMonsterCharacter, Log, All);

// 몬스터는 서버에서 생성하고, 자동 공격 타이머 대신 StateTree가 공격 시점을 결정한다.
AMonsterCharacter::AMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bStartCombatOnPossess = false;
	bUseBehaviorTreeCombat = false;
	bEnableTrainingBotHitReaction = false;
	ContactDamageDataTag = LabGameplayTags::Data_Damage;
}

// 이 몬스터에 지정된 정의에서 체력과 피격 표현 설정을 가져온다.
void AMonsterCharacter::ApplyResolvedEnemyDefinition(const UEnemyBaseDefinition* ResolvedDefinition)
{
	Super::ApplyResolvedEnemyDefinition(ResolvedDefinition);
	MonsterPresentationSettings = ResolvedDefinition ? ResolvedDefinition->GetMonsterPresentationSettings() : FMonsterPresentationSettings();
	ResolvedMonsterMaxHealth = ResolvedDefinition ? ResolvedDefinition->GetMonsterMaxHealth() : 0.0f;
}

// 일반 적의 자동 공격·훈련봇 리스폰을 끄고 몬스터 행동 트리의 제어에 맞춘다.
void AMonsterCharacter::ModifyResolvedEnemySettings(
	FEnemyCombatSettings& CombatSettings, FEnemyTrainingBotSettings& TrainingBotSettings) const
{
	Super::ModifyResolvedEnemySettings(CombatSettings, TrainingBotSettings);
	CombatSettings.bUseNearestPlayerWhenTargetUnset = false;
	CombatSettings.bAttackEnabled = true;
	CombatSettings.bStartCombatOnPossess = false;
	CombatSettings.bUseBehaviorTreeCombat = false;
	CombatSettings.bAttackImmediatelyAfterStart = false;
	TrainingBotSettings.bRespawnOnDeath = false;
	TrainingBotSettings.bEnableHitReaction = false;
}

// 공통 캐릭터와 몬스터 공격 자산이 모두 준비되기 전에는 게임플레이 초기화를 보류한다.
bool AMonsterCharacter::IsAdditionalCharacterRuntimeContentReady() const
{
	return bMonsterContentReady && Super::IsAdditionalCharacterRuntimeContentReady();
}

// 체력을 확정하고 빙결 중 공격을 중단하도록 연결한 뒤, 컨트롤러에 AI 시작을 요청한다.
void AMonsterCharacter::HandleCharacterRuntimeInitialized()
{
	Super::HandleCharacterRuntimeInitialized();
	ApplyMonsterHealthDefaults();
	if (HasAuthority() && AbilitySystemComponent)
	{
		FrozenTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
			LabGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleFrozenTagChanged);
	}
	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
	{
		MonsterController->StartMonsterStateTreeIfReady();
	}
}

// 구성 오류로 공격할 수 없거나 이미 죽은 몬스터의 AI가 시작되지 않게 한다.
bool AMonsterCharacter::IsMonsterReadyForAI() const
{
	return IsCharacterRuntimeInitialized() && !bDying
		&& (MonsterAttackMontage.IsNull() || (MonsterAttackMontage.Get() && AttackComponent.IsValid()));
}

#if WITH_EDITOR
// 전투에 필요한 자산과 수치를 검증한다. 즉시 래그돌 사망에는 사망 몽타주가 필요하지 않다.
EDataValidationResult AMonsterCharacter::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	// BP 컴파일 중 새 BP 의존성을 로딩하면 컴파일러에 재진입하므로 이미 로딩된 설정만 검사한다.
	const UEnemyBaseDefinition* Definition = GCompilingBlueprint ? EnemyDefinition.Get() : EnemyDefinition.LoadSynchronous();
	if (EnemyDefinition.IsNull() || (!GCompilingBlueprint && !Definition)
		|| (Definition && (!Definition->GetMonsterPresentationSettings().ContactDamageEffectClass
			|| !Definition->GetMonsterPresentationSettings().HitReactMontage)))
	{
		Context.AddError(FText::FromString(TEXT("Monster EnemyDefinition requires damage and hit-react settings.")));
		Result = EDataValidationResult::Invalid;
	}
	if (MonsterRewardDefinition.IsNull() || (!GCompilingBlueprint && !MonsterRewardDefinition.LoadSynchronous()))
	{
		Context.AddError(FText::FromString(TEXT("MonsterRewardDefinition must reference a loadable reward definition.")));
		Result = EDataValidationResult::Invalid;
	}
	if (!MonsterAttackMontage.IsNull() && ((!GCompilingBlueprint && !MonsterAttackMontage.LoadSynchronous()) || AttackComponentName.IsNone()))
	{
		Context.AddError(FText::FromString(TEXT("A configured monster attack requires a loadable montage and an exact component name.")));
		Result = EDataValidationResult::Invalid;
	}
	if (!FMath::IsFinite(AttackDamageMagnitude) || AttackDamageMagnitude < 0.0f
		|| !FMath::IsFinite(AttackSphereActiveDuration) || AttackSphereActiveDuration < 0.0f
		|| !FMath::IsFinite(DeathDestroyDelay) || DeathDestroyDelay < 0.0f)
	{
		Context.AddError(FText::FromString(TEXT("Monster damage and durations must be finite and non-negative.")));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

// 공격 금지 조건을 먼저 판정하고, 전용 공격을 설정하지 않은 몬스터만 기존 GAS 공격을 사용한다.
void AMonsterCharacter::Attack()
{
	if (!HasAuthority() || !IsMonsterReadyForAI() || !IsAttackEnabled() || IsAttackInProgress()
		|| IsStatusFrozen() || IsTrainingHitStunned() || !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}
	if (MonsterAttackMontage.IsNull())
	{
		Super::Attack();
		return;
	}
	TryPlayMonsterAttackMontage();
}

// StateTree는 실제 시작한 공격만 기다린다. 아직 로딩 중인 요청을 공격 중으로 취급하지 않는다.
bool AMonsterCharacter::IsAttackInProgress() const
{
	return bMonsterAttackActive || Super::IsAttackInProgress();
}

// BP에서 만든 충돌 형태를 유지하면서 참조를 확보하고, 공격 자산을 준비한 뒤 공통 초기화를 진행한다.
void AMonsterCharacter::BeginPlay()
{
	CacheCollisionComponents();
	DeactivateDamageSphere();
	DeactivateAttackSphere();
	BeginMonsterContentPreload();
	Super::BeginPlay();
}

// 먼저 준비된 캐릭터를 나중에 컨트롤러가 조종하게 된 경우에도 AI 시작 조건을 다시 확인한다.
void AMonsterCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// 기존 적의 능력치·시작 장비 준비 경로는 유지한다. 실제 AI 시작은 모든 자산 준비 후로 제한한다.
	if (HasAuthority() && !IsDefaultAttributeSetupComplete())
	{
		InitializeBehaviorTreeCombat();
	}
	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(NewController))
	{
		MonsterController->StartMonsterStateTreeIfReady();
	}
}

// 몬스터 제거 후 공격 판정·로딩 완료·상태 변경 콜백이 남지 않도록 정리한다.
void AMonsterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bDying = true;
	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
	{
		MonsterController->StopMonsterAI();
	}
	DeactivateAttackSphere();
	DeactivateDamageSphere();
	GetWorldTimerManager().ClearTimer(DeathDestroyTimerHandle);
	if (MonsterContentPreloadHandle)
	{
		MonsterContentPreloadHandle->CancelHandle();
		MonsterContentPreloadHandle.Reset();
	}
	if (AbilitySystemComponent && FrozenTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			LabGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved).Remove(FrozenTagChangedHandle);
		FrozenTagChangedHandle.Reset();
	}
	if (UAnimInstance* AnimInstance = AttackAnimInstance.Get())
	{
		AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &ThisClass::HandleAttackMontageBlendingOut);
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &ThisClass::HandleAttackMontageEnded);
	}
	if (UPrimitiveComponent* Collision = AttackComponent.Get())
	{
		Collision->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleAttackComponentBeginOverlap);
	}
	Super::EndPlay(EndPlayReason);
}

// 실제 피해를 준 공격의 수령자를 즉시 확정한다. 빗나감·무적 피해나 이후 Pawn 파괴가 보상 소유자를 바꾸지 않는다.
void AMonsterCharacter::HandleDamageTaken(
	float DamageAmount, bool bCriticalHit, bool bAllowHitReact, AActor* DamageInstigator, AActor* DamageCauser)
{
	if (HasAuthority() && !bDying && FMath::IsFinite(DamageAmount) && DamageAmount > 0.0f)
	{
		APdPlayerState* RewardPlayerState = ResolvePlayerStateFromActor(DamageInstigator);
		LastDamagingPlayerState = RewardPlayerState ? RewardPlayerState : ResolvePlayerStateFromActor(DamageCauser);
	}
	Super::HandleDamageTaken(DamageAmount, bCriticalHit, bAllowHitReact, DamageInstigator, DamageCauser);
	TryPlayMonsterHitReactMontage(DamageAmount, bAllowHitReact);
}

// 작은 몬스터 몸체에 맞춰 피해 숫자의 표시 높이를 낮춘다.
FVector AMonsterCharacter::GetDamageIndicatorWorldLocation() const
{
	FVector Location = Super::GetDamageIndicatorWorldLocation();
	Location.Z = GetActorLocation().Z + (Location.Z - GetActorLocation().Z) * 0.5f;
	return Location;
}

// 공통 능력치 초기화가 끝난 서버에서 몬스터 종류별 최대 체력과 시작 체력을 적용한다.
void AMonsterCharacter::ApplyMonsterHealthDefaults()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}
	const float MaxHealth = FMath::IsFinite(ResolvedMonsterMaxHealth) ? FMath::Max(ResolvedMonsterMaxHealth, 1.0f) : 1.0f;
	AbilitySystemComponent->ApplyAttributeDefaultValue(UBasicAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->ApplyAttributeDefaultValue(UBasicAttributeSet::GetHealthAttribute(), MaxHealth);
	RefreshHealthBarViewModel();
}

// 공격 자산만 미리 읽는다. 보상 로딩은 시체 수명과 무관하게 플레이어 보상 컴포넌트가 소유한다.
void AMonsterCharacter::BeginMonsterContentPreload()
{
	if (bMonsterContentPreloadStarted)
	{
		return;
	}
	bMonsterContentPreloadStarted = true;
	if (MonsterAttackMontage.IsNull() || MonsterAttackMontage.Get())
	{
		HandleMonsterContentPreloadComplete();
		return;
	}
	MonsterContentPreloadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		MonsterAttackMontage.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::HandleMonsterContentPreloadComplete),
		FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (MonsterContentPreloadHandle)
	{
		MonsterContentPreloadHandle->StartStalledHandle();
	}
	else
	{
		HandleMonsterContentPreloadComplete();
	}
}

// 로딩 결과를 초기화 흐름에 전달한다. 과거 StateTree 상태에서 요청한 공격을 뒤늦게 재실행하지 않는다.
void AMonsterCharacter::HandleMonsterContentPreloadComplete()
{
	if (bDying)
	{
		return;
	}
	bMonsterContentReady = true;
	if (!MonsterAttackMontage.IsNull() && !MonsterAttackMontage.Get())
	{
		UE_LOG(LogMonsterCharacter, Error, TEXT("Monster attack montage could not be loaded: %s."), *MonsterAttackMontage.ToString());
	}
	TryInitializeCharacterRuntime();
}

// 중복 사망을 차단하고 AI·공격을 종료한 뒤, 보상 요청과 기존 즉시 래그돌 사망을 각각 진행한다.
void AMonsterCharacter::HandleDeath_Implementation()
{
	if (bDying)
	{
		return;
	}
	bDying = true;
	SetAttackEnabled(false);
	StopMonsterAttack();
	if (AMonsterAIController* MonsterController = Cast<AMonsterAIController>(GetController()))
	{
		MonsterController->StopMonsterAI();
	}
	DeactivateDamageSphere();
	if (HasAuthority())
	{
		if (APdPlayerState* RewardPlayerState = LastDamagingPlayerState.Get())
		{
			if (UPlayerRewardComponent* RewardComponent = RewardPlayerState->GetPlayerRewardComponent())
			{
				RewardComponent->GrantMonsterDefeatRewards(MonsterRewardDefinition);
			}
		}
	}
	Super::HandleDeath_Implementation();
	const float DestroyDelay = FMath::IsFinite(DeathDestroyDelay) ? FMath::Max(DeathDestroyDelay, 0.01f) : 0.01f;
	StartDeathDissolve(DestroyDelay);
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DeathDestroyTimerHandle, this, &ThisClass::FinishMonsterDeath, DestroyDelay, false);
	}
}

// 시체 표시 시간이 끝나면 제거한다. 보상 로딩 때문에 몬스터를 남겨 두지 않는다.
void AMonsterCharacter::FinishMonsterDeath()
{
	Destroy();
}

// 기존 GAS 공격이 몬스터 몽타주를 조회하는 호환 경로를 유지한다.
bool AMonsterCharacter::GetFallbackAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();
	OutAttackData.AttackMontage = MonsterAttackMontage.Get();
	return OutAttackData.AttackMontage != nullptr;
}

// 대상에게 접근하고 서버에서 몽타주가 실제 시작된 경우에만 타격 판정을 연다.
bool AMonsterCharacter::TryPlayMonsterAttackMontage()
{
	AActor* Target = ITargetingInterface::Execute_GetAttackTarget(this);
	if (!IsActorValidAttackTarget(Target))
	{
		return false;
	}
	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator Rotation = ToTarget.Rotation();
		if (AController* CurrentController = GetController())
		{
			CurrentController->SetControlRotation(Rotation);
		}
		SetActorRotation(Rotation);
	}
	if (MoveToAttackTarget(Target))
	{
		return false;
	}
	UAnimMontage* Montage = MonsterAttackMontage.Get();
	if (!PlayMonsterAttackMontageLocal(Montage, 1.0f))
	{
		return false;
	}
	MulticastPlayMonsterAttackMontage(Montage, 1.0f);
	ActivateAttackSphere();
	return true;
}

// 몽타주 종료·중단을 공격 상태와 연결한다. 재생 실패 시에는 피해 판정을 시작하지 않는다.
bool AMonsterCharacter::PlayMonsterAttackMontageLocal(UAnimMontage* AttackMontage, float PlayRate)
{
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (bDying || !AttackMontage || !AnimInstance || PlayAnimMontage(AttackMontage, PlayRate) <= 0.0f)
	{
		return false;
	}
	if (UAnimInstance* PreviousInstance = AttackAnimInstance.Get(); PreviousInstance && PreviousInstance != AnimInstance)
	{
		PreviousInstance->OnMontageBlendingOut.RemoveDynamic(this, &ThisClass::HandleAttackMontageBlendingOut);
		PreviousInstance->OnMontageEnded.RemoveDynamic(this, &ThisClass::HandleAttackMontageEnded);
	}
	AttackAnimInstance = AnimInstance;
	AnimInstance->OnMontageBlendingOut.AddUniqueDynamic(this, &ThisClass::HandleAttackMontageBlendingOut);
	AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::HandleAttackMontageEnded);
	bMonsterAttackActive = true;
	return true;
}

// 공격 모션이 끝나거나 다른 모션에 끊기는 순간 남아 있는 피해 판정을 닫는다.
void AMonsterCharacter::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == MonsterAttackMontage.Get())
	{
		DeactivateAttackSphere();
	}
}

// 공격 몽타주가 완전히 끝났음을 StateTree의 공격 완료 조회에 반영한다.
void AMonsterCharacter::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == MonsterAttackMontage.Get())
	{
		bMonsterAttackActive = false;
		DeactivateAttackSphere();
	}
}

// 상태 전환·빙결·사망으로 취소한 공격의 모션과 판정을 서버와 클라이언트에서 함께 종료한다.
void AMonsterCharacter::StopMonsterAttack()
{
	DeactivateAttackSphere();
	if (HasAuthority() && bMonsterAttackActive)
	{
		MulticastStopMonsterAttack();
	}
}

// 빙결은 이동 여부 설정과 관계없이 이미 시작한 몬스터 공격도 중단시킨다.
void AMonsterCharacter::HandleFrozenTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		StopMonsterAttack();
	}
}

// 살아남은 피격에만 반응하며, 서버 종류와 관계없이 공격 중단 결과를 동일하게 처리한다.
void AMonsterCharacter::TryPlayMonsterHitReactMontage(float DamageAmount, bool bAllowHitReact)
{
	if (!HasAuthority() || bDying || !bAllowHitReact || DamageAmount <= 0.0f || !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}
	const UBasicAttributeSet* Attributes = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	UAnimMontage* HitReactMontage = MonsterPresentationSettings.HitReactMontage;
	if (!Attributes || Attributes->GetHealth() <= DamageAmount || !HitReactMontage)
	{
		return;
	}
	StopMonsterAttack();
	MulticastPlayMonsterHitReactMontage(HitReactMontage, MonsterPresentationSettings.HitReactPlayRate);
}

// BP 충돌 컴포넌트를 정확한 이름으로 한 번만 연결한다. 비슷한 이름의 다른 컴포넌트로 대체하지 않는다.
void AMonsterCharacter::CacheCollisionComponents()
{
	TInlineComponentArray<UPrimitiveComponent*> Components(this);
	for (UPrimitiveComponent* Component : Components)
	{
		if (Component->GetFName() == AttackComponentName)
		{
			AttackComponent = Component;
		}
		if (Component->GetFName() == DamageComponentName)
		{
			LegacyDamageComponent = Component;
		}
	}
	if (UPrimitiveComponent* Collision = AttackComponent.Get())
	{
		Collision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleAttackComponentBeginOverlap);
	}
	else if (!MonsterAttackMontage.IsNull())
	{
		UE_LOG(LogMonsterCharacter, Error, TEXT("%s requires an attack component named exactly '%s'."),
			*GetPathName(), *AttackComponentName.ToString());
	}
}

// 공격 판정에 들어온 캐릭터를 서버의 1회 타격 처리로 전달한다.
void AMonsterCharacter::HandleAttackComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsValidMonsterDamageTarget(OtherActor))
	{
		ApplyAttackDamageToCharacter(Cast<ACharacterBase>(OtherActor));
	}
}

// 자신·같은 팀·사망한 캐릭터를 몬스터의 타격 대상에서 제외한다.
bool AMonsterCharacter::IsValidMonsterDamageTarget(const AActor* OtherActor) const
{
	const ACharacterBase* Character = Cast<ACharacterBase>(OtherActor);
	if (!IsValid(Character) || Character == this || !CanDamageCharacterByTeam(Character))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	return ASC && !ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
}

// 한 번의 공격에서 같은 캐릭터에게 중복 피해를 주지 않으며, 중단된 공격의 늦은 오버랩을 무시한다.
void AMonsterCharacter::ApplyAttackDamageToCharacter(ACharacterBase* TargetCharacter)
{
	if (!HasAuthority() || bDying || !bMonsterAttackActive || !bAttackWindowOpen || !IsAttackEnabled() || IsStatusFrozen()
		|| !IsValidMonsterDamageTarget(TargetCharacter) || AttackHitActorsThisSwing.Contains(TargetCharacter))
	{
		return;
	}
	AttackHitActorsThisSwing.Add(TargetCharacter);
	ApplyMonsterDamageToCharacter(TargetCharacter);
}

// 설정된 몬스터 피해 GE를 적용한다. 플레이어의 피격 모션 억제 규칙은 기존대로 유지한다.
bool AMonsterCharacter::ApplyMonsterDamageToCharacter(ACharacterBase* TargetCharacter)
{
	if (!MonsterPresentationSettings.ContactDamageEffectClass || !FMath::IsFinite(AttackDamageMagnitude) || AttackDamageMagnitude <= 0.0f)
	{
		return false;
	}
	UPdAbilitySystemComponent* SourceASC = GetEnemyAbilitySystemComponent();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter->GetPdAbilitySystemComponent();
	if (!SourceASC || !TargetASC)
	{
		return false;
	}
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(this, this);
	Context.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(MonsterPresentationSettings.ContactDamageEffectClass, 1.0f, Context);
	FGameplayTag DamageTag = ContactDamageDataTag;
	if (!DamageTag.IsValid())
	{
		SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageTag);
	}
	if (!Spec.IsValid() || !DamageTag.IsValid())
	{
		return false;
	}
	UBasicAttributeSet* TargetAttributes = const_cast<UBasicAttributeSet*>(TargetASC->GetSet<UBasicAttributeSet>());
	const bool bSuppressHitReact = TargetCharacter->IsA<APdPlayer>() && TargetAttributes;
	if (bSuppressHitReact)
	{
		TargetAttributes->SetPendingIncomingDamageAllowHitReact(false);
	}
	Spec.Data->SetSetByCallerMagnitude(DamageTag, AttackDamageMagnitude);
	const bool bApplied = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied();
	if (bSuppressHitReact)
	{
		TargetAttributes->SetPendingIncomingDamageAllowHitReact(true);
	}
	return bApplied;
}

// 과거 BP의 상시 접촉 피해 충돌은 비활성화해 공격 판정과 중복되지 않게 한다.
void AMonsterCharacter::DeactivateDamageSphere()
{
	if (UPrimitiveComponent* Collision = LegacyDamageComponent.Get())
	{
		Collision->OnComponentBeginOverlap.RemoveAll(this);
		Collision->SetGenerateOverlapEvents(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// 공격 시작 때 이미 범위 안에 있던 대상도 포함해 기존 시간만큼 타격 창을 연다.
void AMonsterCharacter::ActivateAttackSphere()
{
	UPrimitiveComponent* Collision = AttackComponent.Get();
	if (!HasAuthority() || bDying || !bMonsterAttackActive || !Collision)
	{
		return;
	}
	AttackHitActorsThisSwing.Reset();
	bAttackWindowOpen = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetGenerateOverlapEvents(true);
	Collision->UpdateOverlaps();
	TArray<AActor*> OverlappingActors;
	Collision->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* Actor : OverlappingActors)
	{
		if (IsValidMonsterDamageTarget(Actor))
		{
			ApplyAttackDamageToCharacter(Cast<ACharacterBase>(Actor));
		}
	}
	if (bDying || !bMonsterAttackActive)
	{
		return;
	}
	const float ActiveDuration = FMath::IsFinite(AttackSphereActiveDuration) ? FMath::Max(AttackSphereActiveDuration, 0.01f) : 0.01f;
	GetWorldTimerManager().SetTimer(AttackSphereTimerHandle, this, &ThisClass::DeactivateAttackSphere, ActiveDuration, false);
}

// 공격 시간이 끝나거나 취소되면 충돌과 이번 공격의 타격 기록을 정리한다.
void AMonsterCharacter::DeactivateAttackSphere()
{
	bAttackWindowOpen = false;
	GetWorldTimerManager().ClearTimer(AttackSphereTimerHandle);
	if (UPrimitiveComponent* Collision = AttackComponent.Get())
	{
		Collision->SetGenerateOverlapEvents(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	AttackHitActorsThisSwing.Reset();
}

// 발사체·무기 등에서 전달된 공격자 정보를 현재 공격에 해당하는 RewardPlayerState로 해석한다.
APdPlayerState* AMonsterCharacter::ResolvePlayerStateFromActor(AActor* Actor) const
{
	for (AActor* Source = Actor; IsValid(Source); Source = Source->GetOwner())
	{
		if (APdPlayerState* RewardPlayerState = Cast<APdPlayerState>(Source))
		{
			return RewardPlayerState;
		}
		if (const APawn* Pawn = Cast<APawn>(Source))
		{
			if (APdPlayerState* RewardPlayerState = Pawn->GetPlayerState<APdPlayerState>())
			{
				return RewardPlayerState;
			}
		}
		if (const AController* SourceController = Cast<AController>(Source))
		{
			if (APdPlayerState* RewardPlayerState = Cast<APdPlayerState>(SourceController->PlayerState))
			{
				return RewardPlayerState;
			}
		}
		if (const APawn* InstigatorPawn = Source->GetInstigator())
		{
			if (APdPlayerState* RewardPlayerState = InstigatorPawn->GetPlayerState<APdPlayerState>())
			{
				return RewardPlayerState;
			}
		}
	}
	return nullptr;
}

// 서버에서 이미 시작한 공격 몽타주를 원격 화면에만 재생한다.
void AMonsterCharacter::MulticastPlayMonsterAttackMontage_Implementation(UAnimMontage* AttackMontage, float PlayRate)
{
	if (!HasAuthority())
	{
		PlayMonsterAttackMontageLocal(AttackMontage, PlayRate);
	}
}

// 취소된 공격이 원격 화면이나 충돌 판정에 남지 않게 한다.
void AMonsterCharacter::MulticastStopMonsterAttack_Implementation()
{
	bMonsterAttackActive = false;
	DeactivateAttackSphere();
	if (UAnimMontage* Montage = MonsterAttackMontage.Get())
	{
		StopAnimMontage(Montage);
	}
}

// 피격 표현은 렌더링하는 인스턴스에서만 재생한다. 공격 중단 여부는 서버에서 이미 확정했다.
void AMonsterCharacter::MulticastPlayMonsterHitReactMontage_Implementation(UAnimMontage* HitReactMontage, float PlayRate)
{
	if (!bDying && HitReactMontage && GetNetMode() != NM_DedicatedServer)
	{
		PlayAnimMontage(HitReactMontage, FMath::Max(PlayRate, 0.01f));
	}
}
