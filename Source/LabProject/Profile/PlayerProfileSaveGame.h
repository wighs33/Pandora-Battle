#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Profile/PlayerProfileSaveData.h"
#include "PlayerProfileSaveGame.generated.h"

namespace PlayerProfileDataVersion
{
	inline constexpr int32 Current = 2;
	inline constexpr int32 MaxMatchRecordCount = 5;
}

namespace PlayerProfileStorageVersion
{
	inline constexpr int32 Current = 2;
}

// 기존 .sav의 클래스 경로를 유지하는 로컬 프로필 저장 데이터다.
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPdSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Save")
	int32 ProfileDataVersion = PlayerProfileDataVersion::Current;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Save")
	FGuid SaveId;

	// primary/backup/shutdown 중 가장 최근의 유효한 저장을 선택하는 증가 번호다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Save")
	int64 SaveRevision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Currency", meta = (ClampMin = "0", UIMin = "0"))
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement", meta = (ClampMin = "0", UIMin = "0"))
	int32 MatchPlayedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record", meta = (ClampMin = "0", UIMin = "0"))
	int32 WinCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement", meta = (ClampMin = "0", UIMin = "0"))
	int32 TotalKillCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement", meta = (ClampMin = "0", UIMin = "0"))
	int32 TotalDeathCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement", meta = (ClampMin = "0", UIMin = "0"))
	int32 TotalRewardGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement", meta = (ClampMin = "0", UIMin = "0"))
	int32 ItemCollectedCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Achievement")
	FName SelectedAchievementId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FPlayerPandoraData PlayerPandoraData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	FPlayerSkinData PlayerSkinData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record")
	TArray<FMatchRecord> MatchRecords;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsCurrentFormat() const;
};

/**
 * 프로필 직렬화 데이터에 CRC와 저장 헤더를 붙이는 디스크 저장 형식이다.
 * 가벼운 난독화만 제공하며 보안을 보장하지 않는다.
 */
UCLASS()
class LABPROJECT_API UProfileSaveEnvelope : public USaveGame
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	static UProfileSaveEnvelope* CreateFromProfile(UPdSaveGame* Profile, UObject* Outer = nullptr);

	UPdSaveGame* DecodeProfile() const;

private:
	// 본문을 풀기 전에 머리글(저장 형식, 난독화 값, 저장 ID, 본문 크기)이 맞는지 본다. 맞으면 빈 문자열이다.
	FString FindHeaderProblem() const;

private:
	UPROPERTY()
	int32 StorageFormatVersion = PlayerProfileStorageVersion::Current;

	UPROPERTY()
	uint32 ObfuscationNonce = 0;

	UPROPERTY()
	uint32 PlainPayloadCrc = 0;

	UPROPERTY()
	FGuid ProfileSaveId;

	UPROPERTY()
	int64 ProfileRevision = 0;

	UPROPERTY()
	TArray<uint8> ObfuscatedPayload;
};
