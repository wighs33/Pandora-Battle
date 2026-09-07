#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "Map/PdMapTypes.h"
#include "PlayerMatchComponent.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerMatchIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "!Match|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "!Match|Identity")
	int32 SpawnIndex = INDEX_NONE;

	/** 팀 색상 인덱스: 빨강 0, 파랑 1, ... */
	UPROPERTY(EditAnywhere, Category = "!Match|Identity")
	int32 TeamColorIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "!Match|Identity")
	FName SelectedAchievementId;

	bool Matches(const FPlayerMatchIdentity& Other) const
	{
		return DisplayName.EqualTo(Other.DisplayName)
			&& SpawnIndex == Other.SpawnIndex
			&& TeamColorIndex == Other.TeamColorIndex
			&& SelectedAchievementId == Other.SelectedAchievementId;
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMatchDisplayNameChanged, const FText& /*NewDisplayName*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMatchTeamColorChanged, int32 /*NewTeamColorIndex*/);

/**
 * 플레이어의 식별 정보와 현재 경기 상태를 저장하고 복제한다.
 *
 * 이름과 팀의 변경을 알리며, 새 경기에서는 식별 정보를 유지하고 경기 기록만 초기화한다.
 * 닉네임 결정과 스폰 위치 관리는 각 입장 처리와 스폰 컴포넌트에서 담당한다.
 */
UCLASS(BlueprintType, ClassGroup=(Player))
class LABPROJECT_API UPlayerMatchComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerMatchComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//------------------------------------------------------------------------------------------------------------------
	FOnMatchDisplayNameChanged OnMatchDisplayNameChanged;
	FOnMatchTeamColorChanged OnMatchTeamColorChanged;

	void SetPlayerMatchIdentity(const FPlayerMatchIdentity& InMatchIdentity);
	const FPlayerMatchIdentity& GetPlayerMatchIdentity() const { return PlayerMatchIdentity; }

	void SetMatchDisplayName(const FText& InDisplayName);

	UFUNCTION(BlueprintPure, Category = "!Match|Identity")
	FText GetMatchDisplayName() const { return PlayerMatchIdentity.DisplayName; }

	void SetMatchSpawnIndex(int32 InSpawnIndex);

	UFUNCTION(BlueprintPure, Category = "!Match|Identity")
	int32 GetMatchSpawnIndex() const { return PlayerMatchIdentity.SpawnIndex; }

	void SetMatchTeamColorIndex(int32 InTeamColorIndex);

	UFUNCTION(BlueprintPure, Category = "!Match|Identity")
	int32 GetMatchTeamColorIndex() const { return PlayerMatchIdentity.TeamColorIndex; }

	void SetSelectedAchievementId(FName InAchievementId);

	UFUNCTION(BlueprintPure, Category = "!Match|Identity")
	FName GetSelectedAchievementId() const { return PlayerMatchIdentity.SelectedAchievementId; }

	UFUNCTION(BlueprintPure, Category = "!Match|Stats")
	int32 GetKillCount() const;

	bool RecordDeath(int32 Amount = 1);

	UFUNCTION(BlueprintPure, Category = "!Match|Stats")
	int32 GetDeathCount() const { return DeathCount; }

	void SetPlayerMapRegion(EPlayerMapRegion InMapRegion);

	UFUNCTION(BlueprintPure, Category = "!Match|Map")
	EPlayerMapRegion GetPlayerMapRegion() const { return PlayerMapRegion; }

	/** 리스폰이 아닌 새 경기 입장에서 호출하며, 식별 정보는 유지한다. */
	void ResetForNewMatch(EPlayerMapRegion InitialMapRegion);

private:
	void SetDeathCount(int32 InDeathCount);
	void BroadcastPlayerMatchIdentityChanged(const FPlayerMatchIdentity& PreviousIdentity);

	UFUNCTION()
	void OnRep_PlayerMatchIdentity(const FPlayerMatchIdentity& PreviousIdentity);

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_PlayerMatchIdentity, Category = "!Match|Identity")
	FPlayerMatchIdentity PlayerMatchIdentity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!Match|Stats", meta = (AllowPrivateAccess = "true"))
	int32 DeathCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!Match|Map", meta = (AllowPrivateAccess = "true"))
	EPlayerMapRegion PlayerMapRegion = EPlayerMapRegion::Dome;
};
