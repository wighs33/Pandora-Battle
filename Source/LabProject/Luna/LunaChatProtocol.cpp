#include "Luna/LunaChatProtocol.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace LunaChat
{
	FName TipKey(const int32 TipNumber)
	{
		return FName(*FString::Printf(TEXT("Title.LunaTip%02d"), TipNumber));
	}

	void FStreamParser::Append(const TConstArrayView<uint8> Bytes)
	{
		Pending.Append(Bytes.GetData(), Bytes.Num());
	}

	void FStreamParser::Consume(FString& OutContent, bool& bOutDone, FString& OutError)
	{
		// '\n'(0x0A)은 UTF-8 다중 바이트 문자 안에 나오지 않으므로, 바이트 단위로 줄을 나눠도 문자가 깨지지 않는다.
		int32 LineStart = 0;
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Pending[Index] == '\n')
			{
				ParseLine(MakeArrayView(Pending.GetData() + LineStart, Index - LineStart), OutContent, bOutDone, OutError);
				LineStart = Index + 1;
			}
		}
		if (LineStart > 0)
		{
			Pending.RemoveAt(0, LineStart, EAllowShrinking::No);
		}
	}

	void FStreamParser::Flush(FString& OutContent, bool& bOutDone, FString& OutError)
	{
		Consume(OutContent, bOutDone, OutError);
		if (!Pending.IsEmpty())
		{
			ParseLine(Pending, OutContent, bOutDone, OutError);
			Pending.Reset();
		}
	}

	void FStreamParser::ParseLine(const TConstArrayView<uint8> Line, FString& OutContent, bool& bOutDone, FString& OutError) const
	{
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Line.GetData()), Line.Num());
		FString Text = FString(Converted.Length(), Converted.Get()).TrimStartAndEnd();
		if (Text.IsEmpty() || Text.StartsWith(TEXT(":")) || Text.StartsWith(TEXT("event:")))
		{
			return;
		}

		// llama-server(OpenAI 호환)는 Server-Sent Events로 "data: {...}" 줄을 보내고 "data: [DONE]"으로 끝낸다.
		if (Text.StartsWith(TEXT("data:")))
		{
			Text = Text.RightChop(5).TrimStart();
			if (Text == TEXT("[DONE]"))
			{
				bOutDone = true;
				return;
			}
		}

		TSharedPtr<FJsonObject> Json;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
		{
			OutError = TEXT("The model server returned a line that is not JSON.");
			return;
		}

		const FString Error = ReadError(*Json);
		if (!Error.IsEmpty())
		{
			OutError = Error;
			return;
		}

		FString Content;
		if (ReadMessageContent(*Json, Content))
		{
			OutContent += Content;
		}

		bool bDone = false;
		if (Json->TryGetBoolField(TEXT("done"), bDone) && bDone)
		{
			bOutDone = true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		FString FinishReason;
		if (Json->TryGetArrayField(TEXT("choices"), Choices) && !Choices->IsEmpty() && (*Choices)[0]->AsObject().IsValid()
			&& (*Choices)[0]->AsObject()->TryGetStringField(TEXT("finish_reason"), FinishReason) && !FinishReason.IsEmpty())
		{
			bOutDone = true;
		}
	}

	FString ReadError(const FJsonObject& Json)
	{
		FString Error;
		if (Json.TryGetStringField(TEXT("error"), Error))
		{
			return Error;
		}
		const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
		if (Json.TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject && (*ErrorObject)->TryGetStringField(TEXT("message"), Error))
		{
			return Error;
		}
		return FString();
	}

	bool ReadMessageContent(const FJsonObject& Json, FString& OutContent)
	{
		// Ollama: {"message":{"content":"..."}}
		const TSharedPtr<FJsonObject>* Message = nullptr;
		if (Json.TryGetObjectField(TEXT("message"), Message) && Message && (*Message)->TryGetStringField(TEXT("content"), OutContent))
		{
			return true;
		}

		// OpenAI 호환: 스트리밍은 choices[0].delta.content, 한 번에 받을 때는 choices[0].message.content
		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (!Json.TryGetArrayField(TEXT("choices"), Choices) || Choices->IsEmpty())
		{
			return false;
		}
		const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
		const TSharedPtr<FJsonObject>* Part = nullptr;
		return Choice.IsValid()
			&& ((Choice->TryGetObjectField(TEXT("delta"), Part) && Part && (*Part)->TryGetStringField(TEXT("content"), OutContent))
				|| (Choice->TryGetObjectField(TEXT("message"), Part) && Part && (*Part)->TryGetStringField(TEXT("content"), OutContent)));
	}

	FString StripThinking(const FString& Text)
	{
		static const FString OpenTag(TEXT("<think>"));
		static const FString CloseTag(TEXT("</think>"));

		FString Result;
		int32 Cursor = 0;
		while (Cursor < Text.Len())
		{
			const int32 Open = Text.Find(OpenTag, ESearchCase::IgnoreCase, ESearchDir::FromStart, Cursor);
			if (Open == INDEX_NONE)
			{
				Result += Text.Mid(Cursor);
				break;
			}
			Result += Text.Mid(Cursor, Open - Cursor);
			const int32 Close = Text.Find(CloseTag, ESearchCase::IgnoreCase, ESearchDir::FromStart, Open + OpenTag.Len());
			if (Close == INDEX_NONE)
			{
				break;
			}
			Cursor = Close + CloseTag.Len();
		}
		return Result;
	}

	FString ToBubbleText(const FString& Text)
	{
		FString Result = Text;
		Result.ReplaceInline(TEXT("**"), TEXT(""));
		Result.ReplaceInline(TEXT("__"), TEXT(""));
		Result.ReplaceInline(TEXT("\r"), TEXT(""));
		while (Result.Contains(TEXT("\n\n")))
		{
			Result.ReplaceInline(TEXT("\n\n"), TEXT("\n"));
		}
		return Result.TrimStartAndEnd();
	}

	TArray<FGuideSection> SplitGuideSections(const FString& PageText)
	{
		TArray<FString> Lines;
		PageText.ParseIntoArrayLines(Lines);
		TArray<FGuideSection> Sections;
		for (const FString& Line : Lines)
		{
			const FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.StartsWith(TEXT("[")) && Trimmed.EndsWith(TEXT("]")))
			{
				Sections.AddDefaulted_GetRef().Title = Trimmed.Mid(1, Trimmed.Len() - 2).TrimStartAndEnd();
			}
			if (Sections.IsEmpty() || Trimmed.IsEmpty())
			{
				continue;
			}
			if (Trimmed.StartsWith(TEXT("■")))
			{
				Sections.Last().Subtitles.Add(Trimmed.RightChop(1).TrimStart());
			}
			Sections.Last().Text += Trimmed + TEXT("\n");
		}
		return Sections;
	}

	FString DescribeGuideSection(const FGuideSection& Section)
	{
		return Section.Subtitles.IsEmpty() ? Section.Title : Section.Title + TEXT(": ") + FString::Join(Section.Subtitles, TEXT(", "));
	}

	TArray<int32> ParseChosenSections(const FString& ResponseBody, const int32 SectionCount, const int32 MaxSections)
	{
		TArray<int32> Chosen;
		TSharedPtr<FJsonObject> Response;
		FString Content;
		TSharedPtr<FJsonObject> Answer;
		const TArray<TSharedPtr<FJsonValue>>* Numbers = nullptr;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseBody), Response) || !Response.IsValid()
			|| !ReadMessageContent(*Response, Content)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content), Answer) || !Answer.IsValid()
			|| !Answer->TryGetArrayField(TEXT("sections"), Numbers))
		{
			return Chosen;
		}

		for (const TSharedPtr<FJsonValue>& Number : *Numbers)
		{
			double Value = 0.0;
			const int32 Index = Number->TryGetNumber(Value) ? FMath::RoundToInt32(Value) - 1 : INDEX_NONE;
			if (Index >= 0 && Index < SectionCount && Chosen.Num() < MaxSections)
			{
				Chosen.AddUnique(Index);
			}
		}
		return Chosen;
	}
}
