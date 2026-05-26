#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void InitializePandoraTree(const TArray<FGrantedPandora>& InGrantedPandoras, int32 InPointsAvailable = -1);

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

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	TArray<FGrantedPandora> GetGrantedPandoras() const { return GrantedPandoras; }

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	TMap<FName, int32> GetGrantedPandoraLevelsByName() const;

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void ResetPandora();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "!PandoraTree")
	void ServerResetPandora();

	UFUNCTION(BlueprintCallable, Category = "!PandoraTree")
	void SetPointsAvailable(int32 NewPointsAvailable);

	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	int32 GetPointsAvailable() const { return PointsAvailable; }

	UPROPERTY(BlueprintAssignable, Category = "!PandoraTree")
	FPdPandoraTreeChangedDelegate OnPandorasChanged;

	UPROPERTY(BlueprintAssignable, Category = "!PandoraTree")
	FPdPandoraPointsChangedDelegate OnPointsChanged;

protected:
	UFUNCTION()
	void OnRep_GrantedPandoras();

	UFUNCTION()
	void OnRep_PointsAvailable();

	void InitializeVariables();
	bool HasAuthority() const;
	bool SpendPointsForPandora(UPandoraDefinition* Pandora, int32 Level);
	int32 ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const;
	bool IncrementGrantedPandoraLevel(UPandoraDefinition* Pandora, int32& OutNewLevel);
	void SetLevelOfPandoraGrantedContent(UPandoraDefinition* Pandora, int32 NewLevel);
	void BroadcastPandoraTreeChanged();
	void RefreshSelectedPandoraAbilityBindings() const;
	const UPandoraDefinition* GetCurrentPandoraDefinition() const;
	void CollectValidGrantedPandoras(const TArray<FGrantedPandora>& SourcePandoras, TArray<FGrantedPandora>& OutPandoras) const;
	void MergeGrantedPandoras(const TArray<FGrantedPandora>& Defaults, const TArray<FGrantedPandora>& Overrides, TArray<FGrantedPandora>& OutPandoras) const;
	int32 GetGrantedDefaultPandoraLevel(UPandoraDefinition* Pandora) const;
	int32 CalculateSpentPandoraPoints() const;
	int32 CalculateResetPandoraPoints() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	bool bInitializeDefaultPandorasOnBeginPlay = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	int32 DefaultPandoraPoints = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!PandoraTree|Defaults")
	TArray<FGrantedPandora> DefaultPandoras;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "!PandoraTree")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_GrantedPandoras, BlueprintReadOnly, Category = "!PandoraTree")
	TArray<FGrantedPandora> GrantedPandoras;

	UPROPERTY(ReplicatedUsing = OnRep_PointsAvailable, BlueprintReadOnly, Category = "!PandoraTree")
	int32 PointsAvailable = 0;

	UPROPERTY(Transient)
	TObjectPtr<APdPlayerState> OwnerPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> OwnerASC;

};
