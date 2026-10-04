#include "Misc/AutomationTest.h"
#include "Online/Backend/AwsSigV4.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> BytesOf(const ANSICHAR* Text)
	{
		return TArray<uint8>(reinterpret_cast<const uint8*>(Text), FCStringAnsi::Strlen(Text));
	}

	TArray<uint8> RepeatedBytes(const uint8 Value, const int32 Count)
	{
		TArray<uint8> Bytes;
		Bytes.Init(Value, Count);
		return Bytes;
	}

	FString Sha256Hex(const TArray<uint8>& Bytes)
	{
		return PdAwsSigV4::ToLowerHex(PdAwsSigV4::Sha256(Bytes));
	}

	const FAwsCredentials ExampleCredentials{
		TEXT("AKIDEXAMPLE"), TEXT("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"), FString()};
}

// NIST 벡터와 패딩 경계(55·56·64바이트: 길이 필드가 같은 블록/다음 블록에 들어가는 경우)를 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdSha256Test, "LabProject.Unit.Backend.AwsSigV4.Sha256",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FPdSha256Test::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("empty"), Sha256Hex({}),
		TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	TestEqual(TEXT("abc"), Sha256Hex(BytesOf("abc")),
		TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	TestEqual(TEXT("two blocks"), Sha256Hex(BytesOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
		TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
	TestEqual(TEXT("55 bytes"), Sha256Hex(RepeatedBytes('a', 55)),
		TEXT("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
	TestEqual(TEXT("56 bytes"), Sha256Hex(RepeatedBytes('a', 56)),
		TEXT("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
	TestEqual(TEXT("64 bytes"), Sha256Hex(RepeatedBytes('a', 64)),
		TEXT("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
	TestEqual(TEXT("1000 bytes"), Sha256Hex(RepeatedBytes('a', 1000)),
		TEXT("41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"));
	return true;
}

// RFC 4231 테스트 케이스 2(짧은 키)와 6(블록보다 긴 키)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdHmacSha256Test, "LabProject.Unit.Backend.AwsSigV4.HmacSha256",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FPdHmacSha256Test::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("case 2"),
		PdAwsSigV4::ToLowerHex(PdAwsSigV4::HmacSha256(BytesOf("Jefe"), BytesOf("what do ya want for nothing?"))),
		TEXT("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
	TestEqual(TEXT("case 6"),
		PdAwsSigV4::ToLowerHex(PdAwsSigV4::HmacSha256(
			RepeatedBytes(0xaa, 131), BytesOf("Test Using Larger Than Block-Size Key - Hash Key First"))),
		TEXT("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
	return true;
}

// AWS SigV4 테스트 모음의 get-vanilla 예제와, 실제 결과 보고 형태(POST + 임시 자격 증명)를 확인한다.
// 두 번째 기대값은 get-vanilla를 재현한 독립 구현으로 계산했다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdSigV4SignTest, "LabProject.Unit.Backend.AwsSigV4.Sign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FPdSigV4SignTest::RunTest(const FString& Parameters)
{
	PdAwsSigV4::FRequest Vanilla;
	Vanilla.Method = TEXT("GET");
	Vanilla.Host = TEXT("example.amazonaws.com");
	Vanilla.Path = TEXT("/");
	Vanilla.Region = TEXT("us-east-1");
	Vanilla.Service = TEXT("service");
	Vanilla.TimestampUtc = FDateTime(2015, 8, 30, 12, 36, 0);
	const PdAwsSigV4::FSignature VanillaSignature = PdAwsSigV4::Sign(Vanilla, ExampleCredentials);
	TestEqual(TEXT("get-vanilla authorization"), VanillaSignature.Authorization,
		TEXT("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, ")
		TEXT("SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31"));
	TestEqual(TEXT("get-vanilla date"), VanillaSignature.AmzDate, TEXT("20150830T123600Z"));

	PdAwsSigV4::FRequest Report;
	Report.Method = TEXT("POST");
	Report.Host = TEXT("abc123.execute-api.ap-northeast-2.amazonaws.com");
	Report.Path = TEXT("/server/match-result");
	Report.Payload = BytesOf("{\"matchId\":\"test\"}");
	Report.Region = TEXT("ap-northeast-2");
	Report.Service = TEXT("execute-api");
	Report.TimestampUtc = FDateTime(2026, 9, 30, 12, 0, 0);
	FAwsCredentials TemporaryCredentials = ExampleCredentials;
	TemporaryCredentials.SessionToken = TEXT("token123");
	const PdAwsSigV4::FSignature ReportSignature = PdAwsSigV4::Sign(Report, TemporaryCredentials);
	TestEqual(TEXT("report authorization"), ReportSignature.Authorization,
		TEXT("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20260930/ap-northeast-2/execute-api/aws4_request, ")
		TEXT("SignedHeaders=host;x-amz-date;x-amz-security-token, Signature=b0b799d7d25fa977a3ba388cc025be7e7e48cc05b032ac1b8b946ce0a0414a2c"));
	return true;
}

#endif
