#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

/** AWS 자격 증명. SessionToken은 임시 자격 증명(STS·플릿 역할)일 때만 있다. */
struct FAwsCredentials
{
	FString AccessKeyId;
	FString SecretAccessKey;
	FString SessionToken;

	bool IsValid() const { return !AccessKeyId.IsEmpty() && !SecretAccessKey.IsEmpty(); }
};

/**
 * AWS Signature Version 4 서명.
 *
 * 전용 서버가 API Gateway의 IAM 인증 경로(execute-api)에 결과를 보고할 때 사용한다.
 * 서명만 필요해 AWS C++ SDK를 넣지 않고 SHA-256·HMAC을 직접 구현했으며, 자동화 테스트가
 * NIST·RFC 4231 벡터와 AWS 서명 예제로 검증한다(Tests/AwsSigV4Tests.cpp).
 */
namespace PdAwsSigV4
{
	using FDigest = TStaticArray<uint8, 32>;

	LABPROJECT_API FDigest Sha256(TConstArrayView<uint8> Data);
	LABPROJECT_API FDigest HmacSha256(TConstArrayView<uint8> Key, TConstArrayView<uint8> Message);
	LABPROJECT_API FString ToLowerHex(TConstArrayView<uint8> Bytes);
	LABPROJECT_API FString ToLowerHex(const FDigest& Digest);

	struct FRequest
	{
		FString Method;
		/** 소문자 호스트. 기본 포트가 아니면 "host:port" 형태로 넣는다. */
		FString Host;
		/** URL에 실제로 보내는 경로. 비어 있으면 "/"로 서명한다. */
		FString Path;
		/** '?' 뒤 문자열. 호출자가 RFC 3986으로 인코딩한 "key=value" 쌍이어야 한다. */
		FString Query;
		TArray<uint8> Payload;
		FString Region;
		FString Service;
		FDateTime TimestampUtc;
	};

	struct FSignature
	{
		FString Authorization;
		FString AmzDate;
		/** 서명 오류를 진단할 때 AWS 응답의 기대값과 비교한다. */
		FString CanonicalRequest;
		FString StringToSign;
	};

	/** 서명 대상 헤더는 host, x-amz-date, (임시 자격 증명이면) x-amz-security-token이다. */
	LABPROJECT_API FSignature Sign(const FRequest& Request, const FAwsCredentials& Credentials);
}
