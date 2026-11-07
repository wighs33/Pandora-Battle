#include "Luna/LunaChatProtocol.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// 외부 서비스와 주고받는 형식의 단위 테스트: Luna 채팅의 모델 서버(llama-server) 스트리밍 응답 해석.
// 실제 서비스 없이는 연결 테스트로 볼 수 없어 바이트 단위 입력으로 본다.

namespace
{
	TArray<uint8> Utf8Bytes(const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	/** llama-server의 SSE 한 덩어리. 실제 서버처럼 이벤트마다 빈 줄을 둔다. */
	FString LunaSseChunk(const FString& Content)
	{
		return FString::Printf(TEXT("data: {\"choices\":[{\"finish_reason\":null,\"index\":0,\"delta\":{\"content\":\"%s\"}}],\"object\":\"chat.completion.chunk\"}\n\n"),
			*Content);
	}
}

// 네트워크 조각이 줄과 한글(UTF-8 3바이트) 중간에서 끊겨도 완성된 줄만 순서대로 해석하고,
// 내용이 null인 첫 조각, finish_reason과 [DONE], 오류 객체를 읽는지 확인한다. 말풍선에는 생각 블록을 뺀 답만 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaStreamTest, "LabProject.Unit.Protocol.LunaStream",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdLunaStreamTest::RunTest(const FString& Parameters)
{
	const FString RoleChunk = TEXT("data: {\"choices\":[{\"finish_reason\":null,\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":null}}]}\n\n");
	const FString StopChunk = TEXT("data: {\"choices\":[{\"finish_reason\":\"stop\",\"index\":0,\"delta\":{}}]}\n\n");
	const FString TimingsChunk = TEXT("data: {\"choices\":[],\"timings\":{\"predicted_n\":9}}\n\n");
	const TArray<uint8> Stream = Utf8Bytes(RoleChunk + LunaSseChunk(TEXT("안녕하세요, ")) + LunaSseChunk(TEXT("루나예요.")) + StopChunk
		+ TimingsChunk + TEXT("data: [DONE]\n\n"));

	LunaChat::FStreamParser Parser;
	FString Content;
	bool bDone = false;
	FString Error;
	bool bDoneBeforeLastChunk = false;
	// 1바이트씩 넣어 모든 경계에서 끊기는 경우를 만든다.
	for (const uint8 Byte : Stream)
	{
		Parser.Append(MakeArrayView(&Byte, 1));
		Parser.Consume(Content, bDone, Error);
		bDoneBeforeLastChunk |= bDone && Content != TEXT("안녕하세요, 루나예요.");
	}
	TestEqual(TEXT("content"), Content, FString(TEXT("안녕하세요, 루나예요.")));
	TestTrue(TEXT("done"), bDone);
	TestFalse(TEXT("not done before the text arrived"), bDoneBeforeLastChunk);
	TestTrue(TEXT("no error"), Error.IsEmpty());

	// 줄바꿈 없이 끝난 마지막 줄은 Flush에서 해석한다.
	LunaChat::FStreamParser DoneParser;
	FString DoneContent;
	bool bOnlyDone = false;
	FString DoneError;
	DoneParser.Append(Utf8Bytes(LunaSseChunk(TEXT("끝")) + TEXT("data: [DONE]")));
	DoneParser.Consume(DoneContent, bOnlyDone, DoneError);
	TestFalse(TEXT("unterminated line waits"), bOnlyDone);
	DoneParser.Flush(DoneContent, bOnlyDone, DoneError);
	TestEqual(TEXT("flushed content"), DoneContent, FString(TEXT("끝")));
	TestTrue(TEXT("[DONE] ends the stream"), bOnlyDone);

	// 서버는 오류를 {"error":{"message":...}} 객체로 보낸다.
	LunaChat::FStreamParser ErrorParser;
	FString ErrorContent;
	bool bErrorDone = false;
	FString ServerError;
	ErrorParser.Append(Utf8Bytes(TEXT("{\"error\":{\"code\":400,\"message\":\"context too long\",\"type\":\"invalid_request_error\"}}\n")));
	ErrorParser.Consume(ErrorContent, bErrorDone, ServerError);
	TestEqual(TEXT("server error object"), ServerError, FString(TEXT("context too long")));

	// 말풍선 문자열은 Luna 채팅이 그리는 것과 같은 순서(생각 블록 제거 → 말풍선 정리)로 만든다.
	const auto BubbleText = [](const FString& Reply) { return LunaChat::ToBubbleText(LunaChat::StripThinking(Reply)); };
	TestEqual(TEXT("think block and markdown removed"), BubbleText(TEXT("<think>plan</think>  **방 목록**을 열어보세요.\r\n\n\n좋아요!  ")),
		FString(TEXT("방 목록을 열어보세요.\n좋아요!")));
	TestEqual(TEXT("open think block is hidden while streaming"), BubbleText(TEXT("<think>still thinking")), FString());
	return true;
}

// Luna가 답에 넣을 가이드 절: "[ 제목 ]" 줄로 나눈 절과, 구조화 출력으로 받은 절 번호 해석을 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdLunaGuideSectionTest, "LabProject.Unit.Protocol.LunaGuideSections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPdLunaGuideSectionTest::RunTest(const FString& Parameters)
{
	const TArray<LunaChat::FGuideSection> Sections = LunaChat::SplitGuideSections(
		TEXT("제목 없는 머리말\n[ 공격 ]\n\n■ 힘\n• 무기 피해가 늘어납니다.\n\n■ 치명타\n• 2.0 + 치명타 / 100\n  [ 방어 ]\n• 방어력"));
	if (!TestEqual(TEXT("section count"), Sections.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("text before the first title is dropped"), Sections[0].Title, FString(TEXT("공격")));
	TestEqual(TEXT("subtitles become the hint"), LunaChat::DescribeGuideSection(Sections[0]), FString(TEXT("공격: 힘, 치명타")));
	TestEqual(TEXT("blank lines are removed"), Sections[0].Text,
		FString(TEXT("[ 공격 ]\n■ 힘\n• 무기 피해가 늘어납니다.\n■ 치명타\n• 2.0 + 치명타 / 100\n")));
	TestEqual(TEXT("indented title"), LunaChat::DescribeGuideSection(Sections[1]), FString(TEXT("방어")));

	// 번호는 1부터 오고, 범위 밖·중복 번호는 버리며 최대 개수까지만 쓴다.
	const auto Choose = [](const FString& Content, const int32 MaxSections)
	{
		const FString Body = FString::Printf(TEXT("{\"choices\":[{\"finish_reason\":\"stop\",\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"}}]}"),
			*Content.ReplaceCharWithEscapedChar());
		return LunaChat::ParseChosenSections(Body, 5, MaxSections);
	};
	TestEqual(TEXT("one-based numbers"), Choose(TEXT("{\"sections\":[2,5]}"), 2), TArray<int32>({1, 4}));
	TestEqual(TEXT("out of range and duplicates"), Choose(TEXT("{\"sections\":[0,6,3,3,1]}"), 3), TArray<int32>({2, 0}));
	TestEqual(TEXT("limited to max"), Choose(TEXT("{\"sections\":[1,2,3]}"), 2), TArray<int32>({0, 1}));
	TestTrue(TEXT("no section fits"), Choose(TEXT("{\"sections\":[]}"), 2).IsEmpty());
	TestTrue(TEXT("content is not JSON"), Choose(TEXT("1, 2"), 2).IsEmpty());
	TestTrue(TEXT("response is not JSON"), LunaChat::ParseChosenSections(TEXT("oops"), 5, 2).IsEmpty());

	// 모델은 JSON 사이에 줄바꿈과 공백을 넣기도 한다.
	TestEqual(TEXT("whitespace inside the JSON"), Choose(TEXT("{\n\"sections\":\n[\n  2\n]\n}"), 2), TArray<int32>({1}));
	return true;
}

#endif
