#include "Online/Backend/BackendSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BackendSettings)

DEFINE_LOG_CATEGORY_STATIC(LogBackendSettings, Log, All);

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

FString UBackendSettings::GetGameServerRoleArn() const
{
	FString RoleArn = GameServerRoleArn;
	FParse::Value(FCommandLine::Get(), TEXT("FleetRoleArn="), RoleArn);
	return RoleArn;
}
