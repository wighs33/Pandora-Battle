#include "Profile/PlayerProfileSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerProfileSaveGame)

DEFINE_LOG_CATEGORY_STATIC(LogProfileSaveEnvelope, Log, All);

namespace
{
	// 기존 저장파일의 CRC와 난독화 키이므로 슬롯 이름과 별개로 이 값은 유지한다.
	const FString ProfileStorageKey(TEXT("LocalProfile"));
	constexpr int32 MaxProfilePayloadSize = 16 * 1024 * 1024;
	constexpr uint32 ProfileObfuscationSaltA = 0x7A19D4E3u;
	constexpr uint32 ProfileObfuscationSaltB = 0xC53B82F1u;

	uint32 MakeObfuscationState(const int32 StorageFormatVersion, const uint32 Nonce)
	{
		uint32 State = FCrc::StrCrc32(
			*ProfileStorageKey,
			ProfileObfuscationSaltA ^ Nonce);
		State = FCrc::TypeCrc32(StorageFormatVersion, State ^ ProfileObfuscationSaltB);
		return State != 0 ? State : ProfileObfuscationSaltB;
	}

	void ObfuscatePayload(TArray<uint8>& Payload, const int32 StorageFormatVersion, const uint32 Nonce)
	{
		uint32 State = MakeObfuscationState(StorageFormatVersion, Nonce);

		for (int32 Index = 0; Index < Payload.Num(); ++Index)
		{
			// 로컬 저장 파일의 값이 바로 읽히지 않게만 하는 층이라 짧은 결정적 난수열로 충분하다.
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			State += ProfileObfuscationSaltA + static_cast<uint32>(Index);

			const uint8 StreamByte = static_cast<uint8>(State ^ (State >> 8) ^ (State >> 16) ^ (State >> 24));
			Payload[Index] ^= StreamByte;
		}
	}

	uint32 CalculatePayloadCrc(const TArray<uint8>& Payload)
	{
		const uint32 PlayerContextCrc = FCrc::StrCrc32(*ProfileStorageKey, ProfileObfuscationSaltB);
		return FCrc::MemCrc32(Payload.GetData(), Payload.Num(), PlayerContextCrc);
	}
}

bool UPdSaveGame::IsCurrentFormat() const
{
	return ProfileDataVersion == PlayerProfileDataVersion::Current && SaveId.IsValid() && SaveRevision >= 0;
}

UProfileSaveEnvelope* UProfileSaveEnvelope::CreateFromProfile(UPdSaveGame* Profile, UObject* Outer)
{
	if (!IsValid(Profile))
	{
		return nullptr;
	}

	if (!Profile->IsCurrentFormat())
	{
		UE_LOG(LogProfileSaveEnvelope, Error,
			TEXT("[SaveGameWrite] Refused to serialize profile '%s' with an invalid version or identity."),
			*ProfileStorageKey);
		return nullptr;
	}

	TArray<uint8> PlainPayload;
	if (!UGameplayStatics::SaveGameToMemory(Profile, PlainPayload) || PlainPayload.IsEmpty()
		|| PlainPayload.Num() > MaxProfilePayloadSize)
	{
		return nullptr;
	}

	UProfileSaveEnvelope* Envelope = NewObject<UProfileSaveEnvelope>(Outer ? Outer : GetTransientPackage());
	if (!Envelope)
	{
		return nullptr;
	}

	const FGuid NonceGuid = FGuid::NewGuid();
	Envelope->StorageFormatVersion = PlayerProfileStorageVersion::Current;
	Envelope->ObfuscationNonce = GetTypeHash(NonceGuid) ^ static_cast<uint32>(FPlatformTime::Cycles());
	if (Envelope->ObfuscationNonce == 0)
	{
		Envelope->ObfuscationNonce = ProfileObfuscationSaltA;
	}

	Envelope->PlainPayloadCrc = CalculatePayloadCrc(PlainPayload);
	Envelope->ProfileSaveId = Profile->SaveId;
	Envelope->ProfileRevision = Profile->SaveRevision;
	Envelope->ObfuscatedPayload = MoveTemp(PlainPayload);
	ObfuscatePayload(Envelope->ObfuscatedPayload, Envelope->StorageFormatVersion, Envelope->ObfuscationNonce);
	return Envelope;
}

FString UProfileSaveEnvelope::FindHeaderProblem() const
{
	if (StorageFormatVersion != PlayerProfileStorageVersion::Current)
	{
		return FString::Printf(TEXT("unsupported storage version %d (current=%d)"), StorageFormatVersion,
			PlayerProfileStorageVersion::Current);
	}
	if (ObfuscationNonce == 0)
	{
		return TEXT("invalid zero nonce");
	}
	if (!ProfileSaveId.IsValid() || ProfileRevision < 0)
	{
		return TEXT("invalid header SaveId or Revision");
	}
	if (ObfuscatedPayload.IsEmpty() || ObfuscatedPayload.Num() > MaxProfilePayloadSize)
	{
		return FString::Printf(TEXT("invalid payload size %d (allowed=1..%d bytes)"), ObfuscatedPayload.Num(),
			MaxProfilePayloadSize);
	}
	return FString();
}

UPdSaveGame* UProfileSaveEnvelope::DecodeProfile() const
{
	const FString HeaderProblem = FindHeaderProblem();
	if (!HeaderProblem.IsEmpty())
	{
		UE_LOG(LogProfileSaveEnvelope, Error, TEXT("[SaveGameLoad] Envelope decode failed for '%s': %s."), *ProfileStorageKey,
			*HeaderProblem);
		return nullptr;
	}

	TArray<uint8> PlainPayload = ObfuscatedPayload;
	ObfuscatePayload(PlainPayload, StorageFormatVersion, ObfuscationNonce);

	const uint32 CalculatedPayloadCrc = CalculatePayloadCrc(PlainPayload);
	if (CalculatedPayloadCrc != PlainPayloadCrc)
	{
		UE_LOG(LogProfileSaveEnvelope, Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': payload CRC mismatch (stored=0x%08X, calculated=0x%08X). "
				"The file may be corrupt or may have been saved for a different ProfileStorageKey."),
			*ProfileStorageKey, PlainPayloadCrc, CalculatedPayloadCrc);
		return nullptr;
	}

	USaveGame* DecodedSaveGame = UGameplayStatics::LoadGameFromMemory(PlainPayload);
	UPdSaveGame* DecodedProfile = Cast<UPdSaveGame>(DecodedSaveGame);
	if (!DecodedProfile)
	{
		UE_LOG(LogProfileSaveEnvelope, Error,
			TEXT("[SaveGameLoad] Envelope payload for '%s' could not be deserialized as UPdSaveGame. Object='%s', Class='%s'."),
			*ProfileStorageKey, *GetNameSafe(DecodedSaveGame),
			DecodedSaveGame ? *GetNameSafe(DecodedSaveGame->GetClass()) : TEXT("None"));
		return nullptr;
	}

	if (DecodedProfile->SaveId != ProfileSaveId || DecodedProfile->SaveRevision != ProfileRevision)
	{
		UE_LOG(LogProfileSaveEnvelope, Error,
			TEXT("[SaveGameLoad] Envelope metadata mismatch for '%s': HeaderSaveId='%s', PayloadSaveId='%s', HeaderRevision=%lld, PayloadRevision=%lld."),
			*ProfileStorageKey, *ProfileSaveId.ToString(), *DecodedProfile->SaveId.ToString(), ProfileRevision,
			DecodedProfile->SaveRevision);
		return nullptr;
	}

	// v1의 미사용 Pandora 선택/배치/포인트 필드는 태그 직렬화에서 건너뛴다.
	if (DecodedProfile->ProfileDataVersion == 1) DecodedProfile->ProfileDataVersion = PlayerProfileDataVersion::Current;
	if (!DecodedProfile->IsCurrentFormat())
	{
		UE_LOG(LogProfileSaveEnvelope, Error,
			TEXT("[SaveGameLoad] Envelope payload for '%s' has an invalid version or identity."),
			*ProfileStorageKey);
		return nullptr;
	}

	UE_LOG(LogProfileSaveEnvelope, Verbose,
		TEXT("[SaveGameLoad] Envelope decoded successfully for '%s': PayloadBytes=%d, SaveId='%s', Revision=%lld, DataVersion=%d."),
		*ProfileStorageKey, PlainPayload.Num(), *DecodedProfile->SaveId.ToString(), DecodedProfile->SaveRevision,
		DecodedProfile->ProfileDataVersion);
	return DecodedProfile;
}
