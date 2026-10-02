#include "Luna/LunaChatProtocol.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> Utf8Bytes(const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	FString ChunkLine(const FString& Content, const bool bDone)
	{
		return FString::Printf(TEXT("{\"model\":\"test\",\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},\"done\":%s}\n"),
			*Content, bDone ? TEXT("true") : TEXT("false"));
	}
}

// 네트워크 조각이 줄과 한글(UTF-8 3바이트) 중간에서 끊겨도 완성된 줄만 순서대로 해석하는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaStreamParserTest, "LabProject.Luna.Protocol.StreamParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdLunaStreamParserTest::RunTest(const FString& Parameters)
{
	const TArray<uint8> Stream = Utf8Bytes(ChunkLine(TEXT("안녕하세요, "), false) + ChunkLine(TEXT("루나예요."), false)
		+ ChunkLine(TEXT(""), true));

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
	FString LastLine = ChunkLine(TEXT("끝"), true);
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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaReplyTextTest, "LabProject.Luna.Protocol.ReplyText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FPdLunaReplyTextTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("closed think block"), LunaChat::StripThinking(TEXT("<think>plan</think>안녕하세요")), FString(TEXT("안녕하세요")));
	TestEqual(TEXT("open think block is hidden while streaming"), LunaChat::StripThinking(TEXT("<think>still thinking")), FString());
	TestEqual(TEXT("no think block"), LunaChat::StripThinking(TEXT("그대로")), FString(TEXT("그대로")));
	TestEqual(TEXT("bubble text"), LunaChat::ToBubbleText(TEXT("  **방 목록**을 열어보세요.\r\n\n\n좋아요!  ")),
		FString(TEXT("방 목록을 열어보세요.\n좋아요!")));
	TestEqual(TEXT("tip key"), LunaChat::TipKey(7), FName(TEXT("Title.LunaTip07")));
	return true;
}

#endif
