#include "SavedGameData/PdSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdSaveGame)

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

UPdProfileSaveEnvelope* UPdProfileSaveEnvelope::CreateFromProfile(
	UPdSaveGame* Profile,
	const FString& PlayerId,
	UObject* Outer)
{
	if (!IsValid(Profile) || PlayerId.IsEmpty())
	{
		return nullptr;
	}

	TArray<uint8> PlainPayload;
	if (!UGameplayStatics::SaveGameToMemory(Profile, PlainPayload)
		|| PlainPayload.IsEmpty()
		|| PlainPayload.Num() > MaxProfilePayloadSize)
	{
		return nullptr;
	}

	UPdProfileSaveEnvelope* Envelope =
		NewObject<UPdProfileSaveEnvelope>(
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
	Envelope->ObfuscatedPayload = MoveTemp(PlainPayload);
	ObfuscatePayload(
		Envelope->ObfuscatedPayload,
		PlayerId,
		Envelope->StorageFormatVersion,
		Envelope->ObfuscationNonce);
	return Envelope;
}

UPdSaveGame* UPdProfileSaveEnvelope::DecodeProfile(
	const FString& PlayerId) const
{
	if (PlayerId.IsEmpty()
		|| StorageFormatVersion != PdProfileSaveStorage::Current
		|| ObfuscationNonce == 0
		|| ObfuscatedPayload.IsEmpty()
		|| ObfuscatedPayload.Num() > MaxProfilePayloadSize)
	{
		return nullptr;
	}

	TArray<uint8> PlainPayload = ObfuscatedPayload;
	ObfuscatePayload(
		PlainPayload,
		PlayerId,
		StorageFormatVersion,
		ObfuscationNonce);

	if (CalculatePayloadCrc(PlainPayload, PlayerId) != PlainPayloadCrc)
	{
		return nullptr;
	}

	return Cast<UPdSaveGame>(
		UGameplayStatics::LoadGameFromMemory(PlainPayload));
}

#if WITH_DEV_AUTOMATION_TESTS
void UPdProfileSaveEnvelope::CorruptPayloadForTest()
{
	if (!ObfuscatedPayload.IsEmpty())
	{
		ObfuscatedPayload[ObfuscatedPayload.Num() / 2] ^= 0x5Au;
	}
}
#endif
