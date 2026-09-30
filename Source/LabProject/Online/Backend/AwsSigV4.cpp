#include "Online/Backend/AwsSigV4.h"

#include "Containers/StringConv.h"

namespace
{
	// FIPS 180-4 SHA-256 ---------------------------------------------------------------------------------------------
	constexpr uint32 RoundConstants[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

	FORCEINLINE uint32 RotateRight(const uint32 Value, const uint32 Count)
	{
		return (Value >> Count) | (Value << (32 - Count));
	}

	void CompressBlock(uint32 State[8], const uint8* Block)
	{
		uint32 Schedule[64];
		for (int32 Index = 0; Index < 16; ++Index)
		{
			Schedule[Index] = (uint32(Block[Index * 4]) << 24) | (uint32(Block[Index * 4 + 1]) << 16)
				| (uint32(Block[Index * 4 + 2]) << 8) | uint32(Block[Index * 4 + 3]);
		}
		for (int32 Index = 16; Index < 64; ++Index)
		{
			const uint32 S0 = RotateRight(Schedule[Index - 15], 7) ^ RotateRight(Schedule[Index - 15], 18) ^ (Schedule[Index - 15] >> 3);
			const uint32 S1 = RotateRight(Schedule[Index - 2], 17) ^ RotateRight(Schedule[Index - 2], 19) ^ (Schedule[Index - 2] >> 10);
			Schedule[Index] = Schedule[Index - 16] + S0 + Schedule[Index - 7] + S1;
		}

		uint32 A = State[0], B = State[1], C = State[2], D = State[3];
		uint32 E = State[4], F = State[5], G = State[6], H = State[7];
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const uint32 Sum1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
			const uint32 Choose = (E & F) ^ (~E & G);
			const uint32 Temp1 = H + Sum1 + Choose + RoundConstants[Index] + Schedule[Index];
			const uint32 Sum0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
			const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
			const uint32 Temp2 = Sum0 + Majority;
			H = G;
			G = F;
			F = E;
			E = D + Temp1;
			D = C;
			C = B;
			B = A;
			A = Temp1 + Temp2;
		}

		State[0] += A;
		State[1] += B;
		State[2] += C;
		State[3] += D;
		State[4] += E;
		State[5] += F;
		State[6] += G;
		State[7] += H;
	}

	TArray<uint8> ToUtf8(const FString& Text)
	{
		const FTCHARToUTF8 Converted(*Text);
		return TArray<uint8>(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	}

	PdAwsSigV4::FDigest HmacSha256Text(const PdAwsSigV4::FDigest& Key, const FString& Message)
	{
		return PdAwsSigV4::HmacSha256(MakeArrayView(Key.GetData(), Key.Num()), ToUtf8(Message));
	}

	bool IsUnreserved(const uint8 Character)
	{
		return (Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z')
			|| (Character >= '0' && Character <= '9')
			|| Character == '-' || Character == '_' || Character == '.' || Character == '~';
	}

	// SigV4는 요청 경로를 한 번 더 URI 인코딩해 서명한다(S3 제외). 경로 구분자 '/'는 유지한다.
	FString EncodeCanonicalPath(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return TEXT("/");
		}

		const TArray<uint8> Utf8Path = ToUtf8(Path);
		FString Encoded;
		Encoded.Reserve(Utf8Path.Num());
		for (const uint8 Character : Utf8Path)
		{
			if (IsUnreserved(Character) || Character == '/')
			{
				Encoded.AppendChar(static_cast<TCHAR>(Character));
			}
			else
			{
				Encoded.Appendf(TEXT("%%%02X"), Character);
			}
		}
		return Encoded;
	}

	FString CanonicalizeQuery(const FString& Query)
	{
		TArray<FString> Pairs;
		Query.ParseIntoArray(Pairs, TEXT("&"));
		for (FString& Pair : Pairs)
		{
			if (!Pair.Contains(TEXT("=")))
			{
				Pair += TEXT("=");
			}
		}
		Pairs.Sort();
		return FString::Join(Pairs, TEXT("&"));
	}
}

namespace PdAwsSigV4
{
	FDigest Sha256(const TConstArrayView<uint8> Data)
	{
		uint32 State[8] = {
			0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

		const int64 Length = Data.Num();
		int64 Offset = 0;
		for (; Offset + 64 <= Length; Offset += 64)
		{
			CompressBlock(State, Data.GetData() + Offset);
		}

		// 남은 바이트 뒤에 0x80, 0 채움, 64비트 길이를 붙인다. 남은 공간이 부족하면 블록 하나를 더 쓴다.
		uint8 Tail[128] = {};
		const int32 Remaining = static_cast<int32>(Length - Offset);
		if (Remaining > 0)
		{
			FMemory::Memcpy(Tail, Data.GetData() + Offset, Remaining);
		}
		Tail[Remaining] = 0x80;
		const int32 TailSize = Remaining + 1 + 8 <= 64 ? 64 : 128;
		const uint64 BitLength = static_cast<uint64>(Length) * 8;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Tail[TailSize - 1 - Index] = static_cast<uint8>(BitLength >> (8 * Index));
		}
		CompressBlock(State, Tail);
		if (TailSize == 128)
		{
			CompressBlock(State, Tail + 64);
		}

		FDigest Digest;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Digest[Index * 4] = static_cast<uint8>(State[Index] >> 24);
			Digest[Index * 4 + 1] = static_cast<uint8>(State[Index] >> 16);
			Digest[Index * 4 + 2] = static_cast<uint8>(State[Index] >> 8);
			Digest[Index * 4 + 3] = static_cast<uint8>(State[Index]);
		}
		return Digest;
	}

	// RFC 2104 HMAC. 블록보다 긴 키는 먼저 해시한다.
	FDigest HmacSha256(const TConstArrayView<uint8> Key, const TConstArrayView<uint8> Message)
	{
		constexpr int32 BlockSize = 64;
		uint8 BlockKey[BlockSize] = {};
		if (Key.Num() > BlockSize)
		{
			const FDigest HashedKey = Sha256(Key);
			FMemory::Memcpy(BlockKey, HashedKey.GetData(), HashedKey.Num());
		}
		else if (Key.Num() > 0)
		{
			FMemory::Memcpy(BlockKey, Key.GetData(), Key.Num());
		}

		TArray<uint8> Inner;
		Inner.SetNumUninitialized(BlockSize + Message.Num());
		for (int32 Index = 0; Index < BlockSize; ++Index)
		{
			Inner[Index] = BlockKey[Index] ^ 0x36;
		}
		if (Message.Num() > 0)
		{
			FMemory::Memcpy(Inner.GetData() + BlockSize, Message.GetData(), Message.Num());
		}
		const FDigest InnerHash = Sha256(Inner);

		uint8 Outer[BlockSize + 32];
		for (int32 Index = 0; Index < BlockSize; ++Index)
		{
			Outer[Index] = BlockKey[Index] ^ 0x5c;
		}
		FMemory::Memcpy(Outer + BlockSize, InnerHash.GetData(), InnerHash.Num());
		return Sha256(MakeArrayView(Outer, UE_ARRAY_COUNT(Outer)));
	}

	FString ToLowerHex(const TConstArrayView<uint8> Bytes)
	{
		FString Hex;
		Hex.Reserve(Bytes.Num() * 2);
		for (const uint8 Byte : Bytes)
		{
			Hex.Appendf(TEXT("%02x"), Byte);
		}
		return Hex;
	}

	FString ToLowerHex(const FDigest& Digest)
	{
		return ToLowerHex(MakeArrayView(Digest.GetData(), Digest.Num()));
	}

	FSignature Sign(const FRequest& Request, const FAwsCredentials& Credentials)
	{
		FSignature Signature;
		Signature.AmzDate = Request.TimestampUtc.ToString(TEXT("%Y%m%dT%H%M%SZ"));
		const FString DateStamp = Signature.AmzDate.Left(8);

		// 헤더 이름 오름차순: host < x-amz-date < x-amz-security-token
		FString CanonicalHeaders = FString::Printf(TEXT("host:%s\nx-amz-date:%s\n"),
			*Request.Host.ToLower().TrimStartAndEnd(), *Signature.AmzDate);
		FString SignedHeaders = TEXT("host;x-amz-date");
		if (!Credentials.SessionToken.IsEmpty())
		{
			CanonicalHeaders += FString::Printf(TEXT("x-amz-security-token:%s\n"), *Credentials.SessionToken);
			SignedHeaders += TEXT(";x-amz-security-token");
		}

		Signature.CanonicalRequest = FString::Printf(TEXT("%s\n%s\n%s\n%s\n%s\n%s"),
			*Request.Method.ToUpper(),
			*EncodeCanonicalPath(Request.Path),
			*CanonicalizeQuery(Request.Query),
			*CanonicalHeaders,
			*SignedHeaders,
			*ToLowerHex(Sha256(Request.Payload)));

		const FString CredentialScope = FString::Printf(TEXT("%s/%s/%s/aws4_request"),
			*DateStamp, *Request.Region, *Request.Service);
		Signature.StringToSign = FString::Printf(TEXT("AWS4-HMAC-SHA256\n%s\n%s\n%s"),
			*Signature.AmzDate, *CredentialScope, *ToLowerHex(Sha256(ToUtf8(Signature.CanonicalRequest))));

		const FDigest DateKey = HmacSha256(ToUtf8(TEXT("AWS4") + Credentials.SecretAccessKey), ToUtf8(DateStamp));
		const FDigest RegionKey = HmacSha256Text(DateKey, Request.Region);
		const FDigest ServiceKey = HmacSha256Text(RegionKey, Request.Service);
		const FDigest SigningKey = HmacSha256Text(ServiceKey, TEXT("aws4_request"));
		const FString SignatureHex = ToLowerHex(HmacSha256Text(SigningKey, Signature.StringToSign));

		Signature.Authorization = FString::Printf(
			TEXT("AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s"),
			*Credentials.AccessKeyId, *CredentialScope, *SignedHeaders, *SignatureHex);
		return Signature;
	}
}
