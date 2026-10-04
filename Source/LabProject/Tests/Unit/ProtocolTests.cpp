#include "Luna/LunaChatProtocol.h"
#include "Misc/AutomationTest.h"
#include "Online/Backend/AwsSigV4.h"

#if WITH_DEV_AUTOMATION_TESTS

// 외부 서비스와 주고받는 형식의 단위 테스트: 전용 서버가 경기 결과를 보고할 때 쓰는 AWS SigV4 서명과
// Luna 채팅의 Ollama 스트리밍 응답 해석. 실제 서비스 없이는 연결 테스트로 볼 수 없어 공개 벡터와 바이트 단위 입력으로 본다.

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

	TArray<uint8> Utf8Bytes(const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	FString Sha256Hex(const TArray<uint8>& Bytes)
	{
		return PdAwsSigV4::ToLowerHex(PdAwsSigV4::Sha256(Bytes));
	}

	FString LunaChunkLine(const FString& Content, const bool bDone)
	{
		return FString::Printf(TEXT("{\"model\":\"test\",\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},\"done\":%s}\n"),
			*Content, bDone ? TEXT("true") : TEXT("false"));
	}

	const FAwsCredentials ExampleCredentials{
		TEXT("AKIDEXAMPLE"), TEXT("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY"), FString()};
}

// 직접 구현한 SHA-256의 패딩 경계와, AWS SigV4 테스트 모음의 get-vanilla 예제·실제 결과 보고 형태(POST + 임시 자격 증명)를 확인한다.
// HMAC은 서명 키를 만들 때 쓰이므로 두 서명 값으로 함께 검증된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdAwsSigV4Test, "LabProject.Unit.Protocol.AwsSigV4",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdAwsSigV4Test::RunTest(const FString& Parameters)
{
	// 55·56·64바이트는 길이 필드가 같은 블록에 들어가는지 다음 블록으로 넘어가는지의 경계이고, 1000바이트는 여러 블록이다.
	TestEqual(TEXT("sha256 55 bytes"), Sha256Hex(RepeatedBytes('a', 55)),
		TEXT("9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"));
	TestEqual(TEXT("sha256 56 bytes"), Sha256Hex(RepeatedBytes('a', 56)),
		TEXT("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
	TestEqual(TEXT("sha256 64 bytes"), Sha256Hex(RepeatedBytes('a', 64)),
		TEXT("ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"));
	TestEqual(TEXT("sha256 1000 bytes"), Sha256Hex(RepeatedBytes('a', 1000)),
		TEXT("41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"));

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

	// 기대값은 get-vanilla를 재현한 독립 구현으로 계산했다.
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

// 네트워크 조각이 줄과 한글(UTF-8 3바이트) 중간에서 끊겨도 완성된 줄만 순서대로 해석하고,
// 말풍선에는 생각 블록을 뺀 답만 보이는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaStreamTest, "LabProject.Unit.Protocol.LunaStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdLunaStreamTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Stream = Utf8Bytes(LunaChunkLine(TEXT("안녕하세요, "), false) + LunaChunkLine(TEXT("루나예요."), false)
		+ LunaChunkLine(TEXT(""), true));

	LunaChat::FStreamParser Parser;
	FString Content;
	bool bDone = false;
	FString Error;
	// 1바이트씩 넣어 모든 경계에서 끊기는 경우를 만든다.
	for (const uint8 Byte : Stream)
	{
		Parser.Append(MakeArrayView(&Byte, 1));
		Parser.Consume(Content, bDone, Error);
	}
	TestEqual(TEXT("content"), Content, FString(TEXT("안녕하세요, 루나예요.")));
	TestTrue(TEXT("done"), bDone);
	TestTrue(TEXT("no error"), Error.IsEmpty());

	// 줄바꿈 없이 끝난 마지막 줄은 Flush에서 해석한다.
	LunaChat::FStreamParser TailParser;
	FString TailContent;
	bool bTailDone = false;
	FString TailError;
	FString LastLine = LunaChunkLine(TEXT("끝"), true);
	LastLine.RemoveFromEnd(TEXT("\n"));
	TailParser.Append(Utf8Bytes(LastLine));
	TailParser.Consume(TailContent, bTailDone, TailError);
	TestTrue(TEXT("unterminated line waits"), TailContent.IsEmpty() && !bTailDone);
	TailParser.Flush(TailContent, bTailDone, TailError);
	TestEqual(TEXT("flushed content"), TailContent, FString(TEXT("끝")));
	TestTrue(TEXT("flushed done"), bTailDone);

	// 모델 이름이 틀렸을 때처럼 서버가 오류 줄을 보내면 오류로 전달한다.
	LunaChat::FStreamParser ErrorParser;
	FString ErrorContent;
	bool bErrorDone = false;
	FString ServerError;
	ErrorParser.Append(Utf8Bytes(TEXT("{\"error\":\"model 'missing' not found\"}\n")));
	ErrorParser.Consume(ErrorContent, bErrorDone, ServerError);
	TestEqual(TEXT("server error"), ServerError, FString(TEXT("model 'missing' not found")));

	// 말풍선 문자열은 Luna 채팅이 그리는 것과 같은 순서(생각 블록 제거 → 말풍선 정리)로 만든다.
	const auto BubbleText = [](const FString& Reply) { return LunaChat::ToBubbleText(LunaChat::StripThinking(Reply)); };
	TestEqual(TEXT("think block and markdown removed"), BubbleText(TEXT("<think>plan</think>  **방 목록**을 열어보세요.\r\n\n\n좋아요!  ")),
		FString(TEXT("방 목록을 열어보세요.\n좋아요!")));
	TestEqual(TEXT("open think block is hidden while streaming"), BubbleText(TEXT("<think>still thinking")), FString());
	return true;
}

#endif
