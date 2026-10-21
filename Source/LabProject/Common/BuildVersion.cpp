#include "Common/BuildVersion.h"

#include "GeneralProjectSettings.h"

// Scripts/StampBuildVersion.ps1만 만드는 파일이다. 만들거나 지울 때 스크립트가 이 파일의 시각을 갱신해 다시 컴파일되게 한다.
#if __has_include("Common/BuildVersionStamp.h")
#include "Common/BuildVersionStamp.h"
#endif

#ifndef PD_BUILD_NUMBER
#define PD_BUILD_NUMBER 0
#endif

#ifndef PD_BUILD_COMMIT
#define PD_BUILD_COMMIT ""
#endif

int32 PdBuildVersion::GetBuildNumber()
{
	return PD_BUILD_NUMBER;
}

FString PdBuildVersion::GetCommit()
{
	return TEXT(PD_BUILD_COMMIT);
}

namespace
{
	/** 프로젝트 설정의 버전. 예: "v1.2.0" */
	FString GetVersionText()
	{
		return TEXT("v") + GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	}
}

FString PdBuildVersion::GetDisplayString()
{
	if (GetBuildNumber() <= 0)
	{
		return GetVersionText() + TEXT(" · dev");
	}
#if UE_BUILD_SHIPPING
	// 플레이어에게는 버전만 보여 준다. 배포한 커밋은 같은 이름의 git 태그(v1.2.0)로 찾는다.
	return GetVersionText();
#else
	return FString::Printf(TEXT("%s · %s"), *GetVersionText(), *GetCommit());
#endif
}

FString PdBuildVersion::GetLogString()
{
	if (GetBuildNumber() <= 0)
	{
		return GetVersionText() + TEXT(" · dev");
	}
	return FString::Printf(TEXT("%s · #%d · %s"), *GetVersionText(), GetBuildNumber(), *GetCommit());
}
