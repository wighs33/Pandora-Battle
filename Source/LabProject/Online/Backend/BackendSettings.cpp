#include "Online/Backend/BackendSettings.h"

#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BackendSettings)

DEFINE_LOG_CATEGORY_STATIC(LogBackendSettings, Log, All);

namespace
{
	/** API 서버(Backend/server)의 .env와 같은 이름이다. */
	constexpr const TCHAR* ServerKeyEnvironmentVariable = TEXT("LABPROJECT_SERVER_KEY");
}

FString UBackendSettings::GetApiBaseUrl() const
{
	FString Url = ApiBaseUrl;
	FParse::Value(FCommandLine::Get(), TEXT("BackendUrl="), Url);
	Url.TrimStartAndEndInline();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
	}

	// ini에서 따옴표 없이 쓴 "https://..."는 '//'부터 주석으로 잘려 "https:"만 남는다.
	const int32 SchemeEnd = Url.Find(TEXT("://"));
	if (!Url.IsEmpty() && (SchemeEnd == INDEX_NONE || SchemeEnd + 3 >= Url.Len()))
	{
		UE_LOG(LogBackendSettings, Error,
			TEXT("Backend ApiBaseUrl '%s' is not a full URL. Quote it in DefaultGame.ini: ApiBaseUrl=\"https://...\""), *Url);
		return FString();
	}
	return Url;
}

FString UBackendSettings::BuildApiUrl(const TCHAR* Path) const
{
	const FString BaseUrl = GetApiBaseUrl();
	return BaseUrl.IsEmpty() ? FString() : BaseUrl + Path;
}

FString UBackendSettings::GetServerKey() const
{
	return FPlatformMisc::GetEnvironmentVariable(ServerKeyEnvironmentVariable);
}
