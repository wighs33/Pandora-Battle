#include "Component/AbilitySystem/PandoraTreeComponent.h"

#include "Algo/Compare.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraTreeComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraTreeComponent, Log, All);

UPandoraTreeComponent::UPandoraTreeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPandoraTreeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPandoraComponent* PandoraComponent = GetOwnerPandoraComponent())
	{
		PandoraComponent->OnPandoraInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraInventoryChanged);
	}
}

void UPandoraTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPandoraComponent* PandoraComponent = GetOwnerPandoraComponent())
	{
		PandoraComponent->OnPandoraInventoryChanged.RemoveDynamic(this, &ThisClass::HandlePandoraInventoryChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UPandoraTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, GrantedPandoras, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, PointsAvailable, Params);
}

// 초기 지급은 무료 투자로 기록하고, 완성된 트리 상태를 한 번만 알린다.
void UPandoraTreeComponent::InitializeFromDefaultProvision(const TArray<FGrantedPandora>& InGrantedPandoras, const int32 InPointsAvailable)
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras)
	{
		return;
	}

	InitialGrantedPandoras.Reset();
	for (const FGrantedPandora& Entry : InGrantedPandoras)
	{
		if (IsValid(Entry.Pandora) && !InitialGrantedPandoras.Contains(Entry))
		{
			InitialGrantedPandoras.Emplace(Entry.Pandora, ClampPandoraLevel(Entry.Pandora, Entry.Level));
		}
	}
	InitialPointsAvailable = FMath::Max(InPointsAvailable, 0);
	RestoreInitialPandoras(InitialPointsAvailable);
}

// 처음 투자할 때만 소유 목록에 추가한다. 비용과 선행 조건은 조회 함수와 같은 기준을 쓴다.
bool UPandoraTreeComponent::GrantPandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost)
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras || HasGrantedPandora(Pandora))
	{
		return false;
	}

	const int32 NewLevel = ClampPandoraLevel(Pandora, StartingLevel);
	int32 Cost = 0;
	return TryGetInvestmentCost(Pandora, 0, NewLevel, bIgnorePointCost, Cost)
		&& ApplyPandoraInvestment(Pandora, INDEX_NONE, NewLevel, Cost);
}

// 기존 투자 항목을 올리고 현재 장착 중인 판도라의 사용 가능한 스킬을 갱신한다.
bool UPandoraTreeComponent::LevelUpGrantedPandora(UPandoraDefinition* Pandora)
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras)
	{
		return false;
	}

	const int32 EntryIndex = FindGrantedPandoraIndex(Pandora);
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 CurrentLevel = ClampPandoraLevel(Pandora, GrantedPandoras[EntryIndex].Level);
	int32 Cost = 0;
	return TryGetInvestmentCost(Pandora, CurrentLevel, CurrentLevel + 1, false, Cost)
		&& ApplyPandoraInvestment(Pandora, EntryIndex, CurrentLevel + 1, Cost);
}

void UPandoraTreeComponent::SpendPointOnPandora(UPandoraDefinition* Pandora)
{
	if (HasPandoraTreeAuthority())
	{
		SpendPointOnPandoraInternal(Pandora);
	}
	else
	{
		ServerSpendPointOnPandora(Pandora);
	}
}

// 클라이언트가 표시한 투자 가능 여부를 신뢰하지 않고 서버 상태로 다시 판정한다.
void UPandoraTreeComponent::ServerSpendPointOnPandora_Implementation(UPandoraDefinition* Pandora)
{
	if (!SpendPointOnPandoraInternal(Pandora))
	{
		LogRejectedServerRequest(TEXT("SpendPoint"), FString::Printf(
			TEXT("definition=%s points=%d request did not satisfy investment rules"), *GetPathNameSafe(Pandora), PointsAvailable));
	}
}

bool UPandoraTreeComponent::SpendPointOnPandoraInternal(UPandoraDefinition* Pandora)
{
	return HasGrantedPandora(Pandora) ? LevelUpGrantedPandora(Pandora) : GrantPandora(Pandora);
}

bool UPandoraTreeComponent::FindGrantedPandora(UPandoraDefinition* Pandora, FGrantedPandora& OutGrantedPandora) const
{
	const int32 Index = FindGrantedPandoraIndex(Pandora);
	if (Index == INDEX_NONE)
	{
		OutGrantedPandora = FGrantedPandora();
		return false;
	}
	OutGrantedPandora = GrantedPandoras[Index];
	return true;
}

bool UPandoraTreeComponent::HasGrantedPandora(UPandoraDefinition* Pandora) const
{
	return FindGrantedPandoraIndex(Pandora) != INDEX_NONE;
}

bool UPandoraTreeComponent::IsPandoraUnlockedForTree(UPandoraDefinition* Pandora) const
{
	const UPandoraComponent* PandoraComponent = GetOwnerPandoraComponent();
	return PandoraComponent && PandoraComponent->HasPandoraDefinition(Pandora);
}

bool UPandoraTreeComponent::IsPandoraAvailableForInvestment(UPandoraDefinition* Pandora) const
{
	return IsValid(Pandora) && (HasGrantedPandora(Pandora) || IsPandoraUnlockedForTree(Pandora) || ArePandoraUnlockRulesMet(Pandora));
}

bool UPandoraTreeComponent::CanGivePandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost) const
{
	int32 Cost = 0;
	return !HasGrantedPandora(Pandora)
		&& TryGetInvestmentCost(Pandora, 0, ClampPandoraLevel(Pandora, StartingLevel), bIgnorePointCost, Cost);
}

bool UPandoraTreeComponent::CanLevelUpPandora(UPandoraDefinition* Pandora) const
{
	const int32 CurrentLevel = GetCurrentPandoraLevel(Pandora);
	int32 Cost = 0;
	return CurrentLevel > 0 && TryGetInvestmentCost(Pandora, CurrentLevel, CurrentLevel + 1, false, Cost);
}

bool UPandoraTreeComponent::CanSpendPointOnPandora(UPandoraDefinition* Pandora) const
{
	const int32 CurrentLevel = GetCurrentPandoraLevel(Pandora);
	int32 Cost = 0;
	return TryGetInvestmentCost(Pandora, CurrentLevel, CurrentLevel + 1, false, Cost);
}

bool UPandoraTreeComponent::ArePandoraUnlockRulesMet(UPandoraDefinition* Pandora) const
{
	if (!IsValid(Pandora))
	{
		return false;
	}

	for (const FPandoraUnlockRule& Rule : Pandora->UnlockRules)
	{
		if (Rule.RequiredPandora && GetCurrentPandoraLevel(Rule.RequiredPandora) < FMath::Max(Rule.RequiredLevel, 1))
		{
			return false;
		}
	}
	return true;
}

int32 UPandoraTreeComponent::GetRequiredPointsForPandora(UPandoraDefinition* Pandora, bool bNextLevel) const
{
	const int32 CurrentLevel = GetCurrentPandoraLevel(Pandora);
	return GetRequiredPointsForPandoraLevel(Pandora, FMath::Max(CurrentLevel + (bNextLevel ? 1 : 0), 1));
}

int32 UPandoraTreeComponent::GetRequiredPointsForPandoraLevel(UPandoraDefinition* Pandora, int32 Level) const
{
	return IsValid(Pandora) ? Pandora->GetRequiredPointsForLevel(Level) : 1;
}

int32 UPandoraTreeComponent::GetCurrentPandoraLevel(UPandoraDefinition* Pandora) const
{
	const int32 Index = FindGrantedPandoraIndex(Pandora);
	return Index != INDEX_NONE ? ClampPandoraLevel(Pandora, GrantedPandoras[Index].Level) : 0;
}

int32 UPandoraTreeComponent::GetMaxPandoraLevel(UPandoraDefinition* Pandora) const
{
	return IsValid(Pandora) ? Pandora->GetMaxLevel() : 0;
}

void UPandoraTreeComponent::ResetPandora()
{
	if (HasPandoraTreeAuthority())
	{
		ResetPandoraInternal();
	}
	else
	{
		ServerResetPandora();
	}
}

void UPandoraTreeComponent::ServerResetPandora_Implementation()
{
	if (!ResetPandoraInternal())
	{
		LogRejectedServerRequest(TEXT("Reset"), TEXT("server-side reset policy rejected the request"));
	}
}

// 초기 무료 지급분을 제외한 투자 비용을 돌려준다. 소유권·실행 중 능력·쿨다운은 제거하지 않는다.
bool UPandoraTreeComponent::ResetPandoraInternal()
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras)
	{
		return false;
	}

	const int64 RefundPoints = FMath::Max<int64>(InitialPointsAvailable, static_cast<int64>(PointsAvailable) + CalculateSpentPandoraPoints());
	RestoreInitialPandoras(static_cast<int32>(FMath::Clamp<int64>(RefundPoints, 0, MAX_int32)));
	return true;
}

void UPandoraTreeComponent::SetPointsAvailable(int32 NewPointsAvailable)
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras)
	{
		return;
	}

	const int32 NewPoints = FMath::Max(NewPointsAvailable, 0);
	if (PointsAvailable != NewPoints)
	{
		PointsAvailable = NewPoints;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
		OnPointsChanged.Broadcast(PointsAvailable);
	}
}

// 처치 보상 등으로 받은 소울 더스트만 늘린다. 트리 레벨 변경 알림은 발생시키지 않는다.
bool UPandoraTreeComponent::AddSoulDust(const int32 Amount)
{
	if (!HasPandoraTreeAuthority() || bChangingPandoras || Amount <= 0 || PointsAvailable == MAX_int32)
	{
		return false;
	}
	SetPointsAvailable(static_cast<int32>(FMath::Min<int64>(static_cast<int64>(PointsAvailable) + Amount, MAX_int32)));
	return true;
}

void UPandoraTreeComponent::SetSoulDust(const int32 NewSoulDust)
{
	SetPointsAvailable(NewSoulDust);
}

void UPandoraTreeComponent::OnRep_GrantedPandoras()
{
	OnPandorasChanged.Broadcast();
}

void UPandoraTreeComponent::HandlePandoraInventoryChanged()
{
	if (!bChangingPandoras)
	{
		OnPandorasChanged.Broadcast();
	}
}

void UPandoraTreeComponent::OnRep_PointsAvailable()
{
	OnPointsChanged.Broadcast(PointsAvailable);
}

bool UPandoraTreeComponent::HasPandoraTreeAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

UPandoraComponent* UPandoraTreeComponent::GetOwnerPandoraComponent() const
{
	const APdPlayerState* PlayerState = GetPlayerState<APdPlayerState>();
	return PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
}

int32 UPandoraTreeComponent::FindGrantedPandoraIndex(UPandoraDefinition* Pandora) const
{
	return IsValid(Pandora) ? GrantedPandoras.IndexOfByPredicate(
		[Pandora](const FGrantedPandora& Entry) { return Entry.Pandora == Pandora; }) : INDEX_NONE;
}

// 비용 합산이 int32 범위를 넘으면 구매를 거절하고, 지불 가능한 경우에만 int32로 변환한다.
bool UPandoraTreeComponent::TryGetInvestmentCost(
	UPandoraDefinition* Pandora, int32 CurrentLevel, int32 TargetLevel, bool bIgnorePointCost, int32& OutCost) const
{
	OutCost = 0;
	if (!IsValid(Pandora) || TargetLevel <= CurrentLevel || TargetLevel > Pandora->GetMaxLevel())
	{
		return false;
	}
	if (!bIgnorePointCost && CurrentLevel == 0 && !IsPandoraAvailableForInvestment(Pandora))
	{
		return false;
	}

	const int64 Cost = bIgnorePointCost ? 0 : CalculatePointCostForPandoraLevels(Pandora, CurrentLevel + 1, TargetLevel);
	if (Cost > PointsAvailable)
	{
		return false;
	}
	OutCost = static_cast<int32>(Cost);
	return true;
}

bool UPandoraTreeComponent::ApplyPandoraInvestment(UPandoraDefinition* Pandora, int32 EntryIndex, int32 NewLevel, int32 Cost)
{
	UPandoraComponent* PandoraComponent = GetOwnerPandoraComponent();
	if (!PandoraComponent)
	{
		return false;
	}

	TGuardValue<bool> ChangeGuard(bChangingPandoras, true);
	if (EntryIndex == INDEX_NONE)
	{
		GrantedPandoras.Emplace(Pandora, NewLevel);
	}
	else
	{
		GrantedPandoras[EntryIndex].Level = NewLevel;
	}
	PointsAvailable -= Cost;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	if (Cost > 0)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	}

	if (EntryIndex == INDEX_NONE)
	{
		PandoraComponent->GrantPandoraDefinition(Pandora);
	}
	if (PandoraComponent->GetCurrentPandoraDefinition() == Pandora)
	{
		PandoraComponent->RefreshCurrentPandoraForWeaponChange();
	}
	OnPandorasChanged.Broadcast();
	if (Cost > 0)
	{
		OnPointsChanged.Broadcast(PointsAvailable);
	}
	return true;
}

// 중간 레벨마다 재지급하지 않고 최종 상태를 먼저 적용해 UI와 스킬 바가 일관된 값을 읽게 한다.
void UPandoraTreeComponent::RestoreInitialPandoras(int32 NewPointsAvailable)
{
	TGuardValue<bool> ChangeGuard(bChangingPandoras, true);
	bool bPandorasChanged = !Algo::Compare(GrantedPandoras, InitialGrantedPandoras,
		[](const FGrantedPandora& A, const FGrantedPandora& B) { return A.Pandora == B.Pandora && A.Level == B.Level; });
	const bool bPointsChanged = PointsAvailable != NewPointsAvailable;
	GrantedPandoras = InitialGrantedPandoras;
	PointsAvailable = NewPointsAvailable;

	if (bPandorasChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	}
	if (bPointsChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	}

	if (UPandoraComponent* PandoraComponent = GetOwnerPandoraComponent())
	{
		for (const FGrantedPandora& Entry : GrantedPandoras)
		{
			if (!PandoraComponent->HasPandoraDefinition(Entry.Pandora))
			{
				bPandorasChanged = true;
				PandoraComponent->GrantPandoraDefinition(Entry.Pandora);
			}
		}
		if (bPandorasChanged)
		{
			PandoraComponent->RefreshCurrentPandoraForWeaponChange();
		}
	}
	if (bPandorasChanged)
	{
		OnPandorasChanged.Broadcast();
	}
	if (bPointsChanged)
	{
		OnPointsChanged.Broadcast(PointsAvailable);
	}
}

int32 UPandoraTreeComponent::ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const
{
	return FMath::Clamp(Level, 1, IsValid(Pandora) ? Pandora->GetMaxLevel() : 1);
}

int64 UPandoraTreeComponent::CalculatePointCostForPandoraLevels(const UPandoraDefinition* Pandora, int32 FirstLevel, int32 LastLevel) const
{
	int64 Cost = 0;
	if (IsValid(Pandora))
	{
		for (int32 Level = FMath::Max(FirstLevel, 1); Level <= FMath::Min(LastLevel, Pandora->GetMaxLevel()); ++Level)
		{
			Cost += Pandora->GetRequiredPointsForLevel(Level);
		}
	}
	return Cost;
}

int64 UPandoraTreeComponent::CalculateSpentPandoraPoints() const
{
	// 현재 정책은 경기 중 비용 정의가 고정되고, 무료 지급은 초기 구성에만 있다는 전제다.
	int64 SpentPoints = 0;
	for (const FGrantedPandora& Entry : GrantedPandoras)
	{
		const FGrantedPandora* Initial = InitialGrantedPandoras.FindByKey(Entry);
		SpentPoints += CalculatePointCostForPandoraLevels(Entry.Pandora, Initial ? Initial->Level + 1 : 1, Entry.Level);
	}
	return SpentPoints;
}

void UPandoraTreeComponent::LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason)
{
	uint32 SuppressedCount = 0;
	if (ServerValidationLogLimiter.TryAcquire(5.0, SuppressedCount))
	{
		UE_LOG(LogPandoraTreeComponent, Warning, TEXT("Rejected Pandora Tree server request. Owner=%s Request=%s Reason=%s SuppressedSinceLast=%u"),
			*GetPathNameSafe(GetOwner()), RequestName, *Reason, SuppressedCount);
	}
}
