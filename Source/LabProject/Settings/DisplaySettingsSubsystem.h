#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DisplaySettingsSubsystem.generated.h"

class UGameUserSettings;

/**
 * 화면 모드·해상도·그래픽 품질·프레임 제한. 엔진의 GameUserSettings에 맡겨 바꾸는 즉시 적용하고 저장하므로,
 * 다음 실행에는 엔진이 시작할 때 같은 설정으로 연다. 전용 서버에는 없다.
 */
UCLASS()
class LABPROJECT_API UDisplaySettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 고를 수 있는 화면 모드(전체 화면, 창 전체 화면, 창 모드) 순서. */
	static TConstArrayView<EWindowMode::Type> GetWindowModes();

	/** 고를 수 있는 프레임 제한. 0은 제한 없음이다. */
	static TConstArrayView<int32> GetFrameRateLimits();

	/** 그래픽 품질 단계 수(낮음·보통·높음·최고 = 엔진 확장성 0~3). */
	static constexpr int32 QualityLevelCount = 4;

	/** 처음 실행했거나 아직 프레임 제한을 고른 적 없는 사람에게 한 번 맞춰 주는 값. */
	static constexpr int32 DefaultFrameRateLimit = 120;

	EWindowMode::Type GetWindowMode() const;
	void SetWindowMode(EWindowMode::Type WindowMode);

	/** 지금 화면 모드에서 고를 수 있는 해상도(작은 것부터). 창 전체 화면은 바탕화면 크기 하나뿐이다. */
	TArray<FIntPoint> GetResolutionOptions() const;
	FIntPoint GetResolution() const;
	bool CanChooseResolution() const;
	void SetResolution(FIntPoint Resolution);

	/** 0~3. 항목마다 단계가 다르면 가장 낮은 항목의 단계다. */
	int32 GetQualityLevel() const;
	void SetQualityLevel(int32 Level);

	int32 GetFrameRateLimit() const;
	void SetFrameRateLimit(int32 FrameRateLimit);

	/** 이 PC에서 기본 프레임 제한을 한 번 맞춘 적이 있다(그 뒤의 무제한 선택은 그대로 둔다). */
	static bool HasAppliedDefaultFrameRateLimit();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static UGameUserSettings& Settings();
	static FIntPoint DefaultWindowSize();
	void ApplyDefaultFrameRateLimitOnce();
	void ApplyResolution();
	void ApplyNonResolution();
};
