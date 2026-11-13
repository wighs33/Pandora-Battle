#include "Settings/DisplaySettingsSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const EWindowMode::Type WindowModes[] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
	const int32 FrameRateLimits[] = {30, 60, 120, 144, 0};

	// 1.0.3까지는 프레임 제한을 고를 수 없어 모두 무제한(0)으로 저장되어 있다. 기본값을 한 번 맞췄다는 표시를 같은 파일에 남겨,
	// 그 뒤에 무제한을 고른 사람의 선택은 건드리지 않는다.
	const TCHAR* DisplayConfigSection = TEXT("/Script/LabProject.DisplaySettings");
	const TCHAR* DefaultFrameRateLimitAppliedKey = TEXT("bDefaultFrameRateLimitApplied");

	bool IsSmallerThan(const FIntPoint& Size, const FIntPoint& Bound)
	{
		return Size.X < Bound.X && Size.Y < Bound.Y;
	}
}

bool UDisplaySettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UDisplaySettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ApplyDefaultFrameRateLimitOnce();
}

TConstArrayView<EWindowMode::Type> UDisplaySettingsSubsystem::GetWindowModes()
{
	return WindowModes;
}

TConstArrayView<int32> UDisplaySettingsSubsystem::GetFrameRateLimits()
{
	return FrameRateLimits;
}

EWindowMode::Type UDisplaySettingsSubsystem::GetWindowMode() const
{
	return Settings().GetFullscreenMode();
}

void UDisplaySettingsSubsystem::SetWindowMode(const EWindowMode::Type WindowMode)
{
	UGameUserSettings& UserSettings = Settings();
	UserSettings.SetFullscreenMode(WindowMode);
	// 창 전체 화면은 늘 바탕화면 크기다. 전체 화면도 바탕화면 크기에서 시작하고, 창 모드는 창 테두리가 화면 안에 들어오는 크기로 연다.
	const FIntPoint Desktop = UserSettings.GetDesktopResolution();
	if (WindowMode != EWindowMode::Windowed)
	{
		UserSettings.SetScreenResolution(Desktop);
	}
	else if (!IsSmallerThan(UserSettings.GetScreenResolution(), Desktop))
	{
		UserSettings.SetScreenResolution(DefaultWindowSize());
	}
	ApplyResolution();
}

TArray<FIntPoint> UDisplaySettingsSubsystem::GetResolutionOptions() const
{
	TArray<FIntPoint> Available;
	if (GetWindowMode() == EWindowMode::Windowed)
	{
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Available);
	}
	else if (GetWindowMode() == EWindowMode::Fullscreen)
	{
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Available);
	}
	// 전체 화면 목록은 주사율마다 같은 크기가 거듭 나온다. 지금 크기는 목록에 없어도 넣어 콤보가 비어 보이지 않게 한다.
	TArray<FIntPoint> Options;
	for (const FIntPoint& Resolution : Available)
	{
		Options.AddUnique(Resolution);
	}
	Options.AddUnique(GetResolution());
	Options.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	return Options;
}

FIntPoint UDisplaySettingsSubsystem::GetResolution() const
{
	// 창 전체 화면 창은 모니터를 덮으므로 저장된 해상도와 상관없이 바탕화면 크기로 그린다.
	const UGameUserSettings& UserSettings = Settings();
	return GetWindowMode() == EWindowMode::WindowedFullscreen ? UserSettings.GetDesktopResolution() : UserSettings.GetScreenResolution();
}

bool UDisplaySettingsSubsystem::CanChooseResolution() const
{
	return GetWindowMode() != EWindowMode::WindowedFullscreen;
}

void UDisplaySettingsSubsystem::SetResolution(const FIntPoint Resolution)
{
	if (CanChooseResolution())
	{
		Settings().SetScreenResolution(Resolution);
		ApplyResolution();
	}
}

int32 UDisplaySettingsSubsystem::GetQualityLevel() const
{
	// 항목마다 단계가 다르면(엔진 기본값에 '시네마틱' 항목이 섞이거나 콘솔로 바꾼 경우) 가장 낮은 항목의 단계로 보여 준다.
	// 그 단계를 고르면 모든 항목이 그 단계로 맞춰진다.
	const UGameUserSettings& UserSettings = Settings();
	const int32 Level = UserSettings.GetOverallScalabilityLevel();
	return FMath::Clamp(Level >= 0 ? Level : UserSettings.ScalabilityQuality.GetMinQualityLevel(), 0, QualityLevelCount - 1);
}

void UDisplaySettingsSubsystem::SetQualityLevel(const int32 Level)
{
	Settings().SetOverallScalabilityLevel(FMath::Clamp(Level, 0, QualityLevelCount - 1));
	ApplyNonResolution();
}

int32 UDisplaySettingsSubsystem::GetFrameRateLimit() const
{
	return FMath::RoundToInt(Settings().GetFrameRateLimit());
}

void UDisplaySettingsSubsystem::SetFrameRateLimit(const int32 FrameRateLimit)
{
	Settings().SetFrameRateLimit(static_cast<float>(FMath::Max(0, FrameRateLimit)));
	ApplyNonResolution();
}

UGameUserSettings& UDisplaySettingsSubsystem::Settings()
{
	return *UGameUserSettings::GetGameUserSettings();
}

FIntPoint UDisplaySettingsSubsystem::DefaultWindowSize()
{
	// 바탕화면보다 작은 편한 창 크기 중 가장 큰 것. 목록이 비면(드문 해상도) 바탕화면의 3/4.
	const FIntPoint Desktop = Settings().GetDesktopResolution();
	TArray<FIntPoint> Sizes;
	UKismetSystemLibrary::GetConvenientWindowedResolutions(Sizes);
	FIntPoint Best = FIntPoint(Desktop.X * 3 / 4, Desktop.Y * 3 / 4);
	for (const FIntPoint& Size : Sizes)
	{
		if (IsSmallerThan(Size, Desktop) && Size.X * Size.Y > Best.X * Best.Y)
		{
			Best = Size;
		}
	}
	return Best;
}

bool UDisplaySettingsSubsystem::HasAppliedDefaultFrameRateLimit()
{
	bool bApplied = false;
	return GConfig->GetBool(DisplayConfigSection, DefaultFrameRateLimitAppliedKey, bApplied, GGameUserSettingsIni) && bApplied;
}

void UDisplaySettingsSubsystem::ApplyDefaultFrameRateLimitOnce()
{
	// 에디터는 에디터용 설정 파일을 쓰고, 프레임 제한이 에디터 창에도 걸리므로 건드리지 않는다.
	if (GIsEditor || HasAppliedDefaultFrameRateLimit())
	{
		return;
	}
	if (GetFrameRateLimit() <= 0)
	{
		SetFrameRateLimit(DefaultFrameRateLimit);
	}
	GConfig->SetBool(DisplayConfigSection, DefaultFrameRateLimitAppliedKey, true, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UDisplaySettingsSubsystem::ApplyResolution()
{
	UGameUserSettings& UserSettings = Settings();
	UserSettings.ApplyResolutionSettings(false);
	UserSettings.ConfirmVideoMode();
	UserSettings.SaveSettings();
}

void UDisplaySettingsSubsystem::ApplyNonResolution()
{
	UGameUserSettings& UserSettings = Settings();
	UserSettings.ApplyNonResolutionSettings();
	UserSettings.SaveSettings();
}
