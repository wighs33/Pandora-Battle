#include "SavedGameData/PdSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdSaveGame)

DEFINE_LOG_CATEGORY_STATIC(LogProfileSaveEnvelope, Log, All);

namespace
{
	constexpr int32 MaxProfilePayloadSize = 16 * 1024 * 1024;
	constexpr uint32 ProfileObfuscationSaltA = 0x7A19D4E3u;
	constexpr uint32 ProfileObfuscationSaltB = 0xC53B82F1u;

	uint32 MakeObfuscationState(
		const FString& PlayerId,
		const int32 StorageFormatVersion,
		const uint32 Nonce)
	{
		uint32 State = FCrc::StrCrc32(
			*PlayerId,
			ProfileObfuscationSaltA ^ Nonce);
		State = FCrc::TypeCrc32(
			StorageFormatVersion,
			State ^ ProfileObfuscationSaltB);
		return State != 0 ? State : ProfileObfuscationSaltB;
	}

	void ObfuscatePayload(
		TArray<uint8>& Payload,
		const FString& PlayerId,
		const int32 StorageFormatVersion,
		const uint32 Nonce)
	{
		uint32 State = MakeObfuscationState(
			PlayerId,
			StorageFormatVersion,
			Nonce);

		for (int32 Index = 0; Index < Payload.Num(); ++Index)
		{
			// A small deterministic stream is sufficient here because this layer only
			// prevents immediately readable values in a local save file.
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			State += ProfileObfuscationSaltA + static_cast<uint32>(Index);

			const uint8 StreamByte = static_cast<uint8>(
				State
				^ (State >> 8)
				^ (State >> 16)
				^ (State >> 24));
			Payload[Index] ^= StreamByte;
		}
	}

	uint32 CalculatePayloadCrc(
		const TArray<uint8>& Payload,
		const FString& PlayerId)
	{
		const uint32 PlayerContextCrc =
			FCrc::StrCrc32(*PlayerId, ProfileObfuscationSaltB);
		return FCrc::MemCrc32(
			Payload.GetData(),
			Payload.Num(),
			PlayerContextCrc);
	}
}

bool UPdSaveGame::IsCurrentFormat() const
{
	return ProfileDataVersion == PdProfileSaveData::Current
		&& SaveId.IsValid()
		&& SaveRevision >= 0;
}

UProfileSaveEnvelope* UProfileSaveEnvelope::CreateFromProfile(
	UPdSaveGame* Profile,
	const FString& PlayerId,
	UObject* Outer)
{
	if (!IsValid(Profile) || PlayerId.IsEmpty())
	{
		return nullptr;
	}

	if (!Profile->IsCurrentFormat())
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameWrite] Refused to serialize profile '%s' with an invalid version or identity."),
			*PlayerId);
		return nullptr;
	}

	TArray<uint8> PlainPayload;
	if (!UGameplayStatics::SaveGameToMemory(Profile, PlainPayload)
		|| PlainPayload.IsEmpty()
		|| PlainPayload.Num() > MaxProfilePayloadSize)
	{
		return nullptr;
	}

	UProfileSaveEnvelope* Envelope =
		NewObject<UProfileSaveEnvelope>(
			Outer ? Outer : GetTransientPackage());
	if (!Envelope)
	{
		return nullptr;
	}

	const FGuid NonceGuid = FGuid::NewGuid();
	Envelope->StorageFormatVersion = PdProfileSaveStorage::Current;
	Envelope->ObfuscationNonce =
		GetTypeHash(NonceGuid) ^ static_cast<uint32>(FPlatformTime::Cycles());
	if (Envelope->ObfuscationNonce == 0)
	{
		Envelope->ObfuscationNonce = ProfileObfuscationSaltA;
	}

	Envelope->PlainPayloadCrc =
		CalculatePayloadCrc(PlainPayload, PlayerId);
	Envelope->ProfileSaveId = Profile->SaveId;
	Envelope->ProfileRevision = Profile->SaveRevision;
	Envelope->ObfuscatedPayload = MoveTemp(PlainPayload);
	ObfuscatePayload(
		Envelope->ObfuscatedPayload,
		PlayerId,
		Envelope->StorageFormatVersion,
		Envelope->ObfuscationNonce);
	return Envelope;
}

UPdSaveGame* UProfileSaveEnvelope::DecodeProfile(
	const FString& PlayerId) const
{
	if (PlayerId.IsEmpty())
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode rejected an empty PlayerId."));
		return nullptr;
	}

	if (StorageFormatVersion != PdProfileSaveStorage::Current)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': unsupported storage version %d (current=%d)."),
			*PlayerId,
			StorageFormatVersion,
			PdProfileSaveStorage::Current);
		return nullptr;
	}

	if (ObfuscationNonce == 0)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': invalid zero nonce."),
			*PlayerId);
		return nullptr;
	}

	if (!ProfileSaveId.IsValid() || ProfileRevision < 0)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': invalid header SaveId or Revision."),
			*PlayerId);
		return nullptr;
	}

	if (ObfuscatedPayload.IsEmpty()
		|| ObfuscatedPayload.Num() > MaxProfilePayloadSize)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': invalid payload size %d (allowed=1..%d bytes)."),
			*PlayerId,
			ObfuscatedPayload.Num(),
			MaxProfilePayloadSize);
		return nullptr;
	}

	TArray<uint8> PlainPayload = ObfuscatedPayload;
	ObfuscatePayload(
		PlainPayload,
		PlayerId,
		StorageFormatVersion,
		ObfuscationNonce);

	const uint32 CalculatedPayloadCrc =
		CalculatePayloadCrc(PlainPayload, PlayerId);
	if (CalculatedPayloadCrc != PlainPayloadCrc)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope decode failed for '%s': payload CRC mismatch (stored=0x%08X, calculated=0x%08X). "
				"The file may be corrupt or may have been saved for a different PlayerId."),
			*PlayerId,
			PlainPayloadCrc,
			CalculatedPayloadCrc);
		return nullptr;
	}

	USaveGame* DecodedSaveGame =
		UGameplayStatics::LoadGameFromMemory(PlainPayload);
	UPdSaveGame* DecodedProfile = Cast<UPdSaveGame>(DecodedSaveGame);
	if (!DecodedProfile)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope payload for '%s' could not be deserialized as UPdSaveGame. Object='%s', Class='%s'."),
			*PlayerId,
			*GetNameSafe(DecodedSaveGame),
			DecodedSaveGame ? *GetNameSafe(DecodedSaveGame->GetClass()) : TEXT("None"));
		return nullptr;
	}

	if (DecodedProfile->SaveId != ProfileSaveId
		|| DecodedProfile->SaveRevision != ProfileRevision)
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope metadata mismatch for '%s': HeaderSaveId='%s', PayloadSaveId='%s', HeaderRevision=%lld, PayloadRevision=%lld."),
			*PlayerId,
			*ProfileSaveId.ToString(),
			*DecodedProfile->SaveId.ToString(),
			ProfileRevision,
			DecodedProfile->SaveRevision);
		return nullptr;
	}

	if (!DecodedProfile->IsCurrentFormat())
	{
		UE_LOG(
			LogProfileSaveEnvelope,
			Error,
			TEXT("[SaveGameLoad] Envelope payload for '%s' has an invalid version or identity."),
			*PlayerId);
		return nullptr;
	}

	UE_LOG(
		LogProfileSaveEnvelope,
		Log,
		TEXT("[SaveGameLoad] Envelope decoded successfully for '%s': PayloadBytes=%d, SaveId='%s', Revision=%lld, DataVersion=%d."),
		*PlayerId,
		PlainPayload.Num(),
		*DecodedProfile->SaveId.ToString(),
		DecodedProfile->SaveRevision,
		DecodedProfile->ProfileDataVersion);
	return DecodedProfile;
}
