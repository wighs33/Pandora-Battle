#include "AI/Bot/PdBotController.h"

#include "Abilities/GameplayAbility.h"
#include "Character/PdPlayer.h"
#include "Common/PdGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "EngineUtils.h"
#include "Component/Match/MatchPlayerSetupComponent.h"
#include "Item/ItemInstance.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerState.h"
#include "Navigation/PathFollowingComponent.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSkillSource.h"
#include "Weapon/RangedWeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdBotController)

namespace
{
	/** 가장 가까운 적을 다시 고르는 간격. */
	constexpr float TargetSearchInterval = 0.5f;
	/** 무기로 칠 수 있는 대상과의 거리(두 몸 가장자리 사이). 근접 무기와 활·총. */
	constexpr float MeleeReach = 150.0f;
	constexpr float RangedReach = 1600.0f;
	/** 쫓아갈 때 무기 거리의 이만큼 안까지 다가간다(가장자리에서 서지 않게). */
	constexpr float ChaseReachRatio = 0.7f;
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
	constexpr EEnum_Direction LoadoutDirections[] = {EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right};
}

APdBotController::APdBotController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
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
	return CurrentTarget.Get();
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

	if (Now >= NextTargetSearchTime || !IsValidEnemy(*Self, CurrentTarget.Get()))
	{
		NextTargetSearchTime = Now + TargetSearchInterval;
		CurrentTarget = FindNearestEnemy(*Self);
	}
	AActor* Target = CurrentTarget.Get();

	switch (Phase)
	{
	case EPhase::Fight:
		if (!Target)
		{
			ReleaseHeldInputs();
			StopChasing();
			return;
		}
		TickFight(*Self, *Target, DeltaSeconds, Now, true);
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

AActor* APdBotController::FindNearestEnemy(const APdPlayer& Self) const
{
	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<APdPlayer> It(GetWorld()); It; ++It)
	{
		if (IsValidEnemy(Self, *It))
		{
			const float Distance = BotSkillRules::GetEdgeDistance(Self, **It);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = *It;
			}
		}
	}
	return Nearest;
}

// 경기 지급(인벤토리·판도라)이 끝나면 칸마다 그 칸 판도라가 쓸 수 있는 무기 가운데 아직 다른 칸에 끼우지 않은 것을 끼운다.
// 판도라가 없는 칸(판도라 정의를 다 불러오기 전에 경기가 시작된 경우)에는 아무 무기나 끼워 무기로라도 싸우게 한다.
bool APdBotController::TryPrepareLoadout()
{
	const AExperienceGameMode* GameMode = GetWorld()->GetAuthGameMode<AExperienceGameMode>();
	APdPlayerState* BotPlayerState = GetBotPlayerState();
	UInventoryComponent* Inventory = BotPlayerState ? BotPlayerState->GetInventoryComponent() : nullptr;
	const UPandoraComponent* Pandoras = BotPlayerState ? BotPlayerState->GetPandoraComponent() : nullptr;
	USelectingPandoraAndWeaponComponent* Selection = BotPlayerState ? BotPlayerState->GetSelectingPandoraAndWeaponComponent() : nullptr;
	if (!GameMode || !GameMode->GetPlayerSetupComponent()->IsPlayerReadyForGameplay(this) || !Inventory || !Pandoras || !Selection)
	{
		return false;
	}
	TSet<const UItemInstance*> UsedWeapons;
	for (const EEnum_Direction Direction : LoadoutDirections)
	{
		const UPandoraDefinition* Pandora = Pandoras->GetPandoraLoadoutDefinition(Direction);
		for (UItemInstance* Item : Inventory->GetAllItems().Items)
		{
			// 끼우는 함수가 무기가 아닌 아이템을 거절한다.
			if (Item && !UsedWeapons.Contains(Item) && (!Pandora || Pandora->IsCompatibleWithWeaponDefinition(Item->ItemDefinition))
				&& Inventory->AssignWeaponToLoadoutSlot(Direction, Item))
			{
				UsedWeapons.Add(Item);
				break;
			}
		}
	}
	Selection->RequestSelectPandoraAndWeapon(PandoraLoadout::GetLoadoutNumberFromDirection(EEnum_Direction::Left));
	LoadoutPreparedPawn = GetPawn();
	return true;
}

// 무기 거리 밖이거나 대상이 가려 있으면 쫓아가고, 안이면 멈춰 선 뒤 대상을 보고 친다.
void APdBotController::TickFight(APdPlayer& Self, AActor& Target, const float DeltaSeconds, const double Now, const bool bSkillsAllowed)
{
	if (bAttackHeld && Now >= AttackReleaseTime)
	{
		if (UCombatComponent* Combat = Self.GetCombatComponent())
		{
			Combat->StopPrimaryAttack();
		}
		bAttackHeld = false;
	}

	if (bSkillsAllowed && Now >= NextSkillTime && !bAttackHeld)
	{
		const int32 SkillSlot = FindCastableSkillSlot(Self, Target);
		if (SkillSlot != INDEX_NONE)
		{
			StopChasing();
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

	const float Reach = GetWeaponReach(Self);
	if (BotSkillRules::GetEdgeDistance(Self, Target) > Reach || !BotSkillRules::HasLineOfSight(Self, Target))
	{
		Chase(Self, Target);
		return;
	}
	StopChasing();
	FaceTarget(Self, Target, DeltaSeconds);
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

// 다른 칸 판도라의 스킬은 그 칸을 들기 전에는 쓸 수 있는지 알 수 없으므로, 판도라가 있는 다음 칸으로 차례로 바꾼다.
void APdBotController::SwitchToNextPandora(const double Now)
{
	APdPlayerState* BotPlayerState = GetBotPlayerState();
	const UPandoraComponent* Pandoras = BotPlayerState ? BotPlayerState->GetPandoraComponent() : nullptr;
	USelectingPandoraAndWeaponComponent* Selection = BotPlayerState ? BotPlayerState->GetSelectingPandoraAndWeaponComponent() : nullptr;
	if (!Pandoras || !Selection)
	{
		return;
	}
	const int32 Current = Selection->GetSelectedPandoraAndWeaponNumber();
	for (int32 Offset = 1; Offset < UE_ARRAY_COUNT(LoadoutDirections); ++Offset)
	{
		const int32 Next = (Current - 1 + Offset) % UE_ARRAY_COUNT(LoadoutDirections) + 1;
		if (Pandoras->GetPandoraLoadoutDefinition(PandoraLoadout::GetDirectionFromLoadoutNumber(Next)))
		{
			Selection->RequestSelectPandoraAndWeapon(Next);
			NextPandoraSwitchTime = Now + PandoraSwitchCooldownSeconds;
			return;
		}
	}
}

// 길을 찾아 쫓는다. 길 찾기 영역이 없는 곳에서는 대상 쪽으로 곧장 뛴다.
void APdBotController::Chase(APdPlayer& Self, const AActor& Target)
{
	if (bAttackHeld)
	{
		if (UCombatComponent* Combat = Self.GetCombatComponent())
		{
			Combat->StopPrimaryAttack();
		}
		bAttackHeld = false;
	}
	const float AcceptanceRadius = GetWeaponReach(Self) * ChaseReachRatio;
	if (GetMoveStatus() != EPathFollowingStatus::Moving || GetCurrentMoveRequestID() == FAIRequestID::InvalidRequest)
	{
		const EPathFollowingRequestResult::Type Result = MoveToActor(const_cast<AActor*>(&Target), AcceptanceRadius, true, true, true);
		bChasing = Result == EPathFollowingRequestResult::RequestSuccessful;
	}
	if (GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		FVector ToTarget = Target.GetActorLocation() - Self.GetActorLocation();
		ToTarget.Z = 0.0f;
		Self.AddMovementInput(ToTarget.GetSafeNormal(), 1.0f);
		SetControlRotation(ToTarget.Rotation());
	}
}

void APdBotController::StopChasing()
{
	if (bChasing || GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}
	bChasing = false;
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

float APdBotController::GetWeaponReach(const APdPlayer& Self) const
{
	return IsUsingRangedWeapon(Self) ? RangedReach : MeleeReach;
}

void APdBotController::ReleaseHeldInputs()
{
	APdPlayer* Self = GetBotPawn();
	if (bAttackHeld && Self)
	{
		if (UCombatComponent* Combat = Self->GetCombatComponent())
		{
			Combat->StopPrimaryAttack();
		}
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
