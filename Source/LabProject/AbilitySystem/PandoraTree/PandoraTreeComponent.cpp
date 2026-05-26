#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"

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

	InitializeVariables();
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("BeginPlay: component=%s owner=%s authority=%s defaultPoints=%d defaultPandoras=%d initDefaults=%s asc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		FMath::Max(DefaultPandoraPoints, 0),
		DefaultPandoras.Num(),
		bInitializeDefaultPandorasOnBeginPlay ? TEXT("true") : TEXT("false"),
		*GetNameSafe(OwnerASC));

	const bool bDeferPlayerStateInitializationToPossessedPawn = OwnerPlayerState != nullptr;
	if (HasAuthority()
		&& bInitializeDefaultPandorasOnBeginPlay
		&& !bDeferPlayerStateInitializationToPossessedPawn
		&& (!DefaultPandoras.IsEmpty() || DefaultPandoraPoints > 0))
	{
		InitializePandoraTree(TArray<FGrantedPandora>(), FMath::Max(DefaultPandoraPoints, 0));
	}
}

void UPandoraTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, GrantedPandoras, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, PointsAvailable, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, PandoraDefinition, Params);
}

void UPandoraTreeComponent::InitializePandoraTree(
	const TArray<FGrantedPandora>& InGrantedPandoras,
	int32 InPointsAvailable)
{
	if (!HasAuthority())
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("Initialize skipped: no authority. component=%s owner=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()));
		return;
	}

	InitializeVariables();

	GrantedPandoras.Reset();
	PointsAvailable = InPointsAvailable >= 0 ? InPointsAvailable : FMath::Max(DefaultPandoraPoints, 0);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("InitializePandoraTree: component=%s owner=%s points=%d defaultPandoras=%d inputPandoras=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		PointsAvailable,
		DefaultPandoras.Num(),
		InGrantedPandoras.Num());

	TArray<FGrantedPandora> ValidDefaultPandoras;
	CollectValidGrantedPandoras(DefaultPandoras, ValidDefaultPandoras);

	TArray<FGrantedPandora> ValidInputPandoras;
	CollectValidGrantedPandoras(InGrantedPandoras, ValidInputPandoras);

	TArray<FGrantedPandora> PandorasToGrant;
	MergeGrantedPandoras(ValidDefaultPandoras, ValidInputPandoras, PandorasToGrant);
	for (const FGrantedPandora& GrantedPandora : PandorasToGrant)
	{
		if (GrantedPandora.Pandora)
		{
			GrantPandora(GrantedPandora.Pandora, GrantedPandora.Level, true);
		}
	}

	BroadcastPandoraTreeChanged();
}

void UPandoraTreeComponent::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	if (PandoraDefinition.Get() == InPandoraDefinition)
	{
		return;
	}

	PandoraDefinition = InPandoraDefinition;
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PandoraDefinition, this);
	}
	BroadcastPandoraTreeChanged();

	if (!HasAuthority())
	{
		ServerSetPandoraDefinition(InPandoraDefinition);
	}
}

void UPandoraTreeComponent::ServerSetPandoraDefinition_Implementation(UPandoraDefinition* InPandoraDefinition)
{
	SetPandoraDefinition(InPandoraDefinition);
}

UPandoraDefinition* UPandoraTreeComponent::GetPandoraDefinition() const
{
	return const_cast<UPandoraDefinition*>(GetCurrentPandoraDefinition());
}

bool UPandoraTreeComponent::GrantPandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost)
{
	if (!HasAuthority() || !Pandora)
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[GrantPandora] skipped: authority=%s component=%s owner=%s pandora=%s"),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Pandora));
		return false;
	}

	InitializeVariables();
	if (!CanGivePandora(Pandora, StartingLevel, bIgnorePointCost))
	{
		const int32 ClampedLevelForCost = ClampPandoraLevel(Pandora, StartingLevel);
		int32 RequiredPoints = 0;
		for (int32 Level = 1; Level <= ClampedLevelForCost; ++Level)
		{
			RequiredPoints += GetRequiredPointsForPandoraLevel(Pandora, Level);
		}

		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[GrantPandora] blocked: component=%s owner=%s pandora=%s startingLevel=%d clampedLevel=%d points=%d required=%d ignoreCost=%s alreadyGranted=%s max=%d skills=%d"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Pandora),
			StartingLevel,
			ClampedLevelForCost,
			PointsAvailable,
			RequiredPoints,
			bIgnorePointCost ? TEXT("true") : TEXT("false"),
			HasGrantedPandora(Pandora) ? TEXT("true") : TEXT("false"),
			Pandora->GetMaxLevel(),
			Pandora->Skill.Num());
		return false;
	}

	const int32 ClampedLevel = ClampPandoraLevel(Pandora, StartingLevel);
	if (!bIgnorePointCost)
	{
		int32 RequiredPoints = 0;
		for (int32 Level = 1; Level <= ClampedLevel; ++Level)
		{
			RequiredPoints += GetRequiredPointsForPandoraLevel(Pandora, Level);
		}

		if (PointsAvailable < RequiredPoints)
		{
			return false;
		}

		PointsAvailable -= RequiredPoints;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	}

	GrantedPandoras.Add(FGrantedPandora(Pandora, ClampedLevel));
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[GrantPandora] unlocked pandora level. component=%s owner=%s pandora=%s level=%d remainingPoints=%d skills=%d icon=%s activeIcon=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Pandora),
		ClampedLevel,
		PointsAvailable,
		Pandora->Skill.Num(),
		*GetNameSafe(Pandora->GetIconResource()),
		*GetNameSafe(Pandora->GetActiveIconResource()));

	if (UPandoraComponent* PandoraComponent = OwnerPlayerState ? OwnerPlayerState->GetPandoraComponent() : nullptr)
	{
		PandoraComponent->ActivatePandoras({ Pandora->GetPrimaryAssetId() });
	}
	else
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[GrantPandora] unlocked but could not activate inventory entry: component=%s owner=%s pandora=%s pandoraComponent=None"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Pandora));
	}

	BroadcastPandoraTreeChanged();
	return true;
}

bool UPandoraTreeComponent::LevelUpGrantedPandora(UPandoraDefinition* Pandora)
{
	if (!HasAuthority() || !Pandora || !CanLevelUpPandora(Pandora))
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[LevelUpPandora] blocked: authority=%s component=%s owner=%s pandora=%s points=%d current=%d max=%d required=%d canLevelUp=%s"),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Pandora),
			PointsAvailable,
			Pandora ? GetCurrentPandoraLevel(Pandora) : INDEX_NONE,
			Pandora ? GetMaxPandoraLevel(Pandora) : INDEX_NONE,
			Pandora ? GetRequiredPointsForPandora(Pandora, true) : INDEX_NONE,
			Pandora && CanLevelUpPandora(Pandora) ? TEXT("true") : TEXT("false"));
		return false;
	}

	FGrantedPandora CurrentPandora;
	if (!FindGrantedPandora(Pandora, CurrentPandora))
	{
		return false;
	}

	const int32 NewLevel = CurrentPandora.Level + 1;
	if (!SpendPointsForPandora(Pandora, NewLevel))
	{
		return false;
	}

	int32 UpdatedLevel = CurrentPandora.Level;
	if (!IncrementGrantedPandoraLevel(Pandora, UpdatedLevel))
	{
		return false;
	}

	if (UPandoraComponent* PandoraComponent = OwnerPlayerState ? OwnerPlayerState->GetPandoraComponent() : nullptr)
	{
		if (PandoraComponent->GetCurrentPandoraDefinition() == Pandora)
		{
			PandoraComponent->RequestPandoraSelection(Pandora);
		}
	}
	BroadcastPandoraTreeChanged();
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[LevelUpPandora] success: component=%s owner=%s pandora=%s level=%d remainingPoints=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Pandora),
		UpdatedLevel,
		PointsAvailable);
	return true;
}

void UPandoraTreeComponent::SpendPointOnPandora(UPandoraDefinition* Pandora)
{
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[SpendPandora] requested: component=%s owner=%s authority=%s pandora=%s points=%d current=%d max=%d required=%d canSpend=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Pandora),
		PointsAvailable,
		Pandora ? GetCurrentPandoraLevel(Pandora) : INDEX_NONE,
		Pandora ? GetMaxPandoraLevel(Pandora) : INDEX_NONE,
		Pandora ? GetRequiredPointsForPandora(Pandora, HasGrantedPandora(Pandora)) : INDEX_NONE,
		Pandora && CanSpendPointOnPandora(Pandora) ? TEXT("true") : TEXT("false"));

	if (HasAuthority())
	{
		ServerSpendPointOnPandora_Implementation(Pandora);
	}
	else
	{
		ServerSpendPointOnPandora(Pandora);
	}
}

void UPandoraTreeComponent::ServerSpendPointOnPandora_Implementation(UPandoraDefinition* Pandora)
{
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[ServerSpendPandora] executing: component=%s owner=%s authority=%s pandora=%s points=%d alreadyGranted=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Pandora),
		PointsAvailable,
		Pandora && HasGrantedPandora(Pandora) ? TEXT("true") : TEXT("false"));

	if (!Pandora)
	{
		return;
	}

	if (HasGrantedPandora(Pandora))
	{
		LevelUpGrantedPandora(Pandora);
	}
	else
	{
		GrantPandora(Pandora, 1, false);
	}
}

bool UPandoraTreeComponent::FindGrantedPandora(UPandoraDefinition* Pandora, FGrantedPandora& OutGrantedPandora) const
{
	if (!Pandora)
	{
		return false;
	}

	for (const FGrantedPandora& GrantedPandora : GrantedPandoras)
	{
		if (GrantedPandora.Pandora == Pandora)
		{
			OutGrantedPandora = GrantedPandora;
			return true;
		}
	}

	return false;
}

bool UPandoraTreeComponent::HasGrantedPandora(UPandoraDefinition* Pandora) const
{
	FGrantedPandora IgnoredPandora;
	return FindGrantedPandora(Pandora, IgnoredPandora);
}

bool UPandoraTreeComponent::CanGivePandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost) const
{
	if (!Pandora || HasGrantedPandora(Pandora))
	{
		return false;
	}

	if (!bIgnorePointCost && !ArePandoraUnlockRulesMet(Pandora))
	{
		return false;
	}

	if (!bIgnorePointCost)
	{
		const int32 ClampedLevel = ClampPandoraLevel(Pandora, StartingLevel);
		int32 RequiredPoints = 0;
		for (int32 Level = 1; Level <= ClampedLevel; ++Level)
		{
			RequiredPoints += GetRequiredPointsForPandoraLevel(Pandora, Level);
		}

		if (PointsAvailable < RequiredPoints)
		{
			return false;
		}
	}

	return true;
}

bool UPandoraTreeComponent::CanLevelUpPandora(UPandoraDefinition* Pandora) const
{
	FGrantedPandora GrantedPandora;
	if (!Pandora || !FindGrantedPandora(Pandora, GrantedPandora))
	{
		return false;
	}

	const int32 NewLevel = GrantedPandora.Level + 1;
	return NewLevel <= GetMaxPandoraLevel(Pandora)
		&& PointsAvailable >= GetRequiredPointsForPandora(Pandora, true);
}

bool UPandoraTreeComponent::CanSpendPointOnPandora(UPandoraDefinition* Pandora) const
{
	if (!Pandora)
	{
		return false;
	}

	return HasGrantedPandora(Pandora)
		? CanLevelUpPandora(Pandora)
		: CanGivePandora(Pandora, 1, false);
}

bool UPandoraTreeComponent::ArePandoraUnlockRulesMet(UPandoraDefinition* Pandora) const
{
	if (!Pandora)
	{
		return false;
	}

	for (const FPandoraUnlockRule& UnlockRule : Pandora->UnlockRules)
	{
		if (!UnlockRule.RequiredPandora)
		{
			continue;
		}

		const int32 CurrentLevel = GetCurrentPandoraLevel(UnlockRule.RequiredPandora.Get());
		if (CurrentLevel < FMath::Max(UnlockRule.RequiredLevel, 1))
		{
			return false;
		}
	}

	return true;
}

FText UPandoraTreeComponent::GetPandoraUnlockRequirementsText(UPandoraDefinition* Pandora) const
{
	if (!Pandora || Pandora->UnlockRules.IsEmpty())
	{
		return FText::GetEmpty();
	}

	FString RequirementLines;
	for (const FPandoraUnlockRule& UnlockRule : Pandora->UnlockRules)
	{
		if (!UnlockRule.RequiredPandora)
		{
			continue;
		}

		const int32 RequiredLevel = FMath::Max(UnlockRule.RequiredLevel, 1);
		const int32 CurrentLevel = GetCurrentPandoraLevel(UnlockRule.RequiredPandora.Get());
		const FText RequirementLine = FText::Format(
			NSLOCTEXT("PandoraTreeComponent", "PandoraUnlockRequirementLine", "- {0} Lv. {1} ({2}/{1})"),
			UnlockRule.RequiredPandora->GetDisplayName(),
			FText::AsNumber(RequiredLevel),
			FText::AsNumber(CurrentLevel));

		if (!RequirementLines.IsEmpty())
		{
			RequirementLines += LINE_TERMINATOR;
		}

		RequirementLines += RequirementLine.ToString();
	}

	if (RequirementLines.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		NSLOCTEXT("PandoraTreeComponent", "PandoraUnlockRequirements", "요구 조건\n{0}"),
		FText::FromString(RequirementLines));
}

int32 UPandoraTreeComponent::GetRequiredPointsForPandora(UPandoraDefinition* Pandora, bool bNextLevel) const
{
	if (!Pandora)
	{
		return 1;
	}

	FGrantedPandora GrantedPandora;
	const bool bFound = FindGrantedPandora(Pandora, GrantedPandora);
	const int32 RequiredLevel = bFound
		? GrantedPandora.Level + (bNextLevel ? 1 : 0)
		: 1;
	return GetRequiredPointsForPandoraLevel(Pandora, RequiredLevel);
}

int32 UPandoraTreeComponent::GetRequiredPointsForPandoraLevel(UPandoraDefinition* Pandora, int32 Level) const
{
	return Pandora ? Pandora->GetRequiredPointsForLevel(Level) : 1;
}

int32 UPandoraTreeComponent::GetCurrentPandoraLevel(UPandoraDefinition* Pandora) const
{
	FGrantedPandora GrantedPandora;
	return FindGrantedPandora(Pandora, GrantedPandora) ? GrantedPandora.Level : 0;
}

int32 UPandoraTreeComponent::GetMaxPandoraLevel(UPandoraDefinition* Pandora) const
{
	return Pandora ? Pandora->GetMaxLevel() : 0;
}

TMap<FName, int32> UPandoraTreeComponent::GetGrantedPandoraLevelsByName() const
{
	TMap<FName, int32> PandorasByName;
	for (const FGrantedPandora& GrantedPandora : GrantedPandoras)
	{
		if (GrantedPandora.Pandora)
		{
			PandorasByName.Add(GrantedPandora.Pandora->GetFName(), GrantedPandora.Level);
		}
	}

	return PandorasByName;
}

void UPandoraTreeComponent::ResetPandora()
{
	if (HasAuthority())
	{
		ServerResetPandora_Implementation();
	}
	else
	{
		ServerResetPandora();
	}
}

void UPandoraTreeComponent::ServerResetPandora_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	InitializeVariables();

	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("ServerResetPandora: component=%s owner=%s oldPoints=%d oldGrantedPandoras=%d defaultPoints=%d resetPoints=%d spentPoints=%d defaultPandoras=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		PointsAvailable,
		GrantedPandoras.Num(),
		FMath::Max(DefaultPandoraPoints, 0),
		CalculateResetPandoraPoints(),
		CalculateSpentPandoraPoints(),
		DefaultPandoras.Num());

	const int32 ResetPoints = CalculateResetPandoraPoints();
	GrantedPandoras.Reset();
	PointsAvailable = ResetPoints;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);

	TArray<FGrantedPandora> ValidDefaultPandoras;
	CollectValidGrantedPandoras(DefaultPandoras, ValidDefaultPandoras);

	for (const FGrantedPandora& DefaultPandora : ValidDefaultPandoras)
	{
		if (DefaultPandora.Pandora)
		{
			GrantPandora(DefaultPandora.Pandora, DefaultPandora.Level, true);
		}
	}

	BroadcastPandoraTreeChanged();
	RefreshSelectedPandoraAbilityBindings();
}

void UPandoraTreeComponent::SetPointsAvailable(int32 NewPointsAvailable)
{
	if (!HasAuthority())
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[SetPoints] skipped: no authority. component=%s owner=%s requested=%d"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			NewPointsAvailable);
		return;
	}

	PointsAvailable = FMath::Max(NewPointsAvailable, 0);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[SetPoints] component=%s owner=%s points=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		PointsAvailable);
	BroadcastPandoraTreeChanged();
}

void UPandoraTreeComponent::OnRep_GrantedPandoras()
{
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[OnRepGrantedPandoras] component=%s owner=%s count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		GrantedPandoras.Num());
	OnPandorasChanged.Broadcast();
}

void UPandoraTreeComponent::OnRep_PointsAvailable()
{
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[OnRepPoints] component=%s owner=%s points=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		PointsAvailable);
	OnPointsChanged.Broadcast(PointsAvailable);
}

void UPandoraTreeComponent::InitializeVariables()
{
	if (!OwnerPlayerState)
	{
		OwnerPlayerState = Cast<APdPlayerState>(GetOwner());
	}

	if (!OwnerASC && OwnerPlayerState)
	{
		OwnerASC = OwnerPlayerState->GetPdAbilitySystemComponent();
	}

	UE_LOG(LogPandoraTreeComponent, Verbose,
		TEXT("[InitializeVariables] component=%s owner=%s playerState=%s asc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(OwnerPlayerState.Get()),
		*GetNameSafe(OwnerASC.Get()));
}

bool UPandoraTreeComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

bool UPandoraTreeComponent::SpendPointsForPandora(UPandoraDefinition* Pandora, int32 Level)
{
	const int32 RequiredPoints = GetRequiredPointsForPandoraLevel(Pandora, Level);
	if (PointsAvailable < RequiredPoints)
	{
		UE_LOG(LogPandoraTreeComponent, Warning,
			TEXT("[SpendPointsPandora] failed: component=%s owner=%s pandora=%s level=%d points=%d required=%d"),
			*GetNameSafe(this),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Pandora),
			Level,
			PointsAvailable,
			RequiredPoints);
		return false;
	}

	PointsAvailable -= RequiredPoints;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	UE_LOG(LogPandoraTreeComponent, Log,
		TEXT("[SpendPointsPandora] success: component=%s owner=%s pandora=%s level=%d spent=%d remaining=%d"),
		*GetNameSafe(this),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Pandora),
		Level,
		RequiredPoints,
		PointsAvailable);
	return true;
}

int32 UPandoraTreeComponent::ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const
{
	const int32 MaxLevel = Pandora ? Pandora->GetMaxLevel() : 1;
	return FMath::Clamp(Level, 1, FMath::Max(MaxLevel, 1));
}

bool UPandoraTreeComponent::IncrementGrantedPandoraLevel(UPandoraDefinition* Pandora, int32& OutNewLevel)
{
	if (!Pandora)
	{
		return false;
	}

	for (FGrantedPandora& GrantedPandora : GrantedPandoras)
	{
		if (GrantedPandora.Pandora == Pandora)
		{
			GrantedPandora.Level = ClampPandoraLevel(Pandora, GrantedPandora.Level + 1);
			OutNewLevel = GrantedPandora.Level;
			MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
			return true;
		}
	}

	return false;
}

void UPandoraTreeComponent::SetLevelOfPandoraGrantedContent(UPandoraDefinition* Pandora, int32 NewLevel)
{
	(void)NewLevel;

	if (!Pandora || !OwnerPlayerState)
	{
		return;
	}

	if (UPandoraComponent* PandoraComponent = OwnerPlayerState->GetPandoraComponent())
	{
		if (PandoraComponent->GetCurrentPandoraDefinition() == Pandora)
		{
			PandoraComponent->RequestPandoraSelection(Pandora);
		}
	}
}

void UPandoraTreeComponent::RefreshSelectedPandoraAbilityBindings() const
{
	if (OwnerPlayerState && OwnerASC)
	{
		if (UPandoraComponent* PandoraComponent = OwnerPlayerState->GetPandoraComponent())
		{
			PandoraComponent->RefreshSelectedPandoraAbilityBindings(OwnerASC);
		}
	}
}

const UPandoraDefinition* UPandoraTreeComponent::GetCurrentPandoraDefinition() const
{
	if (PandoraDefinition)
	{
		return PandoraDefinition.Get();
	}

	if (!OwnerPlayerState)
	{
		return nullptr;
	}

	const UPandoraComponent* PandoraComponent = OwnerPlayerState->GetPandoraComponent();
	return PandoraComponent ? PandoraComponent->GetCurrentPandoraDefinition() : nullptr;
}

void UPandoraTreeComponent::CollectValidGrantedPandoras(const TArray<FGrantedPandora>& SourcePandoras, TArray<FGrantedPandora>& OutPandoras) const
{
	OutPandoras.Reset();

	for (const FGrantedPandora& SourcePandora : SourcePandoras)
	{
		if (!SourcePandora.Pandora)
		{
			continue;
		}

		OutPandoras.Add(FGrantedPandora(SourcePandora.Pandora, ClampPandoraLevel(SourcePandora.Pandora, SourcePandora.Level)));
	}
}

void UPandoraTreeComponent::MergeGrantedPandoras(
	const TArray<FGrantedPandora>& Defaults,
	const TArray<FGrantedPandora>& Overrides,
	TArray<FGrantedPandora>& OutPandoras) const
{
	OutPandoras = Defaults;

	for (const FGrantedPandora& OverridePandora : Overrides)
	{
		if (!OverridePandora.Pandora)
		{
			continue;
		}

		bool bUpdatedExisting = false;
		for (FGrantedPandora& ExistingPandora : OutPandoras)
		{
			if (ExistingPandora.Pandora == OverridePandora.Pandora)
			{
				ExistingPandora.Level = OverridePandora.Level;
				bUpdatedExisting = true;
				break;
			}
		}

		if (!bUpdatedExisting)
		{
			OutPandoras.Add(OverridePandora);
		}
	}
}

int32 UPandoraTreeComponent::GetGrantedDefaultPandoraLevel(UPandoraDefinition* Pandora) const
{
	if (!Pandora)
	{
		return 0;
	}

	for (const FGrantedPandora& DefaultPandora : DefaultPandoras)
	{
		if (DefaultPandora.Pandora == Pandora)
		{
			return ClampPandoraLevel(Pandora, DefaultPandora.Level);
		}
	}

	return 0;
}

int32 UPandoraTreeComponent::CalculateSpentPandoraPoints() const
{
	int32 SpentPoints = 0;
	for (const FGrantedPandora& GrantedPandora : GrantedPandoras)
	{
		if (!GrantedPandora.Pandora)
		{
			continue;
		}

		const int32 DefaultGrantedLevel = GetGrantedDefaultPandoraLevel(GrantedPandora.Pandora.Get());
		const int32 FirstPaidLevel = FMath::Max(DefaultGrantedLevel + 1, 1);
		const int32 CurrentLevel = ClampPandoraLevel(GrantedPandora.Pandora.Get(), GrantedPandora.Level);
		for (int32 Level = FirstPaidLevel; Level <= CurrentLevel; ++Level)
		{
			SpentPoints += GetRequiredPointsForPandoraLevel(GrantedPandora.Pandora.Get(), Level);
		}
	}

	return FMath::Max(SpentPoints, 0);
}

int32 UPandoraTreeComponent::CalculateResetPandoraPoints() const
{
	return FMath::Max(FMath::Max(DefaultPandoraPoints, 0), PointsAvailable + CalculateSpentPandoraPoints());
}

void UPandoraTreeComponent::BroadcastPandoraTreeChanged()
{
	OnPandorasChanged.Broadcast();
	OnPointsChanged.Broadcast(PointsAvailable);
}
