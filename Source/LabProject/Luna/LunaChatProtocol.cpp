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
		const FString Text(Converted.Length(), Converted.Get());
		if (Text.TrimStartAndEnd().IsEmpty())
		{
			return;
		}

		TSharedPtr<FJsonObject> Json;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
		{
			OutError = TEXT("Ollama returned a line that is not JSON.");
			return;
		}

		FString Error;
		if (Json->TryGetStringField(TEXT("error"), Error))
		{
			OutError = Error;
			return;
		}

		const TSharedPtr<FJsonObject>* Message = nullptr;
		FString Content;
		if (Json->TryGetObjectField(TEXT("message"), Message) && Message && (*Message)->TryGetStringField(TEXT("content"), Content))
		{
			OutContent += Content;
		}

		bool bDone = false;
		if (Json->TryGetBoolField(TEXT("done"), bDone) && bDone)
		{
			bOutDone = true;
		}
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
}
