#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "PlayerProfileSaveData.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FMatchRecord
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

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerPandoraData
{
	GENERATED_BODY()

	// 소유 Pandora와 해금 레벨을 Primary Asset ID로 저장한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora")
	TMap<FPrimaryAssetId, int32> GrantedPandorasById;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPlayerSkinData
{
	GENERATED_BODY()

	// 기존 저장 형식을 유지한다. 현재 값 1은 스킨 소유를 의미한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Skin")
	TMap<FPrimaryAssetId, int32> GrantedSkinsById;
};
