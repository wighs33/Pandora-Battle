#pragma once

#include "CoreMinimal.h"

/**
 * Ollama `/api/chat` 응답 해석과 가이드 절 나누기 같은 순수 로직. 네트워크 없이 자동화 테스트로 검증한다.
 */
namespace LunaChat
{
	/** DT_MenuText의 Luna 안내 문구 수(Title.LunaTip01 ~ Title.LunaTip20) */
	inline constexpr int32 TipCount = 20;

	LABPROJECT_API FName TipKey(int32 TipNumber);

	/**
	 * 스트리밍 응답은 한 줄에 JSON 하나(NDJSON)다. 네트워크 조각은 줄이나 UTF-8 문자 중간에서 끊길 수 있으므로
	 * 바이트를 모아 두었다가 줄바꿈까지 도착한 줄만 해석한다.
	 */
	class LABPROJECT_API FStreamParser
	{
	public:
		void Append(TConstArrayView<uint8> Bytes);

		/**
		 * 완성된 줄을 모두 해석한다. 새로 도착한 답변 조각은 OutContent에 이어 붙인다.
		 * 마지막 줄(done)을 받았으면 bOutDone, 서버 오류 줄을 받았으면 OutError를 채운다.
		 */
		void Consume(FString& OutContent, bool& bOutDone, FString& OutError);

		/** 응답이 끝났는데 줄바꿈 없이 남은 마지막 줄이 있으면 해석한다. */
		void Flush(FString& OutContent, bool& bOutDone, FString& OutError);

	private:
		void ParseLine(TConstArrayView<uint8> Line, FString& OutContent, bool& bOutDone, FString& OutError) const;

		TArray<uint8> Pending;
	};

	/** 추론형 모델의 <think>…</think> 구간을 뺀다. 아직 닫히지 않은 구간은 끝까지 숨긴다. */
	LABPROJECT_API FString StripThinking(const FString& Text);

	/** 말풍선에 맞게 마크다운 강조 기호를 빼고 공백과 줄바꿈을 정리한다. */
	LABPROJECT_API FString ToBubbleText(const FString& Text);

	/** 가이드에서 "[ 제목 ]" 줄부터 다음 "[ 제목 ]" 줄 전까지. 안쪽 "■ 소제목"은 절을 고를 때 힌트로 보여 준다. */
	struct FGuideSection
	{
		FString Title;
		TArray<FString> Subtitles;
		/** 빈 줄을 뺀 절 본문(제목 줄 포함) */
		FString Text;
	};

	/** 가이드 한 페이지를 절로 나눈다. 첫 "[ 제목 ]" 줄보다 앞의 내용은 어느 절에도 넣지 않는다. */
	LABPROJECT_API TArray<FGuideSection> SplitGuideSections(const FString& PageText);

	/** 절을 고를 때 보여 줄 이름: "제목: 소제목, 소제목" */
	LABPROJECT_API FString DescribeGuideSection(const FGuideSection& Section);

	/**
	 * 절 고르기 응답({"message":{"content":"{\"sections\":[2,5]}"}})에서 고른 절의 인덱스를 꺼낸다.
	 * 모델은 1부터 번호를 매기므로 0부터 센 인덱스로 바꾸고, 범위 밖이거나 겹친 번호는 버리며, 앞에서부터 MaxSections개까지만 둔다.
	 */
	LABPROJECT_API TArray<int32> ParseChosenSections(const FString& ResponseBody, int32 SectionCount, int32 MaxSections);
}
