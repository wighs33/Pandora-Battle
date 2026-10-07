#include "Common/BuildVersion.h"

#include "GeneralProjectSettings.h"

// CI만 만드는 파일이다. 만들거나 지울 때 스크립트가 이 파일의 시각을 갱신해 다시 컴파일되게 한다.
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

FString PdBuildVersion::GetDisplayString()
{
	const FString& ProjectVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	const FString Prefix = ProjectVersion.IsEmpty() ? FString() : TEXT("v") + ProjectVersion + TEXT(" · ");
	if (GetBuildNumber() <= 0)
	{
		return Prefix + TEXT("dev");
	}
	return FString::Printf(TEXT("%s#%d · %s"), *Prefix, GetBuildNumber(), *GetCommit());
}
