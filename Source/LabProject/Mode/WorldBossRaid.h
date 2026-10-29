#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "WorldBossRaid.generated.h"

class AController;
class AGameModeBase;
class AMonsterCharacter;
class APlayerController;
class APlayerState;
struct FOnAttributeChangeData;

namespace BackendHttp
{
	struct FResponse;
}

/** 한 플레이어의 도전. 서버가 끝나는 시각과 이번 도전에서 넣은 딜을 정해 모두에게 복제한다. */
USTRUCT()
struct FWorldBossAttempt
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> Player;

	/** 서버 월드 시간(초). 이 시각이 지나면 도전이 끝난다. */
	UPROPERTY()
	double EndServerTime = 0.0;

	/** 이번 도전에서 보스 체력을 실제로 깎은 양 */
	UPROPERTY()
	double Damage = 0.0;

	UPROPERTY()
	bool bEnded = false;

	/** 서버 전용. 도전이 끝난 플레이어를 타이틀로 한 번만 보낸다. */
	bool bSentToTitle = false;
};

/**
 * 보스 레이드 월드의 월드 보스. 모든 서버가 하나의 전역 체력을 함께 깎는다.
 *
 * 서버
 * - 보스(AMonsterCharacter::bWorldBoss)가 실제로 잃은 체력을 플레이어별로 모아 백엔드(POST /server/raid-damage)에 보고한다.
 *   응답으로 받은 전역 체력을 이 월드 보스의 체력으로 맞추므로 다른 서버가 깎은 체력도 보인다.
 *   보스의 최대 체력은 전역 최대 체력과 같아서, 페이즈 소환과 사망은 기존 체력 흐름 그대로 전역 체력 비율을 따른다.
 * - 이 월드의 체력이 0이 되면 전역 체력도 0 이하다(다른 서버의 딜을 아직 받지 못했을 뿐 더 깎였을 수는 없다).
 * - 플레이어마다 들어온 순간부터 도전 시간을 재고, 끝나면 남은 딜을 보고한 뒤 타이틀로 보낸다.
 * - 보스가 쓰러지면 쓰러지는 연출을 보여 준 뒤 모두를 타이틀로 보내고 게임 세션을 끝낸다. 보상은 타이틀에서 받는다.
 * - 백엔드 주소나 자격 증명이 없거나 열린 회차가 없으면 보고하지 않는다. 보스는 이 월드의 체력으로 싸우고 쓰러지면 다시 나온다.
 *
 * 클라이언트: HUD가 이 액터의 복제 값으로 긴 보스 체력바와 도전 시간, 이번 도전 딜을 그린다.
 */
UCLASS(NotPlaceable)
class LABPROJECT_API AWorldBossRaid : public AInfo
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	AWorldBossRaid();

	/** 이 월드의 월드 보스 레이드. 보스 레이드로 연 월드에만 있다. */
	static AWorldBossRaid* Find(const UObject* WorldContextObject);

	/** 서버: 월드 보스가 생기면(다시 나오면) 부른다. 알고 있는 전역 체력을 보스에 적용한다. */
	void RegisterBoss(AMonsterCharacter& Boss);

	/** 서버: 보스가 실제로 잃은 체력. 지금 도전 중이 아닌 플레이어의 딜도 전역 체력과 누적 딜에는 들어간다. */
	void RecordBossDamage(const APlayerState* Attacker, float Damage);

	float GetBossHealth() const { return BossHealth; }
	float GetBossMaxHealth() const { return BossMaxHealth; }
	bool IsBossDefeated() const { return bBossDefeated; }
	const FWorldBossAttempt* FindAttempt(const APlayerState* Player) const;
	double GetAttemptSecondsLeft(const FWorldBossAttempt& Attempt) const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandleBossHealthChanged(const FOnAttributeChangeData& Data);
	void HandleBossMaxHealthChanged(const FOnAttributeChangeData& Data);

	UFUNCTION()
	void OnRep_Attempts();
	void HandleReportResponse(const BackendHttp::FResponse& Response);
	void TickRaid();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ShowBarOnLocalHuds();
	void NoteLocalPlayerAttempts() const;
	void BeginAttempt(APlayerState* Player);
	void SendReport();
	void ApplyGlobalHealth(double Health, double MaxHealth, bool bDefeated);
	void ApplyHealthToBoss();
	void SendPlayerToTitle(APlayerState* Player) const;
	double GetServerTime() const;
	double GetUnreportedDamage() const;

	/** 보고하지 않은 딜. 백엔드 ID별로 모으며, ID가 없는 플레이어(로컬 시험)의 딜은 빈 키에 모아 전역 체력에만 반영한다. */
	struct FPendingDamage
	{
		FString DisplayName;
		double Damage = 0.0;
	};

	/** 보내는 중이거나 실패해 다시 보낼 보고. 같은 내용을 같은 순번으로 다시 보내야 백엔드가 중복을 걸러 낸다. */
	struct FReport
	{
		int64 Seq = 0;
		TArray<TPair<FString, FPendingDamage>> Players;
	};

	UPROPERTY(Replicated)
	float BossHealth = 0.0f;

	UPROPERTY(Replicated)
	float BossMaxHealth = 0.0f;

	UPROPERTY(Replicated)
	bool bBossDefeated = false;

	UPROPERTY(ReplicatedUsing = OnRep_Attempts)
	TArray<FWorldBossAttempt> Attempts;

	TWeakObjectPtr<AMonsterCharacter> Boss;
	FDelegateHandle BossHealthChangedHandle;
	FDelegateHandle BossMaxHealthChangedHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FTimerHandle TickTimerHandle;

	TMap<FString, FPendingDamage> PendingDamage;
	TOptional<FReport> UnsentReport;
	bool bReportInFlight = false;
	bool bReportQueued = false;
	/** 백엔드와 연결하지 않고 이 월드의 체력으로만 싸운다. */
	bool bOffline = false;
	bool bGlobalHealthKnown = false;
	FString ServerId;
	FString RaidId;
	int64 LastSeq = 0;
	double GlobalHealth = 0.0;
	double GlobalMaxHealth = 0.0;
	double AttemptSeconds = 0.0;
	double LastReportServerTime = 0.0;
	double DefeatedServerTime = -1.0;
	bool bDefeatTravelStarted = false;
};
