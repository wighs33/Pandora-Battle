#include "Component/Character/TrainingBotPandoraComponent.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Character/EnemyBase.h"
#include "Common/PdGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/EnemyTrainingBotComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Interface/TargetingInterface.h"
#include "Pandora/PandoraSkillSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TrainingBotPandoraComponent)

namespace
{
	/** 스킬을 쓰는 동안 행동 트리를 멈출 때 남기는 이유. */
	const TCHAR* const PauseReason = TEXT("TrainingBotPandoraSkill");

	/** 기본 공격만 하는 동안 공격을 거는 간격. 공격 중이면 공격 함수가 콤보로 잇는다. */
	constexpr float WeaponAttackRequestInterval = 0.3f;

	/** 판도라가 쓸 수 있는 무기 가운데 목록에서 먼저 나오는 것. */
	const UItemDefinition* FindWeaponFor(const UPandoraDefinition& Pandora, const TArray<TSoftObjectPtr<UItemDefinition>>& Weapons)
	{
		for (const TSoftObjectPtr<UItemDefinition>& Weapon : Weapons)
		{
			if (const UItemDefinition* Loaded = Weapon.Get(); Loaded && Pandora.IsCompatibleWithWeaponDefinition(Loaded))
			{
				return Loaded;
			}
		}
		return nullptr;
	}
}

UTrainingBotPandoraComponent::UTrainingBotPandoraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTrainingBotPandoraComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bUsePandorasByDefault)
	{
		SetPandorasEnabled(true);
	}
}

void UTrainingBotPandoraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++SelectionRequestId;
	PandoraPreloadHandle.Reset();
	WeaponLease.Reset();
	Super::EndPlay(EndPlayReason);
}

void UTrainingBotPandoraComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AEnemyBase* Enemy = GetEnemy();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}
	if (bSelectionPending)
	{
		if (Enemy->IsEnemyRuntimeInitialized())
		{
			bSelectionPending = false;
			BeginPandoraSelection();
		}
		return;
	}
	TickSkills(*Enemy, DeltaTime);
}

void UTrainingBotPandoraComponent::SetPandorasEnabled(const bool bEnabled)
{
	AEnemyBase* Enemy = GetEnemy();
	if (!Enemy || !Enemy->HasAuthority() || bPandorasEnabled == bEnabled)
	{
		return;
	}
	bPandorasEnabled = bEnabled;
	if (bEnabled)
	{
		// 시작 무기를 알아야 끌 때 돌아갈 수 있으므로, 봇 정의가 적용된 뒤에 고른다.
		bSelectionPending = true;
		SetComponentTickEnabled(true);
		return;
	}

	++SelectionRequestId;
	bSelectionPending = false;
	PandoraPreloadHandle.Reset();
	WeaponLease.Reset();
	if (Phase != EPhase::Idle)
	{
		CancelCastingSkill();
		ReturnToBehaviorTree(GetWorld()->GetTimeSeconds());
	}
	ClearPandoraSkills();
	SetComponentTickEnabled(false);
	if (!bSwitchedWeapon)
	{
		return;
	}
	bSwitchedWeapon = false;
	if (WeaponBeforePandoras)
	{
		Enemy->RequestTrainingBotWeaponChange(WeaponBeforePandoras);
	}
	else
	{
		Enemy->RequestTrainingBotUnarmed();
	}
}

const UPandoraDefinition* UTrainingBotPandoraComponent::GetCurrentPandora() const
{
	return Pandoras.IsValidIndex(CurrentPandoraIndex) ? Pandoras[CurrentPandoraIndex].Pandora.Get() : nullptr;
}

TArray<const UPandoraDefinition*> UTrainingBotPandoraComponent::GetPandoras() const
{
	TArray<const UPandoraDefinition*> Result;
	for (const FTrainingBotPandora& Entry : Pandoras)
	{
		Result.Add(Entry.Pandora);
	}
	return Result;
}

const UItemDefinition* UTrainingBotPandoraComponent::GetWeaponBeforePandoras() const
{
	if (bSwitchedWeapon)
	{
		return WeaponBeforePandoras;
	}
	const AEnemyBase* Enemy = GetEnemy();
	const UEnemyTrainingBotComponent* TrainingBot = Enemy ? Enemy->GetEnemyTrainingBotComponent() : nullptr;
	return TrainingBot ? TrainingBot->GetRequestedWeaponDefinition() : nullptr;
}

AEnemyBase* UTrainingBotPandoraComponent::GetEnemy() const
{
	return Cast<AEnemyBase>(GetOwner());
}

UPdAbilitySystemComponent* UTrainingBotPandoraComponent::GetAbilitySystem() const
{
	const AEnemyBase* Enemy = GetEnemy();
	return Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
}

UBrainComponent* UTrainingBotPandoraComponent::GetBrain() const
{
	const AEnemyBase* Enemy = GetEnemy();
	const AAIController* AIController = Enemy ? Cast<AAIController>(Enemy->GetController()) : nullptr;
	return AIController ? AIController->GetBrainComponent() : nullptr;
}

// 판도라 정의(스킬을 함께 담는다)와 짝 무기 후보를 모두 불러온 뒤에 고른다.
void UTrainingBotPandoraComponent::BeginPandoraSelection()
{
	UContentDataSubsystem* ContentData = GetWorld() ? UGameInstance::GetSubsystem<UContentDataSubsystem>(GetWorld()->GetGameInstance()) : nullptr;
	if (!ContentData)
	{
		return;
	}
	const int32 RequestId = ++SelectionRequestId;
	TArray<FSoftObjectPath> WeaponPaths;
	for (const TSoftObjectPtr<UItemDefinition>& Weapon : PandoraWeapons)
	{
		WeaponPaths.Add(Weapon.ToSoftObjectPath());
	}
	PendingContentLoads = 2;
	PandoraPreloadHandle = ContentData->PreloadPandoraDataAssetsAsync(
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePandoraContentLoaded, RequestId));
	WeaponLease = UContentDataSubsystem::AcquireContent(WeaponPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandlePandoraContentLoaded, RequestId));
}

void UTrainingBotPandoraComponent::HandlePandoraContentLoaded(const int32 RequestId)
{
	if (RequestId != SelectionRequestId || !bPandorasEnabled || --PendingContentLoads > 0)
	{
		return;
	}
	PickAndGrantPandoras();
}

// 플레이어가 판도라 레벨대로 받는 것과 같은 스킬 레벨로, 판도라마다 모든 스킬 칸을 부여한다.
// 플레이어 판도라 스킬과 달리 '선택된 판도라' 태그로 막지 않는다. 지금 판도라만 쓰는 규칙은 이 컴포넌트가 지킨다.
void UTrainingBotPandoraComponent::PickAndGrantPandoras()
{
	UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	UContentDataSubsystem* ContentData = UGameInstance::GetSubsystem<UContentDataSubsystem>(GetWorld()->GetGameInstance());
	if (!ASC || !ContentData)
	{
		return;
	}
	TMap<FName, TObjectPtr<UPandoraDefinition>> LoadedPandoras;
	ContentData->GetLoadedPandoraDefinitionsByName(LoadedPandoras);
	TArray<const UPandoraDefinition*> Candidates;
	for (const TPair<FName, TObjectPtr<UPandoraDefinition>>& Entry : LoadedPandoras)
	{
		if (Entry.Value && BotSkillRules::IsPandoraUsableByBots(*Entry.Value) && FindWeaponFor(*Entry.Value, PandoraWeapons))
		{
			Candidates.Add(Entry.Value);
		}
	}

	ClearPandoraSkills();
	for (const UPandoraDefinition* Pandora : BotSkillRules::PickRandomPandoras(Candidates, PandoraCount))
	{
		FTrainingBotPandora& Entry = Pandoras.AddDefaulted_GetRef();
		Entry.Pandora = Pandora;
		Entry.Weapon = FindWeaponFor(*Pandora, PandoraWeapons);
		for (int32 SkillIndex = 0; SkillIndex < Pandora->GetSkillCount(); ++SkillIndex)
		{
			const USkillDefinition* Skill = Pandora->GetSkillDefinition(SkillIndex);
			UPandoraSkillSource* Source = NewObject<UPandoraSkillSource>(this);
			Source->Initialize(Pandora, SkillIndex);
			SkillSources.Add(Source);
			FGameplayAbilitySpec Spec(USkillAbility::StaticClass(), UPandoraDefinition::GetRequiredLevelForSkillSlot(SkillIndex), INDEX_NONE, Source);
			Spec.GetDynamicSpecSourceTags().AppendTags(Skill->Activation.Tags);
			if (Skill->SkillType == ESkillType::Press)
			{
				Spec.GetDynamicSpecSourceTags().AddTag(PdGameplayTags::Skill_Type_Press);
			}
			Entry.SkillHandles.Add(ASC->GiveAbility(Spec));
		}
	}
	if (!Pandoras.IsEmpty())
	{
		SelectPandora(0);
	}
}

void UTrainingBotPandoraComponent::ClearPandoraSkills()
{
	if (UPdAbilitySystemComponent* ASC = GetAbilitySystem())
	{
		for (const FTrainingBotPandora& Entry : Pandoras)
		{
			for (const FGameplayAbilitySpecHandle& Handle : Entry.SkillHandles)
			{
				ASC->ClearAbility(Handle);
			}
		}
	}
	Pandoras.Reset();
	SkillSources.Reset();
	CurrentPandoraIndex = INDEX_NONE;
}

void UTrainingBotPandoraComponent::SelectPandora(const int32 PandoraIndex)
{
	AEnemyBase* Enemy = GetEnemy();
	if (!Enemy || !Pandoras.IsValidIndex(PandoraIndex))
	{
		return;
	}
	if (!bSwitchedWeapon)
	{
		const UEnemyTrainingBotComponent* TrainingBot = Enemy->GetEnemyTrainingBotComponent();
		WeaponBeforePandoras = TrainingBot ? TrainingBot->GetRequestedWeaponDefinition() : Enemy->GetCurrentOrStartingEnemyWeaponDefinition();
		bSwitchedWeapon = true;
	}
	CurrentPandoraIndex = PandoraIndex;
	Enemy->RequestTrainingBotWeaponChange(Pandoras[PandoraIndex].Weapon);
}

// 지금 판도라 다음부터 차례로 보아, 재사용 대기가 끝난 스킬이 있는 판도라를 찾는다.
int32 UTrainingBotPandoraComponent::FindNextPandoraWithReadySkill() const
{
	for (int32 Offset = 1; Offset < Pandoras.Num(); ++Offset)
	{
		const int32 Index = (CurrentPandoraIndex + Offset) % Pandoras.Num();
		if (Pandoras[Index].SkillHandles.ContainsByPredicate([this](const FGameplayAbilitySpecHandle Handle) { return CanActivateSkill(Handle); }))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UTrainingBotPandoraComponent::TickSkills(AEnemyBase& Enemy, const float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!CanUseSkillsNow(Enemy))
	{
		if (Phase != EPhase::Idle)
		{
			// 공격을 끄면(설정·테스트) 시전 중인 스킬도 거둔다. 남겨 두면 끈 뒤에도 스킬이 끝까지 나간다.
			// 피격·빙결은 전투 규칙이 스킬을 끊고, 뒤처리 단계의 스킬(장판·블랙홀)은 이미 나간 것이라 그대로 둔다.
			if (Phase == EPhase::Casting && !Enemy.IsAttackEnabled())
			{
				CancelCastingSkill();
			}
			ReturnToBehaviorTree(Now);
		}
		return;
	}
	AActor* Target = ITargetingInterface::Execute_GetAttackTarget(&Enemy);
	if (!Enemy.IsActorValidAttackTarget(Target))
	{
		Target = nullptr;
	}

	switch (Phase)
	{
	case EPhase::Idle:
	{
		const UEnemyTrainingBotComponent* TrainingBot = Enemy.GetEnemyTrainingBotComponent();
		if (!Target || Now < NextSkillTime || !Pandoras.IsValidIndex(CurrentPandoraIndex)
			|| (TrainingBot && TrainingBot->IsWeaponChangeInProgress()))
		{
			return;
		}
		RefillManaForCurrentPandora();
		const int32 SkillSlot = FindCastableSkill(Enemy, *Target);
		if (SkillSlot != INDEX_NONE)
		{
			PauseBrain(Enemy);
			BeginPreparing(SkillSlot, Now);
			return;
		}
		// 지금 판도라의 스킬이 모두 재사용 대기 중이면, 쓸 스킬이 남은 판도라로 바꿔 든다.
		const bool bCurrentHasReadySkill = Pandoras[CurrentPandoraIndex].SkillHandles.ContainsByPredicate(
			[this](const FGameplayAbilitySpecHandle Handle) { return CanActivateSkill(Handle); });
		if (const int32 NextPandora = bCurrentHasReadySkill ? INDEX_NONE : FindNextPandoraWithReadySkill(); NextPandora != INDEX_NONE)
		{
			SelectPandora(NextPandora);
		}
		NextSkillTime = Now + BotSkillRules::RetryDelaySeconds;
		return;
	}
	case EPhase::Preparing:
		if (!Target)
		{
			ReturnToBehaviorTree(Now);
			return;
		}
		TickPreparing(Enemy, *Target, DeltaTime, Now);
		return;
	case EPhase::Casting:
		TickCasting(Enemy, Target, DeltaTime, Now);
		return;
	case EPhase::FollowUp:
		TickFollowUp(Enemy, Target, DeltaTime, Now);
		return;
	}
}

// 행동 트리가 다른 이유로 꺼져 있으면(테스트·연출) 그 결정을 따른다. 이 컴포넌트가 멈춘 경우만 스킬을 이어 간다.
bool UTrainingBotPandoraComponent::CanUseSkillsNow(const AEnemyBase& Enemy) const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const UBrainComponent* Brain = GetBrain();
	const UEnemyTrainingBotComponent* TrainingBot = Enemy.GetEnemyTrainingBotComponent();
	return bPandorasEnabled && ASC && Brain && Enemy.IsAttackEnabled() && !Enemy.IsStatusFrozen()
		&& !ASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead)
		&& !(TrainingBot && TrainingBot->IsHitStunned())
		&& (Brain->IsRunning() || bBrainPausedBySkill);
}

// 재사용 대기가 끝났고, 대상이 스킬 거리 안에 보이는 지금 판도라의 스킬 가운데 하나를 무작위로 고른다.
int32 UTrainingBotPandoraComponent::FindCastableSkill(const AEnemyBase& Enemy, const AActor& Target) const
{
	const FTrainingBotPandora& Current = Pandoras[CurrentPandoraIndex];
	const float Distance = BotSkillRules::GetEdgeDistance(Enemy, Target);
	TArray<int32, TInlineAllocator<3>> Castable;
	for (int32 SkillSlot = 0; SkillSlot < Current.SkillHandles.Num(); ++SkillSlot)
	{
		const USkillDefinition* Skill = Current.Pandora->GetSkillDefinition(SkillSlot);
		if (!Skill || Distance > Skill->BotCastRange)
		{
			continue;
		}
		if (CanActivateSkill(Current.SkillHandles[SkillSlot]))
		{
			Castable.Add(SkillSlot);
		}
	}
	return !Castable.IsEmpty() && BotSkillRules::HasLineOfSight(Enemy, Target) ? Castable[FMath::RandHelper(Castable.Num())] : INDEX_NONE;
}

bool UTrainingBotPandoraComponent::CanActivateSkill(const FGameplayAbilitySpecHandle Handle) const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	return Spec && Spec->Ability && !Spec->IsActive() && Spec->Ability->CanActivateAbility(Handle, ASC->AbilityActorInfo.Get());
}

// 투사체를 모으는 동안처럼 몽타주가 멈춰 있어도 시전 중으로 본다.
bool UTrainingBotPandoraComponent::IsCastAnimating() const
{
	const UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const UGameplayAbility* Animating = ASC ? ASC->GetAnimatingAbility() : nullptr;
	return Animating && Animating->GetCurrentAbilitySpecHandle() == CastingHandle;
}

// 지금 판도라의 스킬 가운데 가장 비싼 것도 쓸 수 있게 채운다. 마나가 넉넉하면 건드리지 않는다.
void UTrainingBotPandoraComponent::RefillManaForCurrentPandora() const
{
	UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	const UPandoraDefinition* Pandora = GetCurrentPandora();
	if (!ASC || !Pandora)
	{
		return;
	}
	double HighestCost = 0.0;
	for (int32 SkillSlot = 0; SkillSlot < Pandora->GetSkillCount(); ++SkillSlot)
	{
		if (const USkillDefinition* Skill = Pandora->GetSkillDefinition(SkillSlot))
		{
			HighestCost = FMath::Max(HighestCost, Skill->ManaCost);
		}
	}
	if (ASC->GetNumericAttribute(UBasicAttributeSet::GetManaAttribute()) < HighestCost)
	{
		ASC->SetNumericAttributeBase(UBasicAttributeSet::GetManaAttribute(), ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxManaAttribute()));
	}
}

void UTrainingBotPandoraComponent::BeginPreparing(const int32 SkillSlot, const double Now)
{
	Phase = EPhase::Preparing;
	PendingSkillSlot = SkillSlot;
	PhaseStartTime = Now;
}

// 멈춰 서서 대상을 마주 본 뒤에 쓴다. 너무 오래 걸리면 이번 시전은 그만둔다.
void UTrainingBotPandoraComponent::TickPreparing(AEnemyBase& Enemy, const AActor& Target, const float DeltaTime, const double Now)
{
	FaceTarget(Enemy, Target, DeltaTime);
	if (Now - PhaseStartTime > BotSkillRules::MaximumPrepareSeconds)
	{
		ReturnToBehaviorTree(Now);
		return;
	}
	const float FacingError = FMath::Abs(FRotator::NormalizeAxis(BotSkillRules::GetYawToTarget(Enemy, Target) - Enemy.GetActorRotation().Yaw));
	if (Enemy.GetVelocity().Size2D() > BotSkillRules::SettledSpeed || FacingError > BotSkillRules::CastFacingToleranceDegrees)
	{
		return;
	}
	const FGameplayAbilitySpecHandle Handle = Pandoras[CurrentPandoraIndex].SkillHandles[PendingSkillSlot];
	UPdAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC || !CanActivateSkill(Handle) || !ASC->TryActivateAbility(Handle))
	{
		ReturnToBehaviorTree(Now);
		return;
	}
	CastingHandle = Handle;
	Phase = EPhase::Casting;
	PhaseStartTime = Now;
}

// 시전 동작이 끝날 때까지 대상을 바라본다(모으는 투사체가 대상 쪽으로 나가게).
void UTrainingBotPandoraComponent::TickCasting(AEnemyBase& Enemy, const AActor* Target, const float DeltaTime, const double Now)
{
	if (Target)
	{
		FaceTarget(Enemy, *Target, DeltaTime);
	}
	const double Elapsed = Now - PhaseStartTime;
	if ((Elapsed >= BotSkillRules::MinimumCastSeconds && !IsCastAnimating()) || Elapsed > BotSkillRules::MaximumCastSeconds)
	{
		EnterFollowUp(Now);
	}
}

// 뒤처리 시간은 시전을 시작한 때부터 센다(스킬 지속 시간과 같은 기준).
void UTrainingBotPandoraComponent::EnterFollowUp(const double Now)
{
	const USkillDefinition* Skill = Pandoras[CurrentPandoraIndex].Pandora->GetSkillDefinition(PendingSkillSlot);
	ActiveFollowUp = Skill ? Skill->BotFollowUp : ESkillBotFollowUp::None;
	FollowUpEndTime = PhaseStartTime + (Skill ? BotSkillRules::GetFollowUpSeconds(*Skill) : 0.0f);
	if (ActiveFollowUp == ESkillBotFollowUp::None || Now >= FollowUpEndTime)
	{
		ReturnToBehaviorTree(Now);
		return;
	}
	Phase = EPhase::FollowUp;
	NextWeaponAttackTime = Now;
	if (ActiveFollowUp == ESkillBotFollowUp::RoamAroundTarget)
	{
		Roam.Start(Now);
	}
}

void UTrainingBotPandoraComponent::TickFollowUp(AEnemyBase& Enemy, const AActor* Target, const float DeltaTime, const double Now)
{
	if (Now >= FollowUpEndTime)
	{
		ReturnToBehaviorTree(Now);
		return;
	}
	if (!Target)
	{
		return;
	}
	if (ActiveFollowUp == ESkillBotFollowUp::RoamAroundTarget)
	{
		// 전속력으로 뛰며 달리는 쪽을 본다.
		const FVector Direction = Roam.Update(Enemy, *Target, Now);
		Enemy.AddMovementInput(Direction, 1.0f);
		if (AController* Controller = Enemy.GetController())
		{
			Controller->SetControlRotation(Direction.Rotation());
		}
	}
	else if (ActiveFollowUp == ESkillBotFollowUp::HoldPosition)
	{
		FaceTarget(Enemy, *Target, DeltaTime);
	}
	else if (ActiveFollowUp == ESkillBotFollowUp::WeaponAttacksOnly)
	{
		TickWeaponAttacksOnly(Enemy, *Target, DeltaTime, Now);
	}
}

// 행동 트리의 길 찾기에 기대지 않고 대상에게 곧장 다가가, 무기 사거리 안에서 멈춰 선 뒤 공격한다.
void UTrainingBotPandoraComponent::TickWeaponAttacksOnly(AEnemyBase& Enemy, const AActor& Target, const float DeltaTime, const double Now)
{
	FaceTarget(Enemy, Target, DeltaTime);
	if (Enemy.GetAttackDistanceToActor(&Target) > Enemy.GetAttackStartDistance())
	{
		FVector ToTarget = Target.GetActorLocation() - Enemy.GetActorLocation();
		ToTarget.Z = 0.0f;
		Enemy.AddMovementInput(ToTarget.GetSafeNormal(), 1.0f);
		return;
	}
	if (Now >= NextWeaponAttackTime && Enemy.GetVelocity().Size2D() <= BotSkillRules::SettledSpeed)
	{
		NextWeaponAttackTime = Now + WeaponAttackRequestInterval;
		Enemy.Attack();
	}
}

void UTrainingBotPandoraComponent::ReturnToBehaviorTree(const double Now)
{
	ResumeBrain();
	Phase = EPhase::Idle;
	ActiveFollowUp = ESkillBotFollowUp::None;
	CastingHandle = FGameplayAbilitySpecHandle();
	PendingSkillSlot = INDEX_NONE;
	NextSkillTime = Now + BotSkillRules::RollGapBetweenSkills();
}

void UTrainingBotPandoraComponent::CancelCastingSkill()
{
	if (UPdAbilitySystemComponent* ASC = GetAbilitySystem(); ASC && CastingHandle.IsValid())
	{
		ASC->CancelAbilityHandle(CastingHandle);
	}
}

void UTrainingBotPandoraComponent::PauseBrain(AEnemyBase& Enemy)
{
	if (UBrainComponent* Brain = GetBrain(); Brain && !bBrainPausedBySkill)
	{
		Brain->PauseLogic(PauseReason);
		bBrainPausedBySkill = true;
	}
	if (AAIController* AIController = Cast<AAIController>(Enemy.GetController()))
	{
		AIController->StopMovement();
	}
}

void UTrainingBotPandoraComponent::ResumeBrain()
{
	if (!bBrainPausedBySkill)
	{
		return;
	}
	bBrainPausedBySkill = false;
	if (UBrainComponent* Brain = GetBrain())
	{
		Brain->ResumeLogic(PauseReason);
	}
}

void UTrainingBotPandoraComponent::FaceTarget(AEnemyBase& Enemy, const AActor& Target, const float DeltaTime) const
{
	const float Yaw = FMath::FixedTurn(Enemy.GetActorRotation().Yaw, BotSkillRules::GetYawToTarget(Enemy, Target),
		BotSkillRules::CastTurnRateDegrees * DeltaTime);
	const FRotator Facing(0.0f, Yaw, 0.0f);
	Enemy.SetActorRotation(Facing);
	if (AController* Controller = Enemy.GetController())
	{
		Controller->SetControlRotation(Facing);
	}
}
