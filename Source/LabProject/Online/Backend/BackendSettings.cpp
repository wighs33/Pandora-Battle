#include "Online/Backend/BackendSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BackendSettings)

FString UBackendSettings::GetApiBaseUrl() const
{
	FString Url = ApiBaseUrl;
	FParse::Value(FCommandLine::Get(), TEXT("BackendUrl="), Url);
	Url.TrimStartAndEndInline();
	while (Url.EndsWith(TEXT("/")))
	{
		Url.LeftChopInline(1);
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
