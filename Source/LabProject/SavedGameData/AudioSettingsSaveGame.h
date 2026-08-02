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

	// Legacy fields kept so an AudioSettings save created by the previous BGM-only
	// implementation can be migrated to the master-volume setting.
	UPROPERTY()
	bool bHasBgmVolumeSetting = false;

	UPROPERTY()
	int32 BgmVolumePercent = 0;

	UPROPERTY()
	int32 LastAudibleBgmVolumePercent = 0;
};
