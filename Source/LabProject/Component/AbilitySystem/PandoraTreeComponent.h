#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "Logging/LogRateLimiter.h"
#include "PandoraTreeComponent.generated.h"

class UDefaultPlayerProvisioner;
class UPandoraComponent;
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

/**
 * 판도라의 경기 중 레벨 투자와 소울 더스트 환불을 관리한다.
 *
 * 소유권과 장착은 PandoraComponent가 담당하며, 해금·비용 판정은 서버와 UI가 공유한다.
 * 리셋은 투자분만 되돌리고 이미 획득한 판도라의 소유권은 유지한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPandoraTreeComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPandoraTreeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//------------------------------------------------------------------------------------------------------------------

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

	// 비용과 최대 레벨을 제외한 해금 판정이다. 보유 판도라는 선행 조건을 다시 요구하지 않는다.
	UFUNCTION(BlueprintPure, Category = "!PandoraTree")
	bool IsPandoraAvailableForInvestment(UPandoraDefinition* Pandora) const;

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

private:
	friend class UDefaultPlayerProvisioner;

	// 경기 시작 지급 상태를 기록해 이후 환불의 기준으로 사용한다.
	void InitializeFromDefaultProvision(const TArray<FGrantedPandora>& InGrantedPandoras, int32 InPointsAvailable);

	UFUNCTION()
	void OnRep_GrantedPandoras();

	UFUNCTION()
	void HandlePandoraInventoryChanged();

	UFUNCTION()
	void OnRep_PointsAvailable();

	bool HasPandoraTreeAuthority() const;
	UPandoraComponent* GetOwnerPandoraComponent() const;
	int32 FindGrantedPandoraIndex(UPandoraDefinition* Pandora) const;
	bool TryGetInvestmentCost(UPandoraDefinition* Pandora, int32 CurrentLevel, int32 TargetLevel, bool bIgnorePointCost, int32& OutCost) const;
	bool ApplyPandoraInvestment(UPandoraDefinition* Pandora, int32 EntryIndex, int32 NewLevel, int32 Cost);
	bool SpendPointOnPandoraInternal(UPandoraDefinition* Pandora);
	bool ResetPandoraInternal();
	void RestoreInitialPandoras(int32 NewPointsAvailable);
	int32 ClampPandoraLevel(const UPandoraDefinition* Pandora, int32 Level) const;
	int64 CalculatePointCostForPandoraLevels(const UPandoraDefinition* Pandora, int32 FirstLevel, int32 LastLevel) const;
	int64 CalculateSpentPandoraPoints() const;
	void LogRejectedServerRequest(const TCHAR* RequestName, const FString& Reason);

	UPROPERTY(Transient)
	TArray<FGrantedPandora> InitialGrantedPandoras;

	UPROPERTY(Transient)
	int32 InitialPointsAvailable = 0;

	UPROPERTY(ReplicatedUsing = OnRep_GrantedPandoras, BlueprintReadOnly, Category = "!PandoraTree", meta = (AllowPrivateAccess = "true"))
	TArray<FGrantedPandora> GrantedPandoras;

	UPROPERTY(ReplicatedUsing = OnRep_PointsAvailable, BlueprintReadOnly, Category = "!PandoraTree", meta = (AllowPrivateAccess = "true"))
	int32 PointsAvailable = 0;

	bool bChangingPandoras = false;

	FLogRateLimiter ServerValidationLogLimiter;
};
