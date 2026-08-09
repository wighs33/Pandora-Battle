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

	/** Red : 0, Blue : 1, ... */
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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerMatchIdentityChanged, const FPlayerMatchIdentity& /*NewMatchIdentity*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerDeathCountChanged, int32 /*NewDeathCount*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerMapRegionChanged, EPlayerMapRegion /*NewMapRegion*/);

UCLASS(BlueprintType, ClassGroup=(Player))
class LABPROJECT_API UPlayerMatchComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerMatchComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FOnMatchDisplayNameChanged OnMatchDisplayNameChanged;
	FOnMatchTeamColorChanged OnMatchTeamColorChanged;
	FOnPlayerMatchIdentityChanged OnPlayerMatchIdentityChanged;
	FOnPlayerDeathCountChanged OnPlayerDeathCountChanged;
	FOnPlayerMapRegionChanged OnPlayerMapRegionChanged;

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

	void SetInitialSpawnTransform(const FTransform& InSpawnTransform);
	void ClearInitialSpawnTransform();

	UFUNCTION(BlueprintPure, Category = "!Match|Spawn")
	bool HasInitialSpawnTransform() const { return bHasInitialSpawnTransform; }

	UFUNCTION(BlueprintPure, Category = "!Match|Spawn")
	bool TryGetInitialSpawnTransform(FTransform& OutSpawnTransform) const;

	void SetPlayerMapRegion(EPlayerMapRegion InMapRegion);

	UFUNCTION(BlueprintPure, Category = "!Match|Map")
	EPlayerMapRegion GetPlayerMapRegion() const { return PlayerMapRegion; }

	void CopyMatchStateTo(
		UPlayerMatchComponent* TargetComponent,
		const FPlayerMatchIdentity& MatchIdentityToCopy,
		bool bCopyMatchStats) const;

private:
	bool HasAuthority() const;
	void InitializeDefaultMatchDisplayNameIfNeeded();
	void SetDeathCount(int32 InDeathCount);
	void BroadcastPlayerMatchIdentityChanged(const FPlayerMatchIdentity* PreviousIdentity = nullptr);

	UFUNCTION()
	void OnRep_PlayerMatchIdentity(const FPlayerMatchIdentity& PreviousIdentity);

	UFUNCTION()
	void OnRep_DeathCount(int32 PreviousDeathCount);

	UFUNCTION()
	void OnRep_PlayerMapRegion(EPlayerMapRegion PreviousPlayerMapRegion);

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_PlayerMatchIdentity, Category = "!Match|Identity")
	FPlayerMatchIdentity PlayerMatchIdentity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_DeathCount, Category = "!Match|Stats", meta = (AllowPrivateAccess = "true"))
	int32 DeathCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Match|Spawn", meta = (AllowPrivateAccess = "true"))
	bool bHasInitialSpawnTransform = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Match|Spawn", meta = (AllowPrivateAccess = "true"))
	FTransform InitialSpawnTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_PlayerMapRegion, Category = "!Match|Map", meta = (AllowPrivateAccess = "true"))
	EPlayerMapRegion PlayerMapRegion = EPlayerMapRegion::Dome;
};
