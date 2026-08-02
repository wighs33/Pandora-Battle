#include "Component/AbilitySystem/PandoraTreeComponent.h"

#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Lobby/Contents/LobbyPlayerState.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "UObject/PrimaryAssetId.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraTreeComponent)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraTreeComponent, Log, All);

namespace
{
	constexpr double ServerValidationLogIntervalSeconds = 5.0;
}

UPandoraTreeComponent::UPandoraTreeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UPandoraTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeVariables();


	const bool bDeferPlayerStateInitializationToPossessedPawn = OwnerPlayerState != nullptr;
	if (HasPandoraTreeAuthority()
		&& bInitializeDefaultPandorasOnBeginPlay
		&& !bDeferPlayerStateInitializationToPossessedPawn
		&& (!DefaultPandoras.IsEmpty() || DefaultPandoraPoints > 0))
	{
		InitializePandoraTree(TArray<FGrantedPandora>(), FMath::Max(DefaultPandoraPoints, 0));
	}
}

void UPandoraTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OwnerASC = nullptr;
	OwnerPlayerState = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UPandoraTreeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, GrantedPandoras, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, OwnedPandoraNames, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, PointsAvailable, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraTreeComponent, PandoraDefinition, Params);
}

bool UPandoraTreeComponent::InitializeForCurrentSession()
{
	InitializeVariables();

	if (bCurrentSessionInitialized)
	{
		return true;
	}

	if (!HasPandoraTreeAuthority()
		|| !OwnerPlayerState
		|| OwnerPlayerState->IsA<ALobbyPlayerState>())
	{
		return false;
	}

	UPandoraComponent* PandoraComponent = OwnerPlayerState->GetPandoraComponent();
	UPdGameInstance* GameInstance = OwnerPlayerState->GetGameInstance<UPdGameInstance>();
	if (!PandoraComponent || !GameInstance)
	{
		return false;
	}

	TArray<FName> DefaultOwnedPandoraNames;
	TArray<FPrimaryAssetId> DefaultUnlockedPandoraIds;
	GameInstance->BuildDefaultUnlockedPandoras(
		DefaultOwnedPandoraNames,
		&DefaultUnlockedPandoraIds);

	SetOwnedPandoraNames(DefaultOwnedPandoraNames);
	InitializePandoraTree(TArray<FGrantedPandora>(), -1, false);
	PandoraComponent->ActivatePandoras(DefaultUnlockedPandoraIds);
	bCurrentSessionInitialized = true;
	return true;
}

void UPandoraTreeComponent::InitializePandoraTree(
	const TArray<FGrantedPandora>& InGrantedPandoras,
	int32 InPointsAvailable,
	const bool bIncludeConfiguredDefaultPandoras)
{
	if (!HasPandoraTreeAuthority())
	{

		return;
	}

	InitializeVariables();

	GrantedPandoras.Reset();
	PointsAvailable = InPointsAvailable >= 0 ? InPointsAvailable : FMath::Max(DefaultPandoraPoints, 0);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);


	TArray<FGrantedPandora> ValidDefaultPandoras;
	if (bIncludeConfiguredDefaultPandoras)
	{
		CollectValidGrantedPandoras(DefaultPandoras, ValidDefaultPandoras);
	}

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

void UPandoraTreeComponent::SetOwnedPandoraNames(const TArray<FName>& InOwnedPandoraNames)
{
	if (!HasPandoraTreeAuthority())
	{

		return;
	}

	TArray<FName> NormalizedOwnedPandoraNames;
	for (const FName PandoraName : InOwnedPandoraNames)
	{
		if (!PandoraName.IsNone())
		{
			NormalizedOwnedPandoraNames.AddUnique(PandoraName);
		}
	}

	if (OwnedPandoraNames == NormalizedOwnedPandoraNames)
	{
		return;
	}

	OwnedPandoraNames = MoveTemp(NormalizedOwnedPandoraNames);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, OwnedPandoraNames, this);

	BroadcastPandoraTreeChanged();
}

void UPandoraTreeComponent::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	if (InPandoraDefinition && !CanReferencePandoraDefinition(InPandoraDefinition))
	{
		return;
	}

	if (PandoraDefinition.Get() == InPandoraDefinition)
	{
		return;
	}

	PandoraDefinition = InPandoraDefinition;
	if (HasPandoraTreeAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PandoraDefinition, this);
	}
	BroadcastPandoraTreeChanged();

	if (!HasPandoraTreeAuthority())
	{
		ServerSetPandoraDefinition(InPandoraDefinition);
	}
}

void UPandoraTreeComponent::ServerSetPandoraDefinition_Implementation(UPandoraDefinition* InPandoraDefinition)
{
	if (InPandoraDefinition && !CanReferencePandoraDefinition(InPandoraDefinition))
	{
		LogRejectedServerRequest(
			TEXT("SetDefinition"),
			FString::Printf(
				TEXT("invalid Pandora definition=%s"),
				*GetPathNameSafe(InPandoraDefinition)));
		return;
	}

	SetPandoraDefinition(InPandoraDefinition);
}

UPandoraDefinition* UPandoraTreeComponent::GetPandoraDefinition() const
{
	return const_cast<UPandoraDefinition*>(GetCurrentPandoraDefinition());
}

bool UPandoraTreeComponent::GrantPandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost)
{
	if (!HasPandoraTreeAuthority() || !CanReferencePandoraDefinition(Pandora))
	{

		return false;
	}

	InitializeVariables();
	if (!CanGivePandora(Pandora, StartingLevel, bIgnorePointCost))
	{
		return false;
	}

	const int32 ClampedLevel = ClampPandoraLevel(Pandora, StartingLevel);
	if (!bIgnorePointCost)
	{
		const int32 RequiredPoints = CalculatePointCostForPandoraLevels(Pandora, 1, ClampedLevel);
		if (PointsAvailable < RequiredPoints)
		{
			return false;
		}

		PointsAvailable -= RequiredPoints;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);
	}

	GrantedPandoras.Add(FGrantedPandora(Pandora, ClampedLevel));
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, GrantedPandoras, this);


	if (UPandoraComponent* PandoraComponent = OwnerPlayerState ? OwnerPlayerState->GetPandoraComponent() : nullptr)
	{
		PandoraComponent->ActivatePandoras({ Pandora->GetPrimaryAssetId() });
	}

	BroadcastPandoraTreeChanged();
	return true;
}

bool UPandoraTreeComponent::LevelUpGrantedPandora(UPandoraDefinition* Pandora)
{
	if (!HasPandoraTreeAuthority() || !CanReferencePandoraDefinition(Pandora) || !CanLevelUpPandora(Pandora))
	{

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

	return true;
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

void UPandoraTreeComponent::ServerSpendPointOnPandora_Implementation(UPandoraDefinition* Pandora)
{
	if (!SpendPointOnPandoraInternal(Pandora))
	{
		LogRejectedServerRequest(
			TEXT("SpendPoint"),
			FString::Printf(
				TEXT("definition=%s points=%d request did not satisfy unlock, level, or ownership rules"),
				*GetPathNameSafe(Pandora),
				PointsAvailable));
	}
}

bool UPandoraTreeComponent::FindGrantedPandora(UPandoraDefinition* Pandora, FGrantedPandora& OutGrantedPandora) const
{
	if (!CanReferencePandoraDefinition(Pandora))
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

bool UPandoraTreeComponent::IsPandoraUnlockedForTree(UPandoraDefinition* Pandora) const
{
	return CanReferencePandoraDefinition(Pandora)
		&& (HasGrantedPandora(Pandora) || OwnedPandoraNames.Contains(Pandora->GetFName()));
}

bool UPandoraTreeComponent::CanGivePandora(UPandoraDefinition* Pandora, int32 StartingLevel, bool bIgnorePointCost) const
{
	if (!CanReferencePandoraDefinition(Pandora) || HasGrantedPandora(Pandora))
	{
		return false;
	}

	if (!bIgnorePointCost && !IsPandoraUnlockedForTree(Pandora) && !ArePandoraUnlockRulesMet(Pandora))
	{
		return false;
	}

	if (!bIgnorePointCost)
	{
		const int32 ClampedLevel = ClampPandoraLevel(Pandora, StartingLevel);
		const int32 RequiredPoints = CalculatePointCostForPandoraLevels(Pandora, 1, ClampedLevel);
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
	if (!CanReferencePandoraDefinition(Pandora) || !FindGrantedPandora(Pandora, GrantedPandora))
	{
		return false;
	}

	const int32 NewLevel = GrantedPandora.Level + 1;
	return NewLevel <= GetMaxPandoraLevel(Pandora)
		&& PointsAvailable >= GetRequiredPointsForPandora(Pandora, true);
}

bool UPandoraTreeComponent::CanSpendPointOnPandora(UPandoraDefinition* Pandora) const
{
	if (!CanReferencePandoraDefinition(Pandora))
	{
		return false;
	}

	return HasGrantedPandora(Pandora)
		? CanLevelUpPandora(Pandora)
		: CanGivePandora(Pandora, 1, false);
}

bool UPandoraTreeComponent::ArePandoraUnlockRulesMet(UPandoraDefinition* Pandora) const
{
	if (!CanReferencePandoraDefinition(Pandora))
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
	if (!CanReferencePandoraDefinition(Pandora) || Pandora->UnlockRules.IsEmpty())
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
	if (!CanReferencePandoraDefinition(Pandora))
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
	return CanReferencePandoraDefinition(Pandora) ? Pandora->GetRequiredPointsForLevel(Level) : 1;
}

int32 UPandoraTreeComponent::GetCurrentPandoraLevel(UPandoraDefinition* Pandora) const
{
	FGrantedPandora GrantedPandora;
	return FindGrantedPandora(Pandora, GrantedPandora) ? ClampPandoraLevel(Pandora, GrantedPandora.Level) : 0;
}

int32 UPandoraTreeComponent::GetMaxPandoraLevel(UPandoraDefinition* Pandora) const
{
	return CanReferencePandoraDefinition(Pandora) ? Pandora->GetMaxLevel() : 0;
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
		LogRejectedServerRequest(
			TEXT("Reset"),
			TEXT("server-side reset policy rejected the request"));
	}
}

bool UPandoraTreeComponent::ResetPandoraInternal()
{
	if (!HasPandoraTreeAuthority())
	{
		return false;
	}

	InitializeVariables();

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
	return true;
}

void UPandoraTreeComponent::SetPointsAvailable(int32 NewPointsAvailable)
{
	if (!HasPandoraTreeAuthority())
	{

		return;
	}

	const int32 ClampedPointsAvailable = FMath::Max(NewPointsAvailable, 0);
	if (PointsAvailable == ClampedPointsAvailable)
	{
		return;
	}

	PointsAvailable = ClampedPointsAvailable;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);

	BroadcastPandoraTreeChanged();
}

bool UPandoraTreeComponent::AddSoulDust(const int32 Amount)
{
	if (!HasPandoraTreeAuthority())
	{

		return false;
	}

	if (Amount <= 0)
	{
		return false;
	}

	const int64 NewSoulDust = static_cast<int64>(PointsAvailable) + static_cast<int64>(Amount);
	PointsAvailable = static_cast<int32>(FMath::Clamp<int64>(NewSoulDust, 0, MAX_int32));
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);

	BroadcastPandoraTreeChanged();
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

void UPandoraTreeComponent::OnRep_OwnedPandoraNames()
{

	OnPandorasChanged.Broadcast();
}

void UPandoraTreeComponent::OnRep_PointsAvailable()
{

	OnPointsChanged.Broadcast(PointsAvailable);
}

void UPandoraTreeComponent::InitializeVariables()
{
	APdPlayerState* CurrentOwnerPlayerState = Cast<APdPlayerState>(GetOwner());
	if (OwnerPlayerState != CurrentOwnerPlayerState)
	{
		OwnerPlayerState = CurrentOwnerPlayerState;
		OwnerASC = nullptr;
	}

	if (OwnerPlayerState)
	{
		OwnerASC = OwnerPlayerState->GetPdAbilitySystemComponent();
	}
}

bool UPandoraTreeComponent::HasPandoraTreeAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UPandoraTreeComponent::LogRejectedServerRequest(
	const TCHAR* RequestName,
	const FString& Reason)
{
	uint32 SuppressedCount = 0;
	if (!ServerValidationLogLimiter.TryAcquire(
		ServerValidationLogIntervalSeconds,
		SuppressedCount))
	{
		return;
	}

	UE_LOG(
		LogPandoraTreeComponent,
		Warning,
		TEXT("Rejected Pandora Tree server request. Owner=%s Request=%s Reason=%s "
			"SuppressedSinceLast=%u"),
		*GetPathNameSafe(GetOwner()),
		RequestName,
		*Reason,
		SuppressedCount);
}

bool UPandoraTreeComponent::CanReferencePandoraDefinition(const UPandoraDefinition* Pandora) const
{
	if (!IsValid(Pandora))
	{
		return false;
	}

	const FPrimaryAssetId PrimaryAssetId = Pandora->GetPrimaryAssetId();
	return PrimaryAssetId.IsValid()
		&& PrimaryAssetId.PrimaryAssetType == FPrimaryAssetType(TEXT("PandoraDefinition"));
}

bool UPandoraTreeComponent::SpendPointOnPandoraInternal(UPandoraDefinition* Pandora)
{
	if (!HasPandoraTreeAuthority() || !CanSpendPointOnPandora(Pandora))
	{
		return false;
	}

	return HasGrantedPandora(Pandora)
		? LevelUpGrantedPandora(Pandora)
		: GrantPandora(Pandora, 1, false);
}

bool UPandoraTreeComponent::SpendPointsForPandora(UPandoraDefinition* Pandora, int32 Level)
{
	if (!CanReferencePandoraDefinition(Pandora))
	{
		return false;
	}

	const int32 RequiredPoints = CalculatePointCostForPandoraLevels(Pandora, Level, Level);
	if (PointsAvailable < RequiredPoints)
	{

		return false;
	}

	PointsAvailable -= RequiredPoints;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraTreeComponent, PointsAvailable, this);

	return true;
}

int32 UPandoraTreeComponent::ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const
{
	const int32 MaxLevel = Pandora ? Pandora->GetMaxLevel() : 1;
	return FMath::Clamp(Level, 1, FMath::Max(MaxLevel, 1));
}

int32 UPandoraTreeComponent::CalculatePointCostForPandoraLevels(
	const UPandoraDefinition* Pandora,
	const int32 FirstLevel,
	const int32 LastLevel) const
{
	if (!CanReferencePandoraDefinition(Pandora))
	{
		return 0;
	}

	if (FirstLevel > LastLevel)
	{
		return 0;
	}

	const int32 ClampedFirstLevel = ClampPandoraLevel(Pandora, FirstLevel);
	const int32 ClampedLastLevel = ClampPandoraLevel(Pandora, LastLevel);
	if (ClampedFirstLevel > ClampedLastLevel)
	{
		return 0;
	}

	int32 TotalCost = 0;
	for (int32 Level = ClampedFirstLevel; Level <= ClampedLastLevel; ++Level)
	{
		TotalCost += FMath::Max(Pandora->GetRequiredPointsForLevel(Level), 0);
	}

	return TotalCost;
}

bool UPandoraTreeComponent::IncrementGrantedPandoraLevel(UPandoraDefinition* Pandora, int32& OutNewLevel)
{
	if (!CanReferencePandoraDefinition(Pandora))
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
	if (CanReferencePandoraDefinition(PandoraDefinition.Get()))
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
		if (!CanReferencePandoraDefinition(SourcePandora.Pandora.Get()))
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
		if (!CanReferencePandoraDefinition(OverridePandora.Pandora.Get()))
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
	if (!CanReferencePandoraDefinition(Pandora))
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
		if (!CanReferencePandoraDefinition(GrantedPandora.Pandora.Get()))
		{
			continue;
		}

		const int32 DefaultGrantedLevel = GetGrantedDefaultPandoraLevel(GrantedPandora.Pandora.Get());
		const int32 FirstPaidLevel = FMath::Max(DefaultGrantedLevel + 1, 1);
		const int32 CurrentLevel = ClampPandoraLevel(GrantedPandora.Pandora.Get(), GrantedPandora.Level);
		SpentPoints += CalculatePointCostForPandoraLevels(
			GrantedPandora.Pandora.Get(),
			FirstPaidLevel,
			CurrentLevel);
	}

	return FMath::Max(SpentPoints, 0);
}

int32 UPandoraTreeComponent::CalculateResetPandoraPoints() const
{
	const int64 ResetPoints = FMath::Max<int64>(
		FMath::Max(DefaultPandoraPoints, 0),
		static_cast<int64>(PointsAvailable) + static_cast<int64>(CalculateSpentPandoraPoints()));
	return static_cast<int32>(FMath::Clamp<int64>(ResetPoints, 0, MAX_int32));
}

void UPandoraTreeComponent::BroadcastPandoraTreeChanged()
{
	OnPandorasChanged.Broadcast();
	OnPointsChanged.Broadcast(PointsAvailable);
}
