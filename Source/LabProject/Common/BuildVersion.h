#pragma once

#include "CoreMinimal.h"

/**
 * 이 실행 파일이 어느 빌드인지 알려 준다.
 *
 * 배포용 패키지를 만들기 전에 Scripts/StampBuildVersion.ps1이 빌드 번호와 커밋 해시를 BuildVersionStamp.h에 쓴다.
 * 그 파일이 없으면(개발자 PC의 일반 빌드) 개발 빌드로 표시한다. 스탬프 파일은 git에서 추적하지 않는다.
 */
namespace PdBuildVersion
{
	/** 빌드 번호(그 커밋까지의 커밋 수). 개발 빌드는 0. */
	LABPROJECT_API int32 GetBuildNumber();

	/** 짧은 커밋 해시. 개발 빌드는 빈 문자열. */
	LABPROJECT_API FString GetCommit();

	/** 타이틀 화면에 쓰는 버전. 예: "v1.2.0 · 3f9c2ab", Shipping은 버전만 "v1.2.0", 개발 빌드는 "v1.2.0 · dev". */
	LABPROJECT_API FString GetDisplayString();

	/** 시작 로그에 쓰는 한 줄. 빌드 번호까지 넣는다. 예: "v1.2.0 · #57 · 3f9c2ab", 개발 빌드는 "v1.2.0 · dev". */
	LABPROJECT_API FString GetLogString();
}
