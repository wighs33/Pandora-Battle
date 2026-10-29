#include "AI/Monster/MonsterCharacter.h"

#include "AI/Monster/MonsterAIController.h"
#include "AI/Monster/MonsterSignificanceSubsystem.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_AttackDamageWindow.h"
#include "Character/PdPlayer.h"
#include "AbilitySystem/Ability/EquipmentAbilityData.h"
#include "Common/PdGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Engine/PostProcessVolume.h"
#include "Definition/Item/RewardDefinition.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Mode/PdPlayerState.h"
#include "Mode/WorldBossRaid.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogMonsterCharacter, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarPhaseSummonCountOverride(TEXT("pd.Monster.PhaseSummonCountOverride"), -1,
		TEXT("보스 페이즈 소환 수를 바꾼다. 0 이상이면 데이터의 수 대신 쓴다. 대규모 몬스터 부하 측정용."), ECVF_Cheat);

	TAutoConsoleVariable<float> CVarSummonSpawnBudgetMs(TEXT("pd.Monster.SummonSpawnBudgetMs"), 4.0f,
		TEXT("졸개 소환에 한 프레임에서 쓸 시간(ms). 이 시간을 넘기면 남은 졸개는 다음 프레임에 만든다. 프레임마다 적어도 한 마리는 만든다."));

	/** 졸개를 보스 몸 바깥 첫 고리부터 이 간격으로 해바라기 씨앗처럼 펼쳐 놓는다. */
	constexpr float MinionSpawnGapFromBoss = 300.0f;
	constexpr float MinionSpawnSpacing = 150.0f;
	constexpr float GoldenAngleRadians = 2.39996323f;

	/** 한꺼번에 죽은 시체를 이 범위 안에서 흩어 지운다. 디졸브가 끝난 뒤라 화면에는 차이가 없다. */
	constexpr float CorpseDestroyJitterSeconds = 1.0f;
	/** 부채꼴 장판 공격이 피해 구간 동안 범위를 다시 검사하는 간격. */
	constexpr float TelegraphAreaCheckInterval = 0.1f;

	const TCHAR* const MaxLeashPropertyName = TEXT("Max Leash Distance From Spawn Point");
	const TCHAR* const MinLeashPropertyName = TEXT("Min Leash Distance From Spawn Point To Resume Roaming");

	FString NormalizePropertyName(FString Name)
	{
		Name.ReplaceInline(TEXT(" "), TEXT(""));
		Name.ReplaceInline(TEXT("_"), TEXT(""));
		Name.ToLowerInline();
		return Name;
	}

	/** BP 변수는 표시 이름에 공백이 있어 이름을 정규화해 찾는다. */
	FNumericProperty* FindNumericProperty(const UClass* Class, const TCHAR* RequestedName)
	{
		const FString NormalizedRequestedName = NormalizePropertyName(RequestedName);
		for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIterationFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			if (NormalizePropertyName(PropertyIt->GetName()) == NormalizedRequestedName
				|| NormalizePropertyName(PropertyIt->GetAuthoredName()) == NormalizedRequestedName)
			{
				return CastField<FNumericProperty>(*PropertyIt);
			}
		}
		return nullptr;
	}
	/** 몽타주가 직접 피해 구간 노티파이를 가지고 있으면 그 구간이 판정 시점을 정한다. */
	bool HasAttackDamageWindow(const UAnimMontage& Montage)
	{
		return Montage.Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Notify)
		{
			return Notify.NotifyStateClass && Notify.NotifyStateClass->IsA<UAnimNotifyState_AttackDamageWindow>();
		});
	}
}

float FMonsterAttackPattern::GetDistanceOutsideRange(const float Distance) const
{
	if (Distance < MinDistance)
	{
		return MinDistance - Distance;
	}
	return MaxDistance > 0.0f && Distance > MaxDistance ? Distance - MaxDistance : 0.0f;
}

// 몬스터는 서버에서 생성하고, 자동 공격 타이머 대신 StateTree가 공격 시점을 결정한다.
AMonsterCharacter::AMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	SetReplicateMovement(true);
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	ContactDamageDataTag = PdGameplayTags::Data_Damage;
	// 공격 예고 장판 데칼이 몬스터 몸에 묻지 않게 한다. 장판 안에 선 플레이어에게는 묻어 위험을 알린다.
	GetMesh()->bReceivesDecals = false;
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
			PdGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleFrozenTagChanged);
		BindAttackDamageWindowEvents();
		// 피격 모션은 몬스터 스켈레톤용 몽타주로 직접 재생한다. 사람 스켈레톤용 공용 피격 능력은 몬스터 몸을 기본 자세로 튀게 하므로 막는다.
		AbilitySystemComponent->BlockAbilitiesWithTags(FGameplayTagContainer(PdGameplayTags::Action_HitReact));
		if (PhaseSummon.HealthRatio > 0.0f && !HealthChangedHandle.IsValid())
		{
			HealthChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ThisClass::HandleHealthChanged);
		}
		// 페이즈 소환 연결 뒤에 전역 체력을 받아야, 이미 절반 아래로 깎인 회차에서도 소환한다.
		AWorldBossRaid* WorldBossRaid = bWorldBoss ? AWorldBossRaid::Find(this) : nullptr;
		if (WorldBossRaid)
		{
			WorldBossRaid->RegisterBoss(*this);
		}
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
		&& (MonsterAttacks.IsEmpty() || (FindFirstLoadedAttackMontage() && AttackComponent.IsValid()));
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
		Context.AddError(NSLOCTEXT("MonsterCharacter", "MissingEnemyCombatSettings",
			"Monster EnemyDefinition requires damage and hit-react settings."));
		Result = EDataValidationResult::Invalid;
	}
	if (MonsterRewardDefinition.IsNull() || (!GCompilingBlueprint && !MonsterRewardDefinition.LoadSynchronous()))
	{
		Context.AddError(NSLOCTEXT("MonsterCharacter", "InvalidRewardDefinition",
			"MonsterRewardDefinition must reference a loadable reward definition."));
		Result = EDataValidationResult::Invalid;
	}
	const bool bHasInvalidAttack = MonsterAttacks.ContainsByPredicate([](const FMonsterAttackPattern& Attack)
	{
		return Attack.Montage.IsNull() || (!GCompilingBlueprint && !Attack.Montage.LoadSynchronous())
			|| (Attack.MaxDistance > 0.0f && Attack.MaxDistance < Attack.MinDistance);
	});
	if (bHasInvalidAttack || (!MonsterAttacks.IsEmpty() && AttackComponentName.IsNone()))
	{
		Context.AddError(NSLOCTEXT("MonsterCharacter", "InvalidAttackPattern",
			"Every monster attack needs a loadable montage, a valid distance range and an exact attack component name."));
		Result = EDataValidationResult::Invalid;
	}
	if (PhaseSummon.HealthRatio > 0.0f && (PhaseSummon.MinionClass.IsNull() || PhaseSummon.Count <= 0))
	{
		Context.AddError(NSLOCTEXT("MonsterCharacter", "InvalidPhaseSummon",
			"A phase summon needs a minion class and a positive count."));
		Result = EDataValidationResult::Invalid;
	}
	if (!FMath::IsFinite(AttackDamageMagnitude) || AttackDamageMagnitude < 0.0f
		|| !FMath::IsFinite(AttackSphereActiveDuration) || AttackSphereActiveDuration < 0.0f
		|| !FMath::IsFinite(HitReactCooldown) || HitReactCooldown < 0.0f
		|| !FMath::IsFinite(SightRadiusOverride) || SightRadiusOverride < 0.0f
		|| !FMath::IsFinite(DeathDestroyDelay) || DeathDestroyDelay < 0.0f)
	{
		Context.AddError(NSLOCTEXT("MonsterCharacter", "InvalidCombatValues",
			"Monster damage and durations must be finite and non-negative."));
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
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::State_Dead))
	{
		return;
	}
	if (MonsterAttacks.IsEmpty())
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
	ApplyMeshOptimizations();
	RefreshHighlightStencil();
	EnsureHighlightPostProcess();
	DeactivateDamageSphere();
	DeactivateAttackSphere();
	BeginMonsterContentPreload();
	if (UMonsterSignificanceSubsystem* Significance = GetWorld()->GetSubsystem<UMonsterSignificanceSubsystem>())
	{
		Significance->RegisterMonster(this);
	}
	Super::BeginPlay();
}

// 몬스터는 수백 마리가 한꺼번에 나오므로, 메시 하나의 작은 비용도 그만큼 곱해진다. 몬스터 메시에만 맞는 갱신 정책을 켠다.
void AMonsterCharacter::ApplyMeshOptimizations()
{
	USkeletalMeshComponent* MonsterMesh = GetMesh();
	if (!MonsterMesh)
	{
		return;
	}
	// 캡슐이 움직이거나 자세가 바뀔 때마다 피격 판정용 물리 바디를 하나씩 옮기지 않고, 물리 시작 전에 모든 몬스터의 바디를 한꺼번에(병렬로) 옮긴다.
	// 서버의 플레이어 공격 검증 RPC는 프레임 시작에 처리되므로, 그때 바디는 지난 프레임 끝 위치로 이미 맞춰져 있다.
	MonsterMesh->bDeferKinematicBoneUpdate = true;

	// 엔진의 애니메이션 갱신 주기 최적화(URO)는 화면에서 작게 보이는 메시일수록 몇 프레임에 한 번만 평가하고, 그려지지 않는 메시는
	// BaseNonRenderedUpdateRate 프레임마다 갱신한다. 전용 서버의 메시는 늘 그려지지 않으므로 그 주기를 몬스터별 서버 갱신 간격으로 둔다.
	// 몬스터마다 갱신하는 프레임이 엇갈려 있어 한 프레임에 몰리지 않는다.
	if (!IsRunningDedicatedServer())
	{
		MonsterMesh->bEnableUpdateRateOptimizations = true;
	}
	else if (ServerAnimationUpdateInterval > 1 && MonsterMesh->AnimUpdateRateParams)
	{
		MonsterMesh->bEnableUpdateRateOptimizations = true;
		MonsterMesh->AnimUpdateRateParams->BaseNonRenderedUpdateRate = ServerAnimationUpdateInterval;
	}
}

// 공격 대상 쪽으로 도는 중이면 회전 속도대로 몸을 돌린다. 정면을 보면 멈추고, 이동을 시작하면 이동 방향 회전에 맡긴다.
void AMonsterCharacter::Tick(const float DeltaSeconds)
{
	if (const AActor* Target = AttackTurnTarget.Get())
	{
		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		const float TargetYaw = ToTarget.Rotation().Yaw;
		if (bDying || GetVelocity().SizeSquared2D() > 1.0f || FMath::Abs(FRotator::NormalizeAxis(TargetYaw - GetActorRotation().Yaw)) < 1.0f)
		{
			AttackTurnTarget.Reset();
		}
		else
		{
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0.0f, TargetYaw, 0.0f), DeltaSeconds,
				GetCharacterMovement()->RotationRate.Yaw));
		}
	}
	Super::Tick(DeltaSeconds);
}

bool AMonsterCharacter::ShouldUseContinuousCharacterTick() const
{
	return AttackTurnTarget.IsValid() || Super::ShouldUseContinuousCharacterTick();
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
	GetWorldTimerManager().ClearTimer(MinionSpawnTimerHandle);
	MonsterContentLease.Reset();
	if (UMonsterSignificanceSubsystem* Significance = GetWorld()->GetSubsystem<UMonsterSignificanceSubsystem>())
	{
		Significance->UnregisterMonster(this);
	}
	UnbindAttackDamageWindowEvents();
	if (AbilitySystemComponent && HealthChangedHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		HealthChangedHandle.Reset();
	}
	if (AbilitySystemComponent && FrozenTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			PdGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved).Remove(FrozenTagChangedHandle);
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
		if (bWorldBoss)
		{
			RecordWorldBossDamage(DamageAmount);
		}
	}
	Super::HandleDamageTaken(DamageAmount, bCriticalHit, bAllowHitReact, DamageInstigator, DamageCauser);
	TryPlayMonsterHitReactMontage(DamageAmount, bAllowHitReact);
}

// 피해 표시값에는 남은 체력을 넘는 양(오버킬)이 들어 있다. 순위에는 이번 공격으로 실제로 깎인 체력만 넣는다.
// 이 함수는 체력을 깎기 직전에 불리므로 지금 체력이 깎일 수 있는 최대치다.
void AMonsterCharacter::RecordWorldBossDamage(const float DamageAmount) const
{
	AWorldBossRaid* WorldBossRaid = AWorldBossRaid::Find(this);
	if (!WorldBossRaid || !AbilitySystemComponent)
	{
		return;
	}
	const float Health = AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute());
	WorldBossRaid->RecordBossDamage(LastDamagingPlayerState.Get(), FMath::Min(DamageAmount, Health));
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
	// 소환 순간에 동기 로딩이 생기지 않도록 페이즈 소환 자산도 함께 미리 읽는다.
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FMonsterAttackPattern& Attack : MonsterAttacks)
	{
		AssetsToLoad.Add(Attack.Montage.ToSoftObjectPath());
	}
	if (PhaseSummon.HealthRatio > 0.0f)
	{
		AssetsToLoad.Add(PhaseSummon.MinionClass.ToSoftObjectPath());
		AssetsToLoad.Add(PhaseSummon.RoarMontage.ToSoftObjectPath());
	}
	AssetsToLoad.RemoveAll([](const FSoftObjectPath& Path) { return Path.IsNull() || Path.ResolveObject(); });
	if (AssetsToLoad.IsEmpty())
	{
		HandleMonsterContentPreloadComplete();
		return;
	}
	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		HandleMonsterContentPreloadComplete();
		return;
	}
	MonsterContentLease = ContentSubsystem->AcquireContent(AssetsToLoad,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleMonsterContentPreloadComplete));
}

// 로딩 결과를 초기화 흐름에 전달한다. 과거 StateTree 상태에서 요청한 공격을 뒤늦게 재실행하지 않는다.
void AMonsterCharacter::HandleMonsterContentPreloadComplete()
{
	if (bDying)
	{
		return;
	}
	bMonsterContentReady = true;
	for (const FMonsterAttackPattern& Attack : MonsterAttacks)
	{
		if (!Attack.Montage.Get())
		{
			UE_LOG(LogMonsterCharacter, Error, TEXT("Monster attack montage could not be loaded: %s."), *Attack.Montage.ToString());
		}
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
	RefreshHighlightStencil();
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
		// 범위 공격으로 수백 마리가 같은 프레임에 죽으면 시체도 같은 프레임에 사라져 서버가 멈춘다. 지우는 시각을 조금씩 흩는다.
		GetWorldTimerManager().SetTimer(DeathDestroyTimerHandle, this, &ThisClass::FinishMonsterDeath,
			DestroyDelay + FMath::FRandRange(0.0f, CorpseDestroyJitterSeconds), false);
	}
}

// 시체 표시 시간이 끝나면 제거한다. 보상 로딩 때문에 몬스터를 남겨 두지 않는다.
void AMonsterCharacter::FinishMonsterDeath()
{
	Destroy();
}

void AMonsterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AMonsterCharacter, ReplicatedAttackTarget, Params);
}

// 서버는 AI가 고른 대상을, 원격 화면은 서버가 복제한 대상을 돌려준다.
AActor* AMonsterCharacter::GetAttackTarget_Implementation() const
{
	return HasAuthority() ? Super::GetAttackTarget_Implementation() : ReplicatedAttackTarget.Get();
}

void AMonsterCharacter::SetReplicatedAttackTarget(AActor* Target)
{
	if (HasAuthority() && ReplicatedAttackTarget != Target)
	{
		ReplicatedAttackTarget = Target;
		MARK_PROPERTY_DIRTY_FROM_NAME(AMonsterCharacter, ReplicatedAttackTarget, this);
	}
}

// 기존 GAS 공격이 몬스터 몽타주를 조회하는 호환 경로를 유지한다.
bool AMonsterCharacter::GetFallbackAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();
	OutAttackData.AttackMontage = FindFirstLoadedAttackMontage();
	return OutAttackData.AttackMontage != nullptr;
}

// 대상에게 접근하고 서버에서 몽타주가 실제 시작된 경우에만 타격 판정을 연다.
// 피해 구간 노티파이가 있는 몽타주는 그 구간이 판정을 열고 닫으며, 없는 몽타주는 시작하자마자 정해진 시간만큼 연다.
bool AMonsterCharacter::TryPlayMonsterAttackMontage()
{
	AActor* Target = ITargetingInterface::Execute_GetAttackTarget(this);
	if (!IsActorValidAttackTarget(Target))
	{
		return false;
	}
	if (MoveToAttackTarget(Target))
	{
		AttackTurnTarget.Reset();
		return false;
	}
	if (!FaceAttackTarget(*Target))
	{
		return false;
	}
	UAnimMontage* Montage = PickAttackMontage(GetAttackDistanceToActor(Target));
	if (!PlayMonsterAttackMontageLocal(Montage, 1.0f))
	{
		return false;
	}
	MulticastPlayMonsterAttackMontage(Montage, 1.0f, Target);
	if (!HasAttackDamageWindow(*Montage))
	{
		ActivateAttackSphere();
		const float ActiveDuration = FMath::IsFinite(AttackSphereActiveDuration) ? FMath::Max(AttackSphereActiveDuration, 0.01f) : 0.01f;
		GetWorldTimerManager().SetTimer(AttackSphereTimerHandle, this, &ThisClass::DeactivateAttackSphere, ActiveDuration, false);
	}
	return true;
}

// 대상 쪽으로 몸을 돌린다. 허용 각도가 0이면 바로 돌려 세우고, 아니면 각도 안에 들어올 때까지 Tick에서 회전 속도대로 돌린다.
bool AMonsterCharacter::FaceAttackTarget(AActor& Target)
{
	FVector ToTarget = Target.GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}
	const FRotator TargetRotation = ToTarget.Rotation();
	if (AController* CurrentController = GetController())
	{
		CurrentController->SetControlRotation(TargetRotation);
	}
	if (AttackFacingTolerance <= 0.0f)
	{
		SetActorRotation(TargetRotation);
		return true;
	}
	if (FMath::Abs(FRotator::NormalizeAxis(TargetRotation.Yaw - GetActorRotation().Yaw)) <= AttackFacingTolerance)
	{
		AttackTurnTarget.Reset();
		return true;
	}
	AttackTurnTarget = &Target;
	RefreshCharacterTickEnabled();
	return false;
}

// 불러온 공격 중 대상 거리에 가장 맞는 것들을 모아 하나를 고른다. 범위 안의 공격이 있으면 그중에서, 없으면 범위에 가장 가까운 공격 중에서 고른다.
UAnimMontage* AMonsterCharacter::PickAttackMontage(const float TargetDistance) const
{
	TArray<UAnimMontage*, TInlineAllocator<4>> BestMontages;
	float BestOutsideDistance = TNumericLimits<float>::Max();
	for (const FMonsterAttackPattern& Attack : MonsterAttacks)
	{
		UAnimMontage* Montage = Attack.Montage.Get();
		if (!Montage)
		{
			continue;
		}
		const float OutsideDistance = Attack.GetDistanceOutsideRange(TargetDistance);
		if (OutsideDistance < BestOutsideDistance)
		{
			BestOutsideDistance = OutsideDistance;
			BestMontages.Reset();
		}
		if (OutsideDistance == BestOutsideDistance)
		{
			BestMontages.Add(Montage);
		}
	}
	return BestMontages.IsEmpty() ? nullptr : BestMontages[FMath::RandHelper(BestMontages.Num())];
}

float AMonsterCharacter::GetMaxAttackPatternDistance() const
{
	float MaxDistance = 0.0f;
	for (const FMonsterAttackPattern& Attack : MonsterAttacks)
	{
		if (Attack.MaxDistance <= 0.0f)
		{
			return 0.0f;
		}
		MaxDistance = FMath::Max(MaxDistance, Attack.MaxDistance);
	}
	return MaxDistance;
}

UAnimMontage* AMonsterCharacter::FindFirstLoadedAttackMontage() const
{
	for (const FMonsterAttackPattern& Attack : MonsterAttacks)
	{
		if (UAnimMontage* Montage = Attack.Montage.Get())
		{
			return Montage;
		}
	}
	return nullptr;
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
	CurrentAttackMontage = AttackMontage;
	bMonsterAttackActive = true;
	return true;
}

// 공격 모션이 끝나거나 다른 모션에 끊기는 순간 남아 있는 피해 판정을 닫는다.
void AMonsterCharacter::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == CurrentAttackMontage.Get())
	{
		DeactivateAttackSphere();
	}
}

// 공격 몽타주가 완전히 끝났음을 StateTree의 공격 완료 조회에 반영한다.
void AMonsterCharacter::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == CurrentAttackMontage.Get())
	{
		bMonsterAttackActive = false;
		DeactivateAttackSphere();
	}
}

// 상태 전환·빙결·사망으로 취소한 공격의 모션과 판정을 서버와 클라이언트에서 함께 종료한다.
void AMonsterCharacter::StopMonsterAttack()
{
	AttackTurnTarget.Reset();
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

// 체력이 처음으로 페이즈 비율 미만이 되는 순간 한 번만 소환한다. 이번 피해로 죽었으면 부르지 않는다.
void AMonsterCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!HasPendingPhaseSummon() || Data.NewValue <= 0.0f || !AbilitySystemComponent)
	{
		return;
	}
	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth > 0.0f && Data.NewValue < MaxHealth * PhaseSummon.HealthRatio)
	{
		TriggerPhaseSummon();
	}
}

bool AMonsterCharacter::TriggerPhaseSummon()
{
	if (!HasAuthority() || !HasPendingPhaseSummon())
	{
		return false;
	}
	bPhaseSummonDone = true;
	StartPhaseSummon();
	return true;
}

// 포효는 공격 몽타주와 같은 길로 재생해 AI가 끝날 때까지 기다리게 한다. 졸개는 포효하는 동안 프레임 예산에 맞춰 나눠서 만든다.
void AMonsterCharacter::StartPhaseSummon()
{
	if (UAnimMontage* Roar = PhaseSummon.RoarMontage.Get())
	{
		StopMonsterAttack();
		if (PlayMonsterAttackMontageLocal(Roar, 1.0f))
		{
			MulticastPlayMonsterAttackMontage(Roar, 1.0f, nullptr);
		}
	}
	const int32 CountOverride = CVarPhaseSummonCountOverride.GetValueOnGameThread();
	MinionCountToSpawn = CountOverride >= 0 ? CountOverride : PhaseSummon.Count;
	SpawnedMinionCount = 0;
	if (PhaseSummon.MinionClass.Get() && MinionCountToSpawn > 0)
	{
		SpawnPendingMinions();
	}
}

// 졸개 하나를 만드는 데 스폰, 컴포넌트 등록, AI 컨트롤러 생성, BeginPlay가 모두 들어가 수백 마리를 한 프레임에 만들면 서버가 수백 ms 멈춘다.
// 그래서 한 프레임에 예산 시간만큼만 만들고, 남은 졸개는 다음 프레임으로 넘긴다.
void AMonsterCharacter::SpawnPendingMinions()
{
	if (bDying)
	{
		return;
	}
	const double Deadline = FPlatformTime::Seconds() + CVarSummonSpawnBudgetMs.GetValueOnGameThread() * 0.001;
	do
	{
		SpawnMinion(SpawnedMinionCount++);
	}
	while (SpawnedMinionCount < MinionCountToSpawn && FPlatformTime::Seconds() < Deadline);

	if (SpawnedMinionCount < MinionCountToSpawn)
	{
		MinionSpawnTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::SpawnPendingMinions);
	}
	else
	{
		UE_LOG(LogMonsterCharacter, Log, TEXT("%s summoned %d minions."), *GetName(), SpawnedMinionCount);
	}
}

// 보스 둘레 내비메시 위에 졸개를 해바라기 씨앗처럼 펼쳐 놓는다. 추적 범위는 보스와 같게 해 같은 경기장에서 싸우게 한다.
void AMonsterCharacter::SpawnMinion(const int32 Index)
{
	UClass* MinionClass = PhaseSummon.MinionClass.Get();
	UWorld* World = GetWorld();
	if (!MinionClass || !World)
	{
		return;
	}
	const float FirstRingDistance = GetCapsuleComponent()->GetScaledCapsuleRadius() + MinionSpawnGapFromBoss;
	const float Angle = Index * GoldenAngleRadians;
	const float Distance = FirstRingDistance + MinionSpawnSpacing * FMath::Sqrt(static_cast<float>(Index));
	FVector Location = GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Distance;
	FNavLocation NavLocation;
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavigationSystem && NavigationSystem->ProjectPointToNavigation(Location, NavLocation, FVector(300.0f, 300.0f, 1000.0f)))
	{
		const float MinionHalfHeight = MinionClass->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Location = NavLocation.Location + FVector(0.0f, 0.0f, MinionHalfHeight);
	}
	const FTransform SpawnTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), Location);
	AMonsterCharacter* Minion = World->SpawnActorDeferred<AMonsterCharacter>(MinionClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Minion)
	{
		return;
	}
	double MaxLeash = 0.0;
	double MinLeash = 0.0;
	if (GetLeashDistances(MaxLeash, MinLeash))
	{
		Minion->SetLeashDistances(MaxLeash, MinLeash);
	}
	UGameplayStatics::FinishSpawningActor(Minion, SpawnTransform);
}

bool AMonsterCharacter::SetLeashDistances(const double MaxDistanceFromSpawnPoint, const double MinDistanceToResumeRoaming)
{
	FNumericProperty* MaxProperty = FindNumericProperty(GetClass(), MaxLeashPropertyName);
	FNumericProperty* MinProperty = FindNumericProperty(GetClass(), MinLeashPropertyName);
	if (!MaxProperty || !MinProperty || !MaxProperty->IsFloatingPoint() || !MinProperty->IsFloatingPoint())
	{
		return false;
	}
	MaxProperty->SetFloatingPointPropertyValue(MaxProperty->ContainerPtrToValuePtr<void>(this), MaxDistanceFromSpawnPoint);
	MinProperty->SetFloatingPointPropertyValue(MinProperty->ContainerPtrToValuePtr<void>(this), MinDistanceToResumeRoaming);
	return true;
}

bool AMonsterCharacter::GetLeashDistances(double& OutMaxDistanceFromSpawnPoint, double& OutMinDistanceToResumeRoaming) const
{
	const FNumericProperty* MaxProperty = FindNumericProperty(GetClass(), MaxLeashPropertyName);
	const FNumericProperty* MinProperty = FindNumericProperty(GetClass(), MinLeashPropertyName);
	if (!MaxProperty || !MinProperty || !MaxProperty->IsFloatingPoint() || !MinProperty->IsFloatingPoint())
	{
		return false;
	}
	OutMaxDistanceFromSpawnPoint = MaxProperty->GetFloatingPointPropertyValue(MaxProperty->ContainerPtrToValuePtr<void>(this));
	OutMinDistanceToResumeRoaming = MinProperty->GetFloatingPointPropertyValue(MinProperty->ContainerPtrToValuePtr<void>(this));
	return true;
}

// 공격 몽타주의 피해 구간 노티파이는 서버에서 능력 시스템 이벤트로 들어온다.
void AMonsterCharacter::BindAttackDamageWindowEvents()
{
	if (!DamageWindowOpenHandle.IsValid())
	{
		DamageWindowOpenHandle = AbilitySystemComponent->GenericGameplayEventCallbacks
			.FindOrAdd(PdGameplayTags::Notifier_Attack_DamageWindowOpen).AddUObject(this, &ThisClass::HandleAttackDamageWindowEvent);
	}
	if (!DamageWindowCloseHandle.IsValid())
	{
		DamageWindowCloseHandle = AbilitySystemComponent->GenericGameplayEventCallbacks
			.FindOrAdd(PdGameplayTags::Notifier_Attack_DamageWindowClose).AddUObject(this, &ThisClass::HandleAttackDamageWindowEvent);
	}
}

void AMonsterCharacter::UnbindAttackDamageWindowEvents()
{
	if (!AbilitySystemComponent)
	{
		return;
	}
	if (FGameplayEventMulticastDelegate* OpenEvent = AbilitySystemComponent->GenericGameplayEventCallbacks.Find(PdGameplayTags::Notifier_Attack_DamageWindowOpen))
	{
		OpenEvent->Remove(DamageWindowOpenHandle);
	}
	if (FGameplayEventMulticastDelegate* CloseEvent = AbilitySystemComponent->GenericGameplayEventCallbacks.Find(PdGameplayTags::Notifier_Attack_DamageWindowClose))
	{
		CloseEvent->Remove(DamageWindowCloseHandle);
	}
	DamageWindowOpenHandle.Reset();
	DamageWindowCloseHandle.Reset();
}

void AMonsterCharacter::HandleAttackDamageWindowEvent(const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return;
	}
	if (Payload->EventTag == PdGameplayTags::Notifier_Attack_DamageWindowOpen)
	{
		const FMonsterAttackPattern* Attack = FindAttackPattern(CurrentAttackMontage.Get());
		if (Attack && Attack->Telegraph.Shape == EAttackTelegraphShape::FrontCone)
		{
			ActivateTelegraphAreaDamage();
		}
		else
		{
			ActivateAttackSphere();
		}
	}
	else
	{
		DeactivateAttackSphere();
	}
}

// 살아남은 피격에만 반응하며, 서버 종류와 관계없이 공격 중단 결과를 동일하게 처리한다.
// 공격을 끊지 않는 몬스터는 공격 중 피격 모션을 건너뛰고, 재사용 대기 중에도 건너뛴다.
void AMonsterCharacter::TryPlayMonsterHitReactMontage(float DamageAmount, bool bAllowHitReact)
{
	if (!HasAuthority() || bDying || !bAllowHitReact || DamageAmount <= 0.0f || !AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(PdGameplayTags::State_Dead))
	{
		return;
	}
	const UBasicAttributeSet* Attributes = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	UAnimMontage* HitReactMontage = MonsterPresentationSettings.HitReactMontage;
	if (!Attributes || Attributes->GetHealth() <= DamageAmount || !HitReactMontage
		|| (bMonsterAttackActive && !bHitReactInterruptsAttack))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextHitReactTime)
	{
		return;
	}
	NextHitReactTime = Now + HitReactCooldown;
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
	else if (!MonsterAttacks.IsEmpty())
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

// 자신·다른 몬스터·같은 팀·사망한 캐릭터를 몬스터의 타격 대상에서 제외한다.
// 몬스터는 PlayerState가 없어 팀 판정으로는 서로 적이 되므로, 보스의 공격이 졸개를 죽이지 않게 따로 막는다.
bool AMonsterCharacter::IsValidMonsterDamageTarget(const AActor* OtherActor) const
{
	const ACharacterBase* Character = Cast<ACharacterBase>(OtherActor);
	if (!IsValid(Character) || Character == this || Character->IsA<AMonsterCharacter>() || !CanDamageCharacterByTeam(Character))
	{
		return false;
	}
	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	return ASC && !ASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead);
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

// 설정된 몬스터 피해 GE를 적용한다. 플레이어는 몬스터 접촉 피해로 피격 모션을 재생하지 않는다.
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
		DamageTag = UProjectTagDefinition::GetDefaultDefinition()->GetSetByCallerDamageMagnitudeTag();
	}
	if (!Spec.IsValid() || !DamageTag.IsValid())
	{
		return false;
	}
	if (TargetCharacter->IsA<APdPlayer>())
	{
		Spec.Data->AddDynamicAssetTag(PdGameplayTags::Effect_Damage_NoHitReaction);
	}
	Spec.Data->SetSetByCallerMagnitude(DamageTag, AttackDamageMagnitude);
	return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied();
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

// 판정을 여는 순간 이미 범위 안에 있던 대상도 포함해 타격한다. 닫는 시점은 호출한 쪽이 정한다.
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
}

// 공격 시간이 끝나거나 취소되면 충돌·장판 판정과 이번 공격의 타격 기록을 정리한다.
void AMonsterCharacter::DeactivateAttackSphere()
{
	bAttackWindowOpen = false;
	GetWorldTimerManager().ClearTimer(AttackSphereTimerHandle);
	GetWorldTimerManager().ClearTimer(TelegraphAreaTimerHandle);
	if (UPrimitiveComponent* Collision = AttackComponent.Get())
	{
		Collision->SetGenerateOverlapEvents(false);
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	AttackHitActorsThisSwing.Reset();
}

// 부채꼴 장판으로 알린 공격(브레스)은 충돌 구 대신 장판과 같은 범위로 판정한다.
// 피해 구간 동안 짧은 간격으로 다시 검사해, 구간 중에 장판으로 들어온 대상도 한 번 맞는다.
void AMonsterCharacter::ActivateTelegraphAreaDamage()
{
	if (!HasAuthority() || bDying || !bMonsterAttackActive)
	{
		return;
	}
	AttackHitActorsThisSwing.Reset();
	bAttackWindowOpen = true;
	ApplyTelegraphAreaDamage();
	GetWorldTimerManager().SetTimer(TelegraphAreaTimerHandle, this, &ThisClass::ApplyTelegraphAreaDamage, TelegraphAreaCheckInterval, true);
}

void AMonsterCharacter::ApplyTelegraphAreaDamage()
{
	const FMonsterAttackPattern* Attack = FindAttackPattern(CurrentAttackMontage.Get());
	UWorld* World = GetWorld();
	if (!bAttackWindowOpen || !Attack || !World)
	{
		return;
	}
	const FTransform Origin = AAttackTelegraphActor::GetMonsterOrigin(*this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin.GetLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Attack->Telegraph.Radius), FCollisionQueryParams(SCENE_QUERY_STAT(MonsterTelegraphArea), false, this));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacterBase* Character = Cast<ACharacterBase>(Overlap.GetActor());
		if (!Character || AttackHitActorsThisSwing.Contains(Character))
		{
			continue;
		}
		float CapsuleRadius = 0.0f;
		float CapsuleHalfHeight = 0.0f;
		Character->GetSimpleCollisionCylinder(CapsuleRadius, CapsuleHalfHeight);
		const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, CapsuleHalfHeight);
		if (Attack->Telegraph.IsInsideFrontCone(Origin, Feet, CapsuleRadius))
		{
			ApplyAttackDamageToCharacter(Character);
		}
	}
}

const FMonsterAttackPattern* AMonsterCharacter::FindAttackPattern(const UAnimMontage* AttackMontage) const
{
	return AttackMontage
		? MonsterAttacks.FindByPredicate([AttackMontage](const FMonsterAttackPattern& Pattern) { return Pattern.Montage.Get() == AttackMontage; })
		: nullptr;
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
void AMonsterCharacter::MulticastPlayMonsterAttackMontage_Implementation(UAnimMontage* AttackMontage, float PlayRate, AActor* Target)
{
	if (!HasAuthority())
	{
		PlayMonsterAttackMontageLocal(AttackMontage, PlayRate);
	}
	ShowAttackTelegraph(AttackMontage, PlayRate, Target);
}

// 공격 예고 장판은 화면을 그리는 인스턴스에서만 띄운다. 공격마다 새로 띄우고, 공격이 끊기면 함께 지운다.
void AMonsterCharacter::ShowAttackTelegraph(const UAnimMontage* AttackMontage, const float PlayRate, AActor* Target)
{
	HideAttackTelegraph();
	const FMonsterAttackPattern* Attack = FindAttackPattern(AttackMontage);
	if (Attack && !bDying && GetNetMode() != NM_DedicatedServer)
	{
		ActiveTelegraph = AAttackTelegraphActor::SpawnForAttack(*this, Attack->Telegraph, AttackTelegraphMaterial, *AttackMontage, PlayRate, Target);
	}
}

void AMonsterCharacter::HideAttackTelegraph()
{
	if (AAttackTelegraphActor* Telegraph = ActiveTelegraph.Get())
	{
		Telegraph->Destroy();
	}
	ActiveTelegraph.Reset();
}

// 윤곽선(스텐실 비트 1)과 피격 플래시(비트 2)를 메시의 CustomDepth 스텐실로 넘긴다. 그리는 쪽은 포스트 프로세스(Shaders/CombatHighlight.ush)다.
// 스텐실 값만 바꾸는 것은 싸지만 CustomDepth를 켜고 끄면 렌더 상태를 다시 만들므로, 쓰는 몬스터는 살아 있는 동안 켜 둔다.
void AMonsterCharacter::RefreshHighlightStencil()
{
	USkeletalMeshComponent* MonsterMesh = GetMesh();
	if (!MonsterMesh || GetNetMode() == NM_DedicatedServer || (!bDrawOutline && HitFlashDuration <= 0.0f))
	{
		return;
	}
	MonsterMesh->SetRenderCustomDepth(!bDying);
	MonsterMesh->SetCustomDepthStencilValue((bDrawOutline ? 1 : 0) | (bHitFlashActive ? 2 : 0));
}

// 스텐실을 그리는 포스트 프로세스를 월드에 한 번만 깐다. 이 화면에서만 쓰는 로컬 볼륨이라 복제하지 않는다.
void AMonsterCharacter::EnsureHighlightPostProcess() const
{
	static const FName HighlightTag(TEXT("CombatHighlight"));
	UWorld* World = GetWorld();
	if (!HighlightPostProcessMaterial || !World || GetNetMode() == NM_DedicatedServer || (!bDrawOutline && HitFlashDuration <= 0.0f))
	{
		return;
	}
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(HighlightTag))
		{
			return;
		}
	}
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>();
	Volume->bUnbound = true;
	Volume->Tags.Add(HighlightTag);
	Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, HighlightPostProcessMaterial));
}

// 서버가 확정한 피해 표현에 피격 플래시를 더한다. 화면을 그리는 인스턴스에서만 돈다.
void AMonsterCharacter::MulticastHandleDamageTaken_Implementation(const float DamageAmount, const bool bCriticalHit, const FVector_NetQuantize WorldLocation)
{
	Super::MulticastHandleDamageTaken_Implementation(DamageAmount, bCriticalHit, WorldLocation);
	if (DamageAmount <= 0.0f || HitFlashDuration <= 0.0f || bDying || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bHitFlashActive = true;
	RefreshHighlightStencil();
	GetWorldTimerManager().SetTimer(HitFlashTimerHandle, this, &ThisClass::EndHitFlash, HitFlashDuration, false);
}

void AMonsterCharacter::EndHitFlash()
{
	bHitFlashActive = false;
	RefreshHighlightStencil();
}

// 취소된 공격이 원격 화면이나 충돌 판정에 남지 않게 한다.
void AMonsterCharacter::MulticastStopMonsterAttack_Implementation()
{
	bMonsterAttackActive = false;
	DeactivateAttackSphere();
	HideAttackTelegraph();
	if (UAnimMontage* Montage = CurrentAttackMontage.Get())
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
