#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AudioSettingsSaveGame.generated.h"

UCLASS()
class LABPROJECT_API UAudioSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bHasMasterVolumeSetting = false;

	UPROPERTY()
	int32 MasterVolumePercent = 0;

	UPROPERTY()
	int32 LastAudibleMasterVolumePercent = 0;

	// BGM 전용 구버전 파일을 읽는 이관 필드. 새 저장에서는 Master 설정만 사용한다.
	UPROPERTY()
	bool bHasBgmVolumeSetting = false;

	UPROPERTY()
	int32 BgmVolumePercent = 0;

	UPROPERTY()
	int32 LastAudibleBgmVolumePercent = 0;
};
