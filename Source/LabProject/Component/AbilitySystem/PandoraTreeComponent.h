#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "Logging/LogRateLimiter.h"
#include "PandoraTreeComponent.generated.h"

class APdPlayerState;
class UPdAbilitySystemComponent;
class UPandoraDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdPandoraTreeChangedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdPandoraPointsChangedDelegate, int32, PointsAvailable);

USTRUCT(BlueprintType)
struct LABPROJECT_API FGrantedPandora
{
	GENERATED_BODY()

	FGrantedPandora() = default;

	FGrantedPandora(UPandoraDefinition* InPandora, int32 InLevel)
		: Pandora(InPandora)
		, Level(InLevel)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pandora")
	TObjectPtr<UPandoraDefinition> Pandora;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Pandora", meta = (ClampMin = "1"))
	int32 Level = 1;

	bool IsValid() const
	{
		return Pandora != nullptr && Level > 0;
	}

	bool operator==(const FGrantedPandora& Other) const
	{
		return Pandora == Other.Pandora;
	}
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPandoraTreeComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPandoraTreeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Composes the default Pandora ownership/loadout once for this PlayerState's
	 * current match. Repeated pawn possession must not reset earned progress.
	 */
	bool InitializeForCurrentSession();

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void InitializePandoraTree(
		const TArray<FGrantedPandora>& InGrantedPandoras,
		int32 InPointsAvailable = -1,
		bool bIncludeConfiguredDefaultPandoras = true);

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void SetOwnedPandoraNames(const TArray<FName>& InOwnedPandoraNames);

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(Server, Reliable, Category = "!PandoraTree")
	void ServerSetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	UPandoraDefinition* GetPandoraDefinition() const;

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	bool GrantPandora(UPandoraDefinition* Pandora, int32 StartingLevel = 1, bool bIgnorePointCost = false);

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	bool LevelUpGrantedPandora(UPandoraDefinition* Pandora);

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void SpendPointOnPandora(UPandoraDefinition* Pandora);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "!PandoraTree")
	void ServerSpendPointOnPandora(UPandoraDefinition* Pandora);

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool FindGrantedPandora(UPandoraDefinition* Pandora, FGrantedPandora& OutGrantedPandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool HasGrantedPandora(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool IsPandoraUnlockedForTree(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool CanGivePandora(UPandoraDefinition* Pandora, int32 StartingLevel = 1, bool bIgnorePointCost = false) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool CanLevelUpPandora(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool CanSpendPointOnPandora(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool ArePandoraUnlockRulesMet(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	FText GetPandoraUnlockRequirementsText(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetRequiredPointsForPandora(UPandoraDefinition* Pandora, bool bNextLevel = false) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetRequiredPointsForPandoraLevel(UPandoraDefinition* Pandora, int32 Level) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetCurrentPandoraLevel(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetMaxPandoraLevel(UPandoraDefinition* Pandora) const;

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void ResetPandora();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "!PandoraTree")
	void ServerResetPandora();

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void SetPointsAvailable(int32 NewPointsAvailable);

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetPointsAvailable() const { return PointsAvailable; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!PandoraTree|Soul Dust")
	bool AddSoulDust(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!PandoraTree|Soul Dust")
	void SetSoulDust(int32 NewSoulDust);

	UFUNCTION(BlueprintPure, Category = "!PandoraTree|Soul Dust")
	int32 GetSoulDust() const { return PointsAvailable; }

	UPROPERTY(BlueprintAssignable, Category = "!PandoraTree")
	FPdPandoraTreeChangedDelegate OnPandorasChanged;

	UPROPERTY(BlueprintAssignable, Category = "!PandoraTree")
	FPdPandoraPointsChangedDelegate OnPointsChanged;

protected:
	UFUNCTION()
	void OnRep_GrantedPandoras();

	UFUNCTION()
	void OnRep_OwnedPandoraNames();

	UFUNCTION()
	void OnRep_PointsAvailable();

	void InitializeVariables();
	bool HasPandoraTreeAuthority() const;
	bool CanReferencePandoraDefinition(const UPandoraDefinition* Pandora) const;
	bool SpendPointOnPandoraInternal(UPandoraDefinition* Pandora);
	bool ResetPandoraInternal();
	bool SpendPointsForPandora(UPandoraDefinition* Pandora, int32 Level);
	int32 ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const;
	int32 CalculatePointCostForPandoraLevels(const UPandoraDefinition* Pandora, int32 FirstLevel, int32 LastLevel) const;
	bool IncrementGrantedPandoraLevel(UPandoraDefinition* Pandora, int32& OutNewLevel);
	void RefreshSelectedPandoraAfterLevelChange(const UPandoraDefinition* Pandora) const;
	void BroadcastPandoraTreeChanged();
	void RefreshSelectedPandoraAbilityBindings() const;
	const UPandoraDefinition* GetCurrentPandoraDefinition() const;
	void CollectValidGrantedPandoras(const TArray<FGrantedPandora>& SourcePandoras, TArray<FGrantedPandora>& OutPandoras) const;
	void MergeGrantedPandoras(const TArray<FGrantedPandora>& Defaults, const TArray<FGrantedPandora>& Overrides, TArray<FGrantedPandora>& OutPandoras) const;
	int32 GetGrantedDefaultPandoraLevel(UPandoraDefinition* Pandora) const;
	int32 CalculateSpentPandoraPoints() const;
	int32 CalculateResetPandoraPoints() const;
	void LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	bool bInitializeDefaultPandorasOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	int32 DefaultPandoraPoints = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	TArray<FGrantedPandora> DefaultPandoras;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "!PandoraTree")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_GrantedPandoras, BlueprintReadOnly, Category = "!PandoraTree")
	TArray<FGrantedPandora> GrantedPandoras;

	UPROPERTY(ReplicatedUsing = OnRep_OwnedPandoraNames, BlueprintReadOnly, Category = "!PandoraTree")
	TArray<FName> OwnedPandoraNames;

	UPROPERTY(ReplicatedUsing = OnRep_PointsAvailable, BlueprintReadOnly, Category = "!PandoraTree")
	int32 PointsAvailable = 0;

	UPROPERTY(Transient)
	TObjectPtr<APdPlayerState> OwnerPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> OwnerASC;

	UPROPERTY(Transient)
	bool bCurrentSessionInitialized = false;

	FLogRateLimiter ServerValidationLogLimiter;
};
