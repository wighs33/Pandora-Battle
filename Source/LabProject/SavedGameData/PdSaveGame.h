#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SavedGameData/PlayerPandoraData.h"
#include "SavedGameData/PlayerSkinData.h"
#include "PdSaveGame.generated.h"

namespace PdSaveGameSchema
{
	inline constexpr int32 LegacyAssetNames = 0;
	inline constexpr int32 PrimaryAssetIds = 1;
	inline constexpr int32 Current = PrimaryAssetIds;
}

namespace PdProfileSaveStorage
{
	inline constexpr int32 ObfuscatedPayload = 1;
	inline constexpr int32 Current = ObfuscatedPayload;
}

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdMatchRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record")
	bool bWin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record", meta = (ClampMin = "0", UIMin = "0"))
	int32 KillCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record", meta = (ClampMin = "0", UIMin = "0"))
	int32 DeathCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record", meta = (ClampMin = "0", UIMin = "0"))
	int32 Reward = 0;
};

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPdSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Missing in legacy saves, so zero reliably identifies the name-based schema.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Save")
	int32 SaveSchemaVersion = PdSaveGameSchema::LegacyAssetNames;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Save")
	FGuid SaveId;

	// Monotonic revision used to select the newest valid primary/backup/shutdown snapshot.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	FPlayerPandoraData PlayerPandoraData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	FPlayerSkinData PlayerSkinData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Record")
	TArray<FPdMatchRecord> MatchRecords;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Save")
	TArray<FString> ImportedLegacySaveSlots;
};

/**
 * Disk-only wrapper that keeps the runtime profile object and all gameplay APIs unchanged.
 * This is intentionally lightweight obfuscation, not a security boundary.
 */
UCLASS()
class LABPROJECT_API UPdProfileSaveEnvelope : public USaveGame
{
	GENERATED_BODY()

public:
	static UPdProfileSaveEnvelope* CreateFromProfile(
		UPdSaveGame* Profile,
		const FString& PlayerId,
		UObject* Outer = nullptr);

	UPdSaveGame* DecodeProfile(const FString& PlayerId) const;

	int32 GetStorageFormatVersion() const { return StorageFormatVersion; }
	int32 GetObfuscatedPayloadSize() const { return ObfuscatedPayload.Num(); }

#if WITH_DEV_AUTOMATION_TESTS
	void CorruptPayloadForTest();
#endif

private:
	UPROPERTY()
	int32 StorageFormatVersion = PdProfileSaveStorage::Current;

	UPROPERTY()
	uint32 ObfuscationNonce = 0;

	UPROPERTY()
	uint32 PlainPayloadCrc = 0;

	UPROPERTY()
	TArray<uint8> ObfuscatedPayload;
};
