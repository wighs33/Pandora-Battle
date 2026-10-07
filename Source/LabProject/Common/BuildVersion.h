#pragma once

#include "CoreMinimal.h"

/**
 * 이 실행 파일이 어느 빌드인지 알려 준다.
 *
 * CI가 빌드 전에 Scripts/StampBuildVersion.ps1로 빌드 번호와 커밋 해시를 BuildVersionStamp.h에 쓴다.
 * 그 파일이 없으면(개발자 PC의 일반 빌드) 개발 빌드로 표시한다. 스탬프 파일은 git에서 추적하지 않는다.
 */
namespace PdBuildVersion
{
	/** CI 실행 번호. 개발 빌드는 0. */
	LABPROJECT_API int32 GetBuildNumber();

	/** 짧은 커밋 해시. 개발 빌드는 빈 문자열. */
	LABPROJECT_API FString GetCommit();

	/** 화면과 로그에 쓰는 한 줄. 예: "v1.2.0 · #57 · 3f9c2ab", 개발 빌드는 "v1.2.0 · dev". */
	LABPROJECT_API FString GetDisplayString();
}
