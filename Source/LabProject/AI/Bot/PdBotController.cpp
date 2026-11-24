#include "AI/Bot/PdBotController.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AI/Bot/BotGrowthComponent.h"
#include "Character/PdPlayer.h"
#include "Common/PdGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "EngineUtils.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Item/RewardChest.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Navigation/PathFollowingComponent.h"
#include "Pandora/PandoraSkillSource.h"
#include "Weapon/RangedWeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdBotController)

namespace
{
	/** 대상을 다시 고르고 얻은 것을 쓰는 간격. */
	constexpr float TargetSearchInterval = 0.5f;
	/** 이 거리 안에 보이는 적과는 크던 것을 멈추고 싸운다. 스킬을 쓸 수 없으면 코앞의 적과만 싸운다. */
	constexpr float EngageDistance = 1800.0f;
	constexpr float UnreadyEngageDistance = 300.0f;
	/** 대상에게 이만큼씩 가까워지지도, 피해를 주지도 못한 채 시간이 지나면 다른 곳으로 간다. */
	constexpr float ProgressStepDistance = 100.0f;
	constexpr double StalledSeconds = 8.0;
	/** 상자에 다가가는 거리와, 상자를 연 뒤 여는 동작을 하며 서 있는 시간. */
	constexpr float ChestAcceptanceRadius = 60.0f;
	constexpr float ChestOpenSeconds = 0.8f;
	/** 무기로 칠 수 있는 대상과의 거리(두 몸 가장자리 사이). 근접 무기와 활·총. */
	constexpr float MeleeReach = 80.0f;
	constexpr float RangedReach = 1600.0f;
	/** 쫓아갈 때 무기 거리의 이만큼 안까지 다가간다(가장자리에서 서지 않게). */
	constexpr float ChaseReachRatio = 0.7f;
	/** 무기 거리 안이라도 대상과의 높이 차이가 무기 거리에 이만큼 더한 것보다 크면 닿지 않는다. */
	constexpr float ReachHeightMargin = 100.0f;
	/** 무기 거리 안인데 가려 있으면 보일 때까지 이 거리까지 다가간다. */
	constexpr float HiddenTargetAcceptanceRadius = 60.0f;
	/** 쫓는데 이보다 느린 채로 시간이 지나면 막힌 것으로 보고, 잠깐 뛰어오르며 대상 쪽으로 곧장 뛴다. */
	constexpr float StuckSpeed = 60.0f;
	constexpr double StuckSeconds = 1.0;
	constexpr double DirectRunSeconds = 1.0;
	/** 공격 입력을 누르고 있는 시간. 활은 시위를 당기는 동안 누른다. */
	constexpr float MeleeAttackHoldSeconds = 0.15f;
	constexpr float RangedAttackHoldSeconds = 0.7f;
	/** 공격 입력을 다시 누르는 간격. 근접은 이어 누르면 콤보가 된다. */
	constexpr float MeleeAttackRepeatSeconds = 0.35f;
	constexpr float RangedAttackRepeatSeconds = 1.0f;
	/** 판도라를 너무 자주 바꾸지 않게, 바꾼 뒤 이만큼은 그 판도라를 든다. */
	constexpr float PandoraSwitchCooldownSeconds = 4.0f;
	/** 스킬 입력을 누른 뒤 떼기까지의 시간. 모으는 스킬은 스킬이 스스로 다 모은 뒤 쏜다. */
	constexpr float SkillInputHoldSeconds = 0.1f;

	FGameplayTag GetSkillInputTag(const int32 SkillSlot)
	{
		switch (SkillSlot)
		{
		case 0: return PdGameplayTags::Input_Ability_Skill1;
		case 1: return PdGameplayTags::Input_Ability_Skill2;
		case 2: return PdGameplayTags::Input_Ability_Skill3;
		default: return FGameplayTag();
		}
	}

	constexpr int32 SkillSlotCount = 3;
	constexpr int32 FirstLoadoutNumber = 1;
}

APdBotController::APdBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
	Growth = CreateDefaultSubobject<UBotGrowthComponent>(TEXT("Growth"));
}

void APdBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Phase = EPhase::Fight;
	ActiveFollowUp = ESkillBotFollowUp::None;
	CurrentTarget.Reset();
	ReleaseHeldInputs();
	NextSkillTime = GetWorld()->GetTimeSeconds() + BotSkillRules::RollGapBetweenSkills();
}

void APdBotController::OnUnPossess()
{
	ReleaseHeldInputs();
	StopChasing();
	Super::OnUnPossess();
}

AActor* APdBotController::GetAttackTarget_Implementation() const
{
	return Cast<ACharacterBase>(CurrentTarget.Get());
}

APdPlayer* APdBotController::GetBotPawn() const
{
	return Cast<APdPlayer>(GetPawn());
}

APdPlayerState* APdBotController::GetBotPlayerState() const
{
	return GetPlayerState<APdPlayerState>();
}

UPdAbilitySystemComponent* APdBotController::GetAbilitySystem() const
{
	const APdPlayerState* BotPlayerState = GetBotPlayerState();
	return BotPlayerState ? Cast<UPdAbilitySystemComponent>(BotPlayerState->GetAbilitySystemComponent()) : nullptr;
}

bool APdBotController::IsBotAlive() const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	return GetBotPawn() && ASC && !ASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead);
}

void APdBotController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APdPlayer* Self = GetBotPawn();
	if (!HasAuthority() || !Self)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (!IsBotAlive() || Self->IsStatusFrozen())
	{
		if (Phase != EPhase::Fight)
		{
			ReturnToFight(Now);
		}
		ReleaseHeldInputs();
		StopChasing();
		return;
	}
	if (LoadoutPreparedPawn.Get() != Self && !TryPrepareLoadout())
	{
		return;
	}

	// 스킬 입력은 누른 다음 틱에 뗀다(사람이 키를 눌렀다 떼는 것과 같은 경로).
	if (HeldSkillInput.IsValid() && Now - PhaseStartTime >= SkillInputHoldSeconds)
	{
		if (UPdAbilitySystemComponent* ASC = GetAbilitySystem())
		{
			ASC->HandleAbilityInputReleased(HeldSkillInput);
		}
		HeldSkillInput = FGameplayTag();
	}

	// 스킬을 쓰는 동안에는 대상이나 무기를 바꾸지 않는다. 대상이 사라졌으면 바로 다시 고른다.
	const bool bTargetLost = CurrentTarget.IsValid() && !IsValidTarget(*Self, CurrentTarget.Get());
	if ((Phase == EPhase::Fight && Now >= NextTargetSearchTime) || bTargetLost)
	{
		NextTargetSearchTime = Now + TargetSearchInterval;
		if (Phase == EPhase::Fight && !bAttackHeld)
		{
			Growth->ApplyGrowth();
		}
		AActor* NewTarget = ChooseTarget(*Self);
		if (NewTarget != CurrentTarget.Get())
		{
			StopChasing();
			CurrentTarget = NewTarget;
		}
	}
	ARewardChest* Chest = Cast<ARewardChest>(CurrentTarget.Get());
	AActor* Target = Chest ? nullptr : CurrentTarget.Get();

	switch (Phase)
	{
	case EPhase::Fight:
		if (Chest)
		{
			TickOpenChest(*Self, *Chest, DeltaSeconds, Now);
			return;
		}
		if (!Target)
		{
			ReleaseHeldInputs();
			StopChasing();
			return;
		}
		TickFight(*Self, *Target, DeltaSeconds, Now, !GetAbilitySystem()->HasMatchingGameplayTag(PdGameplayTags::State_Movement_RespawnFall));
		return;
	case EPhase::Preparing:
		if (!Target)
		{
			ReturnToFight(Now);
			return;
		}
		TickPreparing(*Self, *Target, DeltaSeconds, Now);
		return;
	case EPhase::Casting:
		TickCasting(*Self, Target, DeltaSeconds, Now);
		return;
	case EPhase::FollowUp:
		TickFollowUp(*Self, Target, DeltaSeconds, Now);
		return;
	}
}

// 사람과 봇 참가자 가운데 이 봇이 때릴 수 있는(같은 팀이 아닌) 살아 있는 캐릭터.
bool APdBotController::IsValidEnemy(const APdPlayer& Self, const AActor* Candidate) const
{
	const APdPlayer* Other = Cast<APdPlayer>(Candidate);
	const UAbilitySystemComponent* OtherASC = Other ? Other->GetAbilitySystemComponent() : nullptr;
	return Other && Other != &Self && OtherASC && !OtherASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead)
		&& Self.CanDamageCharacterByTeam(Other);
}

APdPlayer* APdBotController::FindNearestEnemy(const APdPlayer& Self, const bool bSkipPostponed) const
{
	APdPlayer* Nearest = nullptr;
	double NearestDistanceSquared = TNumericLimits<double>::Max();
	for (TActorIterator<APdPlayer> It(GetWorld()); It; ++It)
	{
		const double DistanceSquared = Self.GetSquaredDistanceTo(*It);
		if (DistanceSquared < NearestDistanceSquared && IsValidEnemy(Self, *It) && !(bSkipPostponed && Growth->IsPostponed(*It)))
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = *It;
		}
	}
	return Nearest;
}

// 가까이 보이는 적이 있으면 싸운다. 그렇지 않으면 크러 가고, 스킬을 쓸 수 있게 된 뒤에는 크러 갈 곳보다 가까운 적을 먼저 쫓는다.
AActor* APdBotController::ChooseTarget(const APdPlayer& Self) const
{
	APdPlayer* NearestEnemy = FindNearestEnemy(Self, false);
	const bool bCanUseSkills = Growth->HasUsableLoadout();
	if (NearestEnemy && Self.GetDistanceTo(NearestEnemy) <= (bCanUseSkills ? EngageDistance : UnreadyEngageDistance)
		&& BotSkillRules::HasLineOfSight(Self, *NearestEnemy))
	{
		return NearestEnemy;
	}
	APdPlayer* Enemy = FindNearestEnemy(Self, true);
	AActor* GrowthTarget = Growth->FindGrowthTarget(Self);
	if (!Enemy || !GrowthTarget)
	{
		return GrowthTarget ? GrowthTarget : NearestEnemy;
	}
	return bCanUseSkills && Self.GetSquaredDistanceTo(Enemy) < Self.GetSquaredDistanceTo(GrowthTarget) ? Enemy : GrowthTarget;
}

bool APdBotController::IsValidTarget(const APdPlayer& Self, AActor* Candidate) const
{
	return IsValidEnemy(Self, Candidate) || Growth->IsGrowthTarget(Self, Candidate);
}

// 경기 지급(인벤토리·판도라)이 끝나면 첫 칸을 든다. 무기와 판도라 레벨은 경기에서 얻는다.
bool APdBotController::TryPrepareLoadout()
{
	const AExperienceGameMode* GameMode = GetWorld()->GetAuthGameMode<AExperienceGameMode>();
	const APdPlayerState* BotPlayerState = GetBotPlayerState();
	if (!GameMode || !BotPlayerState || !GameMode->GetPlayerSetupComponent()->IsPlayerReadyForGameplay(this))
	{
		return false;
	}
	BotPlayerState->GetSelectingPandoraAndWeaponComponent()->RequestSelectPandoraAndWeapon(FirstLoadoutNumber);
	Growth->ApplyGrowth();
	LoadoutPreparedPawn = GetPawn();
	return true;
}

// 상자 앞까지 가서 멈춰 서서 연다. 여는 동작이 끝날 때까지 서 있다가 다음 대상을 고른다.
void APdBotController::TickOpenChest(APdPlayer& Self, ARewardChest& Chest, const float DeltaSeconds, const double Now)
{
	StopAttacking(Self);
	SetAiming(Self, false);
	if (!Self.CanInteractWithActor(&Chest))
	{
		Chase(Self, Chest, ChestAcceptanceRadius, Now);
		PostponeStalledTarget(Self, Chest, Now);
		return;
	}
	StopChasing();
	FaceTarget(Self, Chest, DeltaSeconds);
	if (Self.GetVelocity().Size2D() <= BotSkillRules::SettledSpeed)
	{
		Growth->OpenChest(Chest);
		CurrentTarget.Reset();
		NextTargetSearchTime = Now + ChestOpenSeconds;
	}
}

void APdBotController::PostponeStalledTarget(const APdPlayer& Self, const AActor& Target, const double Now)
{
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(&Target);
	const UAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetAbilitySystemComponent() : nullptr;
	const float Health = TargetASC ? TargetASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) : 0.0f;
	const float Distance = Self.GetDistanceTo(&Target);
	if (ProgressTarget.Get() != &Target || Distance <= ProgressDistance - ProgressStepDistance || Health < ProgressHealth)
	{
		ProgressTarget = &Target;
		ProgressDistance = Distance;
		ProgressHealth = Health;
		ProgressTime = Now;
	}
	else if (Now - ProgressTime > StalledSeconds)
	{
		Growth->PostponeTarget(Target);
		ProgressTarget.Reset();
	}
}

// 무기 거리 밖이거나 대상이 가려 있으면 쫓아가고, 안이면 멈춰 선 뒤 대상을 보고 친다.
void APdBotController::TickFight(APdPlayer& Self, AActor& Target, const float DeltaSeconds, const double Now, const bool bSkillsAllowed)
{
	if (Now >= AttackReleaseTime)
	{
		StopAttacking(Self);
	}

	if (bSkillsAllowed && Now >= NextSkillTime && !bAttackHeld)
	{
		const int32 SkillSlot = FindCastableSkillSlot(Self, Target);
		if (SkillSlot != INDEX_NONE)
		{
			StopChasing();
			SetAiming(Self, false);
			Phase = EPhase::Preparing;
			PendingSkillSlot = SkillSlot;
			PhaseStartTime = Now;
			return;
		}
		if (!HasAnyReadySkill() && Now >= NextPandoraSwitchTime)
		{
			SwitchToNextPandora(Now);
		}
		NextSkillTime = Now + BotSkillRules::RetryDelaySeconds;
	}

	PostponeStalledTarget(Self, Target, Now);

	// 쓸 수 있는 스킬이 있으면 멀리서 쏘기만 하지 않고 스킬이 닿는 곳까지 다가간다.
	float Reach = GetWeaponReach(Self);
	if (const float SkillRange = bSkillsAllowed ? GetReadySkillRange() : 0.0f; SkillRange > 0.0f)
	{
		Reach = FMath::Min(Reach, SkillRange);
	}
	const bool bInReach = BotSkillRules::GetEdgeDistance(Self, Target) <= Reach
		&& FMath::Abs(Target.GetActorLocation().Z - Self.GetActorLocation().Z) <= Reach + ReachHeightMargin;
	if (!bInReach || !BotSkillRules::HasLineOfSight(Self, Target))
	{
		StopAttacking(Self);
		SetAiming(Self, false);
		Chase(Self, Target, bInReach ? HiddenTargetAcceptanceRadius : Reach * ChaseReachRatio, Now);
		return;
	}
	StopChasing();
	FaceTarget(Self, Target, DeltaSeconds);
	SetAiming(Self, IsUsingRangedWeapon(Self));
	if (!bAttackHeld && Now >= NextAttackTime && Self.GetVelocity().Size2D() <= BotSkillRules::SettledSpeed)
	{
		if (UCombatComponent* Combat = Self.GetCombatComponent())
		{
			const bool bRanged = IsUsingRangedWeapon(Self);
			Combat->StartPrimaryAttack();
			bAttackHeld = true;
			AttackReleaseTime = Now + (bRanged ? RangedAttackHoldSeconds : MeleeAttackHoldSeconds);
			NextAttackTime = Now + (bRanged ? RangedAttackRepeatSeconds : MeleeAttackRepeatSeconds);
		}
	}
}

// 멈춰 서서 대상을 마주 본 뒤에 스킬 입력을 누른다. 너무 오래 걸리면 이번 시전은 그만둔다.
void APdBotController::TickPreparing(APdPlayer& Self, AActor& Target, const float DeltaSeconds, const double Now)
{
	FaceTarget(Self, Target, DeltaSeconds);
	if (Now - PhaseStartTime > BotSkillRules::MaximumPrepareSeconds)
	{
		ReturnToFight(Now);
		return;
	}
	const float FacingError = FMath::Abs(FRotator::NormalizeAxis(BotSkillRules::GetYawToTarget(Self, Target) - Self.GetActorRotation().Yaw));
	if (Self.GetVelocity().Size2D() > BotSkillRules::SettledSpeed || FacingError > BotSkillRules::CastFacingToleranceDegrees)
	{
		return;
	}
	UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const FGameplayAbilitySpec* Spec = FindSkillSpec(PendingSkillSlot);
	if (!ASC || !Spec || !IsSkillReady(*Spec))
	{
		ReturnToFight(Now);
		return;
	}
	const FGameplayAbilitySpecHandle Handle = Spec->Handle;
	CastingSkill = GetSkillDefinition(*Spec);
	HeldSkillInput = GetSkillInputTag(PendingSkillSlot);
	ASC->HandleAbilityInputPressed(HeldSkillInput);
	const FGameplayAbilitySpec* Activated = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Activated || !Activated->IsActive())
	{
		ASC->HandleAbilityInputReleased(HeldSkillInput);
		HeldSkillInput = FGameplayTag();
		ReturnToFight(Now);
		return;
	}
	CastingHandle = Handle;
	Phase = EPhase::Casting;
	PhaseStartTime = Now;
}

// 시전 동작이 끝날 때까지 대상을 바라본다(모으는 투사체가 대상 쪽으로 나가게).
void APdBotController::TickCasting(APdPlayer& Self, const AActor* Target, const float DeltaSeconds, const double Now)
{
	if (Target)
	{
		FaceTarget(Self, *Target, DeltaSeconds);
	}
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const UGameplayAbility* Animating = ASC ? ASC->GetAnimatingAbility() : nullptr;
	const bool bAnimating = Animating && Animating->GetCurrentAbilitySpecHandle() == CastingHandle;
	const double Elapsed = Now - PhaseStartTime;
	if ((Elapsed >= BotSkillRules::MinimumCastSeconds && !bAnimating) || Elapsed > BotSkillRules::MaximumCastSeconds)
	{
		EnterFollowUp(Now);
	}
}

// 뒤처리 시간은 시전을 시작한 때부터 센다(스킬 지속 시간과 같은 기준).
void APdBotController::EnterFollowUp(const double Now)
{
	const USkillDefinition* Skill = CastingSkill.Get();
	ActiveFollowUp = Skill ? Skill->BotFollowUp : ESkillBotFollowUp::None;
	FollowUpEndTime = PhaseStartTime + (Skill ? BotSkillRules::GetFollowUpSeconds(*Skill) : 0.0f);
	if (ActiveFollowUp == ESkillBotFollowUp::None || Now >= FollowUpEndTime)
	{
		ReturnToFight(Now);
		return;
	}
	Phase = EPhase::FollowUp;
	if (ActiveFollowUp == ESkillBotFollowUp::RoamAroundTarget)
	{
		Roam.Start(Now);
	}
}

void APdBotController::TickFollowUp(APdPlayer& Self, AActor* Target, const float DeltaSeconds, const double Now)
{
	if (Now >= FollowUpEndTime)
	{
		ReturnToFight(Now);
		return;
	}
	if (!Target)
	{
		StopChasing();
		return;
	}
	switch (ActiveFollowUp)
	{
	case ESkillBotFollowUp::RoamAroundTarget:
	{
		// 전속력으로 뛰며 달리는 쪽을 본다.
		StopChasing();
		const FVector Direction = Roam.Update(Self, *Target, Now);
		Self.AddMovementInput(Direction, 1.0f);
		SetControlRotation(Direction.Rotation());
		return;
	}
	case ESkillBotFollowUp::HoldPosition:
		StopChasing();
		FaceTarget(Self, *Target, DeltaSeconds);
		return;
	case ESkillBotFollowUp::WeaponAttacksOnly:
		TickFight(Self, *Target, DeltaSeconds, Now, false);
		return;
	default:
		ReturnToFight(Now);
		return;
	}
}

void APdBotController::ReturnToFight(const double Now)
{
	if (HeldSkillInput.IsValid())
	{
		if (UPdAbilitySystemComponent* ASC = GetAbilitySystem())
		{
			ASC->HandleAbilityInputReleased(HeldSkillInput);
		}
		HeldSkillInput = FGameplayTag();
	}
	Phase = EPhase::Fight;
	ActiveFollowUp = ESkillBotFollowUp::None;
	PendingSkillSlot = INDEX_NONE;
	CastingHandle = FGameplayAbilitySpecHandle();
	CastingSkill.Reset();
	NextSkillTime = Now + BotSkillRules::RollGapBetweenSkills();
}

// 지금 판도라의 스킬은 입력 태그(Input.Ability.Skill1~3)로 칸에 묶여 있다. 사람의 키 입력도 이 태그로 스킬을 찾는다.
const FGameplayAbilitySpec* APdBotController::FindSkillSpec(const int32 SkillSlot) const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const FGameplayTag InputTag = GetSkillInputTag(SkillSlot);
	if (!ASC || !InputTag.IsValid())
	{
		return nullptr;
	}
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return &Spec;
		}
	}
	return nullptr;
}

const USkillDefinition* APdBotController::GetSkillDefinition(const FGameplayAbilitySpec& Spec) const
{
	const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Spec.SourceObject.Get());
	return Source ? Source->GetSkillDataAsset() : nullptr;
}

bool APdBotController::IsSkillReady(const FGameplayAbilitySpec& Spec) const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	return ASC && Spec.Ability && !Spec.IsActive() && Spec.Ability->CanActivateAbility(Spec.Handle, ASC->AbilityActorInfo.Get());
}

// 재사용 대기가 끝났고, 대상이 스킬 거리 안에 보이는 지금 판도라의 스킬 가운데 하나를 무작위로 고른다.
int32 APdBotController::FindCastableSkillSlot(const APdPlayer& Self, const AActor& Target) const
{
	const float Distance = BotSkillRules::GetEdgeDistance(Self, Target);
	TArray<int32, TInlineAllocator<SkillSlotCount>> Castable;
	for (int32 SkillSlot = 0; SkillSlot < SkillSlotCount; ++SkillSlot)
	{
		const FGameplayAbilitySpec* Spec = FindSkillSpec(SkillSlot);
		const USkillDefinition* Skill = Spec ? GetSkillDefinition(*Spec) : nullptr;
		if (Skill && Distance <= Skill->BotCastRange && IsSkillReady(*Spec))
		{
			Castable.Add(SkillSlot);
		}
	}
	return !Castable.IsEmpty() && BotSkillRules::HasLineOfSight(Self, Target) ? Castable[FMath::RandHelper(Castable.Num())] : INDEX_NONE;
}

bool APdBotController::HasAnyReadySkill() const
{
	for (int32 SkillSlot = 0; SkillSlot < SkillSlotCount; ++SkillSlot)
	{
		if (const FGameplayAbilitySpec* Spec = FindSkillSpec(SkillSlot); Spec && IsSkillReady(*Spec))
		{
			return true;
		}
	}
	return false;
}

float APdBotController::GetReadySkillRange() const
{
	float Range = 0.0f;
	for (int32 SkillSlot = 0; SkillSlot < SkillSlotCount; ++SkillSlot)
	{
		const FGameplayAbilitySpec* Spec = FindSkillSpec(SkillSlot);
		const USkillDefinition* Skill = Spec ? GetSkillDefinition(*Spec) : nullptr;
		if (Skill && IsSkillReady(*Spec))
		{
			Range = FMath::Max(Range, Skill->BotCastRange);
		}
	}
	return Range;
}

// 다른 칸 판도라의 스킬이 재사용 대기 중인지는 그 칸을 들기 전에는 알 수 없으므로, 쓸 수 있는 다음 칸으로 차례로 바꾼다.
void APdBotController::SwitchToNextPandora(const double Now)
{
	const APdPlayerState* BotPlayerState = GetBotPlayerState();
	const int32 Next = Growth->FindNextUsableLoadout();
	if (BotPlayerState && Next != INDEX_NONE)
	{
		BotPlayerState->GetSelectingPandoraAndWeaponComponent()->RequestSelectPandoraAndWeapon(Next);
		NextPandoraSwitchTime = Now + PandoraSwitchCooldownSeconds;
	}
}

// 길을 찾아 쫓는다. 길이 없거나 막혀 섰으면 잠깐 대상 쪽으로 곧장 뛴다. 실패한 길 찾기는 속도를 0으로 만들므로 그동안은 다시 찾지 않는다.
void APdBotController::Chase(APdPlayer& Self, const AActor& Target, const float AcceptanceRadius, const double Now)
{
	const bool bMoving = Self.GetVelocity().Size2D() >= StuckSpeed;
	if (bMoving || !bChasing)
	{
		LastChaseMoveTime = Now;
	}
	if (Now >= DirectRunEndTime)
	{
		Self.StopJumping();
		bool bBlocked = Now - LastChaseMoveTime > StuckSeconds;
		if (!bBlocked && GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			bBlocked = MoveToActor(const_cast<AActor*>(&Target), AcceptanceRadius, true, true, true) == EPathFollowingRequestResult::Failed;
		}
		if (bBlocked)
		{
			StopChasing();
			DirectRunEndTime = Now + DirectRunSeconds;
			LastChaseMoveTime = Now;
			Self.Jump();
		}
	}
	bChasing = true;
	if (Now < DirectRunEndTime)
	{
		FVector ToTarget = Target.GetActorLocation() - Self.GetActorLocation();
		ToTarget.Z = 0.0f;
		Self.AddMovementInput(ToTarget.GetSafeNormal(), 1.0f);
		SetControlRotation(ToTarget.Rotation());
	}
}

void APdBotController::StopChasing()
{
	DirectRunEndTime = 0.0;
	if (bChasing || GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}
	bChasing = false;
}

void APdBotController::StopAttacking(APdPlayer& Self)
{
	if (bAttackHeld)
	{
		if (UCombatComponent* Combat = Self.GetCombatComponent())
		{
			Combat->StopPrimaryAttack();
		}
		bAttackHeld = false;
	}
}

void APdBotController::FaceTarget(APdPlayer& Self, const AActor& Target, const float DeltaSeconds)
{
	const float Yaw = FMath::FixedTurn(Self.GetActorRotation().Yaw, BotSkillRules::GetYawToTarget(Self, Target),
		BotSkillRules::CastTurnRateDegrees * DeltaSeconds);
	const FRotator Facing(0.0f, Yaw, 0.0f);
	Self.SetActorRotation(Facing);
	SetControlRotation(Facing);
}

bool APdBotController::IsUsingRangedWeapon(const APdPlayer& Self) const
{
	const UEquipmentComponent* Equipment = Self.GetEquipmentComponent();
	return Equipment && Cast<ARangedWeaponBase>(Equipment->GetCurrentWeaponActor());
}

void APdBotController::SetAiming(APdPlayer& Self, const bool bAiming)
{
	UCombatComponent* Combat = Self.GetCombatComponent();
	if (!Combat || Self.IsWeaponAimActive() == bAiming)
	{
		return;
	}
	if (bAiming)
	{
		Combat->StartAim();
	}
	else
	{
		Combat->StopAim();
	}
}

float APdBotController::GetWeaponReach(const APdPlayer& Self) const
{
	return IsUsingRangedWeapon(Self) ? RangedReach : MeleeReach;
}

void APdBotController::ReleaseHeldInputs()
{
	if (APdPlayer* Self = GetBotPawn())
	{
		StopAttacking(*Self);
		SetAiming(*Self, false);
	}
	bAttackHeld = false;
	if (HeldSkillInput.IsValid())
	{
		if (UPdAbilitySystemComponent* ASC = GetAbilitySystem())
		{
			ASC->HandleAbilityInputReleased(HeldSkillInput);
		}
		HeldSkillInput = FGameplayTag();
	}
}
