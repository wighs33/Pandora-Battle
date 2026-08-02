#include "Component/Skin/SkinEquipmentComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pet/PetCharacter.h"
#include "Component/Skin/SkinComponent.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"
#include "UObject/PrimaryAssetId.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipmentComponent)

DEFINE_LOG_CATEGORY(SkinEquipmentComponentLog);

namespace
{
constexpr float DefaultGestureCancelBlendOutTime = 0.12f;

FGameplayTag ResolveGestureSlotTag(const int32 GestureSlotIndex)
{
	switch (GestureSlotIndex)
	{
	case 0:
		return LabGameplayTags::Skin_Gesture_Slot1;
	case 1:
		return LabGameplayTags::Skin_Gesture_Slot2;
	case 2:
		return LabGameplayTags::Skin_Gesture_Slot3;
	case 3:
		return LabGameplayTags::Skin_Gesture_Slot4;
	default:
		return FGameplayTag();
	}
}

bool IsPetSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	return SkinDefinition
		&& (!SkinDefinition->PetActorClass.IsNull()
			|| SkinDefinition->IdTag.MatchesTag(LabGameplayTags::Skin_Pet)
			|| SlotTag.MatchesTag(LabGameplayTags::Skin_Pet));
}
}

USkinEquipmentComponent::USkinEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USkinEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	RebuildEquippedSkinActors();
}

void USkinEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseSkinPresentationLoad();
	DestroyEquippedSkinActors();
	Super::EndPlay(EndPlayReason);
}

void USkinEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(USkinEquipmentComponent, EquippedSkins, Params);
}

bool USkinEquipmentComponent::RequestEquipSkin(USkinInstance* SkinInstance, const FGameplayTag SlotTag)
{
	const USkinDefinition* SkinDefinition = IsValid(SkinInstance) ? SkinInstance->SkinDefinition.Get() : nullptr;
	if (!CanEquipSkinDefinition(SkinDefinition, SlotTag))
	{
		return false;
	}

	if (!HasSkinEquipmentAuthority())
	{
		ServerEquipSkin(const_cast<USkinDefinition*>(SkinDefinition), SlotTag);
		return true;
	}

	return EquipSkinDefinition(SkinDefinition, SlotTag);
}

bool USkinEquipmentComponent::RequestUnequipSkinSlot(const FGameplayTag SlotTag)
{
	if (!GetOwner() || !SlotTag.IsValid())
	{
		return false;
	}

	if (!HasSkinEquipmentAuthority())
	{
		ServerUnequipSkinSlot(SlotTag);
		return true;
	}

	return UnequipSkinSlotInternal(SlotTag);
}

const USkinDefinition* USkinEquipmentComponent::GetEquippedSkinDefinition(const FGameplayTag SlotTag) const
{
	const int32 SlotIndex = FindEquippedSkinSlotIndex(SlotTag);
	return SlotIndex != INDEX_NONE ? EquippedSkins[SlotIndex].SkinDefinition.Get() : nullptr;
}

void USkinEquipmentComponent::GetEquippedSkinSlots(TArray<FEquippedSkinSlot>& OutEquippedSkins) const
{
	OutEquippedSkins = EquippedSkins;
}

bool USkinEquipmentComponent::RequestEquipSkinDefinition(USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	if (!CanEquipSkinDefinition(SkinDefinition, SlotTag))
	{
		return false;
	}

	if (!HasSkinEquipmentAuthority())
	{
		ServerEquipSkin(SkinDefinition, SlotTag);
		return true;
	}

	return EquipSkinDefinition(SkinDefinition, SlotTag);
}

bool USkinEquipmentComponent::RequestPlayGestureSlot(const int32 GestureSlotIndex)
{
	if (!GetOwner())
	{
		return false;
	}

	const FGameplayTag SlotTag = ResolveGestureSlotTag(GestureSlotIndex);
	if (!SlotTag.IsValid())
	{
		return false;
	}

	if (!HasSkinEquipmentAuthority())
	{
		if (!TryConsumeGesturePlayRequest())
		{
			return false;
		}

		ServerPlayGestureSlot(GestureSlotIndex);
		return true;
	}

	const USkinDefinition* SkinDefinition = GetEquippedSkinDefinition(SlotTag);
	UAnimMontage* GestureMontage = SkinDefinition ? SkinDefinition->GestureMontage.Get() : nullptr;
	if (!GestureMontage)
	{
		return false;
	}

	if (!TryConsumeGesturePlayRequest())
	{
		return false;
	}

	MulticastPlayGestureMontage(GestureMontage);
	return true;
}

bool USkinEquipmentComponent::RequestCancelActiveGestureMontage(const float BlendOutTime)
{
	if (!GetOwner())
	{
		return false;
	}

	const float SafeBlendOutTime = GetClampedGestureBlendOutTime(BlendOutTime);
	if (!HasSkinEquipmentAuthority())
	{
		CancelActiveGestureMontage(SafeBlendOutTime);
		ServerCancelActiveGestureMontage(SafeBlendOutTime);
		return true;
	}

	if (!TryConsumeGestureCancelRequest())
	{
		return false;
	}

	const bool bCanceledLocally = CancelActiveGestureMontage(SafeBlendOutTime);
	MulticastCancelActiveGestureMontage(SafeBlendOutTime);
	return bCanceledLocally;
}

void USkinEquipmentComponent::ServerEquipSkin_Implementation(USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	EquipSkinDefinition(SkinDefinition, SlotTag);
}

void USkinEquipmentComponent::ServerUnequipSkinSlot_Implementation(const FGameplayTag SlotTag)
{
	UnequipSkinSlotInternal(SlotTag);
}

void USkinEquipmentComponent::ServerPlayGestureSlot_Implementation(const int32 GestureSlotIndex)
{
	const FGameplayTag SlotTag = ResolveGestureSlotTag(GestureSlotIndex);
	const USkinDefinition* SkinDefinition = SlotTag.IsValid() ? GetEquippedSkinDefinition(SlotTag) : nullptr;
	UAnimMontage* GestureMontage = SkinDefinition ? SkinDefinition->GestureMontage.Get() : nullptr;
	if (!GestureMontage)
	{
		return;
	}

	if (!TryConsumeGesturePlayRequest())
	{
		return;
	}

	MulticastPlayGestureMontage(GestureMontage);
}

void USkinEquipmentComponent::ServerCancelActiveGestureMontage_Implementation(const float BlendOutTime)
{
	if (!TryConsumeGestureCancelRequest())
	{
		return;
	}

	MulticastCancelActiveGestureMontage(GetClampedGestureBlendOutTime(BlendOutTime));
}

void USkinEquipmentComponent::MulticastPlayGestureMontage_Implementation(UAnimMontage* GestureMontage)
{
	PlayGestureMontage(GestureMontage);
}

void USkinEquipmentComponent::MulticastCancelActiveGestureMontage_Implementation(const float BlendOutTime)
{
	CancelActiveGestureMontage(GetClampedGestureBlendOutTime(BlendOutTime));
}

void USkinEquipmentComponent::OnRep_EquippedSkins()
{
	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();
}

bool USkinEquipmentComponent::EquipSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	if (!CanEquipSkinDefinition(SkinDefinition, SlotTag))
	{
		return false;
	}

	const int32 ExistingSlotIndex = FindEquippedSkinSlotIndex(SlotTag);
	if (ExistingSlotIndex != INDEX_NONE)
	{
		if (EquippedSkins[ExistingSlotIndex].SkinDefinition == SkinDefinition)
		{
			return true;
		}

		EquippedSkins[ExistingSlotIndex].SkinDefinition = SkinDefinition;
	}
	else
	{
		FEquippedSkinSlot& NewSlot = EquippedSkins.AddDefaulted_GetRef();
		NewSlot.SlotTag = SlotTag;
		NewSlot.SkinDefinition = SkinDefinition;
	}

	if (HasSkinEquipmentAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	}

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();


	return true;
}

bool USkinEquipmentComponent::UnequipSkinSlotInternal(const FGameplayTag SlotTag)
{
	const int32 ExistingSlotIndex = FindEquippedSkinSlotIndex(SlotTag);
	if (ExistingSlotIndex == INDEX_NONE)
	{
		return false;
	}

	EquippedSkins.RemoveAt(ExistingSlotIndex);
	if (HasSkinEquipmentAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	}

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();
	return true;
}

bool USkinEquipmentComponent::CanEquipSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag) const
{
	if (!GetOwner() || !CanReferenceSkinDefinition(SkinDefinition) || !SlotTag.IsValid())
	{
		return false;
	}

	if (!SkinDefinition->IdTag.IsValid())
	{
		return false;
	}

	const FGameplayTag RequiredSkinTag = SlotTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? LabGameplayTags::Skin_Gesture
		: SlotTag;
	if (!SkinDefinition->IdTag.MatchesTag(RequiredSkinTag))
	{
		return false;
	}

	if (HasSkinEquipmentAuthority())
	{
		const ACharacterBase* CharacterOwner = GetCharacterOwner();
		const APdPlayerState* PlayerState = CharacterOwner ? CharacterOwner->GetPlayerState<APdPlayerState>() : nullptr;
		const USkinComponent* SkinComponent = PlayerState ? PlayerState->GetSkinComponent() : nullptr;
		if (!SkinComponent || !SkinComponent->HasSkinDefinition(SkinDefinition))
		{
			return false;
		}
	}

	return true;
}

bool USkinEquipmentComponent::CanReferenceSkinDefinition(const USkinDefinition* SkinDefinition) const
{
	if (!IsValid(SkinDefinition))
	{
		return false;
	}

	const FPrimaryAssetId PrimaryAssetId = SkinDefinition->GetPrimaryAssetId();
	return PrimaryAssetId.IsValid()
		&& PrimaryAssetId.PrimaryAssetType == FPrimaryAssetType(TEXT("SkinDefinition"));
}

bool USkinEquipmentComponent::HasSkinEquipmentAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

bool USkinEquipmentComponent::TryConsumeGestureNetworkEvent(double& LastRequestTime, const double MinInterval)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	const double SafeMinInterval = FMath::Max(MinInterval, 0.0);
	if (LastRequestTime >= 0.0 && CurrentTime - LastRequestTime < SafeMinInterval)
	{
		return false;
	}

	LastRequestTime = CurrentTime;
	return true;
}

bool USkinEquipmentComponent::TryConsumeGesturePlayRequest()
{
	return TryConsumeGestureNetworkEvent(LastGesturePlayRequestTime, GesturePlayRequestMinInterval);
}

bool USkinEquipmentComponent::TryConsumeGestureCancelRequest()
{
	return TryConsumeGestureNetworkEvent(LastGestureCancelRequestTime, GestureCancelRequestMinInterval);
}

float USkinEquipmentComponent::GetClampedGestureBlendOutTime(const float BlendOutTime) const
{
	const float SafeMaxBlendOutTime = static_cast<float>(FMath::Max(MaxGestureCancelBlendOutTime, 0.0));
	const float SafeBlendOutTime = FMath::IsFinite(BlendOutTime)
		? BlendOutTime
		: DefaultGestureCancelBlendOutTime;
	return FMath::Clamp(SafeBlendOutTime, 0.0f, SafeMaxBlendOutTime);
}

int32 USkinEquipmentComponent::FindEquippedSkinSlotIndex(const FGameplayTag SlotTag) const
{
	for (int32 Index = 0; Index < EquippedSkins.Num(); ++Index)
	{
		if (EquippedSkins[Index].SlotTag == SlotTag)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

ACharacterBase* USkinEquipmentComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

bool USkinEquipmentComponent::PlayGestureMontage(UAnimMontage* GestureMontage) const
{
	ACharacterBase* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = OwnerMesh ? OwnerMesh->GetAnimInstance() : nullptr;
	if (!GestureMontage || !AnimInstance)
	{
		return false;
	}

	const float Duration = AnimInstance->Montage_Play(GestureMontage);

	return Duration > 0.0f;
}

bool USkinEquipmentComponent::CancelActiveGestureMontage(const float BlendOutTime) const
{
	ACharacterBase* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = OwnerMesh ? OwnerMesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	bool bCanceled = false;
	for (const FEquippedSkinSlot& EquippedSkin : EquippedSkins)
	{
		const USkinDefinition* SkinDefinition = EquippedSkin.SkinDefinition.Get();
		UAnimMontage* GestureMontage = nullptr;
		if (CanReferenceSkinDefinition(SkinDefinition)
			&& SkinDefinition->IdTag.MatchesTag(LabGameplayTags::Skin_Gesture))
		{
			GestureMontage = SkinDefinition->GestureMontage.Get();
		}

		if (!GestureMontage || !AnimInstance->Montage_IsPlaying(GestureMontage))
		{
			continue;
		}

		AnimInstance->Montage_Stop(BlendOutTime, GestureMontage);
		bCanceled = true;
	}

	return bCanceled;
}

void USkinEquipmentComponent::RebuildEquippedSkinActors()
{
	ReleaseSkinPresentationLoad();
	DestroyEquippedSkinActors();

	const bool bIsDedicatedServer = GetNetMode() == NM_DedicatedServer;
	TSet<FSoftObjectPath> PresentationAssetPaths;
	for (const FEquippedSkinSlot& EquippedSkin : EquippedSkins)
	{
		const USkinDefinition* SkinDefinition = EquippedSkin.SkinDefinition.Get();
		if (!CanReferenceSkinDefinition(SkinDefinition))
		{
			continue;
		}

		if (IsPetSkinDefinition(SkinDefinition, EquippedSkin.SlotTag))
		{
			if (!SkinDefinition->PetActorClass.IsNull())
			{
				PresentationAssetPaths.Add(SkinDefinition->PetActorClass.ToSoftObjectPath());
			}
		}
		else if (!bIsDedicatedServer && !SkinDefinition->ActorClass.IsNull())
		{
			PresentationAssetPaths.Add(SkinDefinition->ActorClass.ToSoftObjectPath());
		}
	}

	if (PresentationAssetPaths.IsEmpty())
	{
		return;
	}

	const uint32 RequestGeneration = SkinPresentationRequestGeneration;
	SkinPresentationLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PresentationAssetPaths.Array(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::RebuildEquippedSkinActorsFromLoadedContent,
				RequestGeneration));
	if (!SkinPresentationLoadHandle.IsValid())
	{
		UE_LOG(
			SkinEquipmentComponentLog,
			Error,
			TEXT("Failed to start skin presentation preload for '%s'."),
			*GetPathNameSafe(GetOwner()));
	}
}

void USkinEquipmentComponent::RebuildEquippedSkinActorsFromLoadedContent(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != SkinPresentationRequestGeneration
		|| !IsValid(GetOwner()))
	{
		return;
	}

	const bool bHasAuthority = HasSkinEquipmentAuthority();
	const bool bIsDedicatedServer = GetNetMode() == NM_DedicatedServer;
	for (const FEquippedSkinSlot& EquippedSkin : EquippedSkins)
	{
		const USkinDefinition* SkinDefinition = EquippedSkin.SkinDefinition.Get();
		if (!CanReferenceSkinDefinition(SkinDefinition))
		{
			continue;
		}

		AActor* SpawnedActor = nullptr;
		if (IsPetSkinDefinition(SkinDefinition, EquippedSkin.SlotTag))
		{
			if (bHasAuthority)
			{
				SpawnedActor = SpawnPetSkinActor(SkinDefinition);
			}
		}
		else if (!bIsDedicatedServer && !SkinDefinition->ActorClass.IsNull())
		{
			SpawnedActor = SpawnAndAttachSkinActor(SkinDefinition);
		}

		if (SpawnedActor)
		{
			EquippedSkinActors.Add(EquippedSkin.SlotTag, SpawnedActor);
		}
	}
}

void USkinEquipmentComponent::ReleaseSkinPresentationLoad()
{
	++SkinPresentationRequestGeneration;
	if (SkinPresentationLoadHandle.IsValid())
	{
		SkinPresentationLoadHandle->CancelHandle();
		SkinPresentationLoadHandle->ReleaseHandle();
		SkinPresentationLoadHandle.Reset();
	}
}

void USkinEquipmentComponent::DestroyEquippedSkinActors()
{
	for (TPair<FGameplayTag, TObjectPtr<AActor>>& EquippedSkinActor : EquippedSkinActors)
	{
		if (AActor* SkinActor = EquippedSkinActor.Value.Get())
		{
			SkinActor->Destroy();
		}
	}

	EquippedSkinActors.Reset();
}

AActor* USkinEquipmentComponent::SpawnPetSkinActor(const USkinDefinition* SkinDefinition) const
{
	ACharacterBase* CharacterOwner = GetCharacterOwner();
	UWorld* World = GetWorld();
	if (!CharacterOwner || !World || !CanReferenceSkinDefinition(SkinDefinition))
	{
		return nullptr;
	}

	TSubclassOf<AActor> PetActorClass = SkinDefinition->PetActorClass.Get();
	if (!PetActorClass)
	{
		UE_LOG(
			SkinEquipmentComponentLog,
			Error,
			TEXT("Pet skin class '%s' was unavailable after preload for '%s'."),
			*SkinDefinition->PetActorClass.ToString(),
			*GetPathNameSafe(GetOwner()));
		return nullptr;
	}

	const FVector SpawnLocation =
		CharacterOwner->GetActorLocation()
		- CharacterOwner->GetActorForwardVector() * 140.0f
		+ CharacterOwner->GetActorRightVector() * 80.0f;
	const FTransform SpawnTransform(CharacterOwner->GetActorRotation(), SpawnLocation);

	APetCharacter* DeferredPetCharacter = nullptr;
	AActor* SpawnedActor = nullptr;
	if (PetActorClass->IsChildOf(APetCharacter::StaticClass()))
	{
		DeferredPetCharacter = World->SpawnActorDeferred<APetCharacter>(
			PetActorClass,
			SpawnTransform,
			nullptr,
			CharacterOwner,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (DeferredPetCharacter)
		{
			DeferredPetCharacter->SetReplicates(true);
			DeferredPetCharacter->SetReplicateMovement(true);
			DeferredPetCharacter->SetFollowTargetActor(CharacterOwner);
			DeferredPetCharacter->ApplySkinDefinition(SkinDefinition);
			DeferredPetCharacter->FinishSpawning(SpawnTransform);
			SpawnedActor = DeferredPetCharacter;
		}
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Instigator = CharacterOwner;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedActor = World->SpawnActor<AActor>(PetActorClass, SpawnTransform, SpawnParams);
	}

	if (!SpawnedActor)
	{
		return nullptr;
	}

	SpawnedActor->SetReplicates(true);
	SpawnedActor->SetReplicateMovement(true);

	if (APetCharacter* PetCharacter = Cast<APetCharacter>(SpawnedActor))
	{
		if (PetCharacter != DeferredPetCharacter)
		{
			PetCharacter->SetFollowTargetActor(CharacterOwner);
			PetCharacter->ApplySkinDefinition(SkinDefinition);
		}
	}
	else
	{
		if (!Cast<APawn>(SpawnedActor))
		{
			SpawnedActor->AttachToActor(CharacterOwner, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	return SpawnedActor;
}

AActor* USkinEquipmentComponent::SpawnAndAttachSkinActor(const USkinDefinition* SkinDefinition) const
{
	ACharacterBase* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!CharacterOwner || !OwnerMesh || !World || !CanReferenceSkinDefinition(SkinDefinition))
	{
		return nullptr;
	}

	TSubclassOf<AActor> SkinActorClass = SkinDefinition->ActorClass.Get();
	if (!SkinActorClass)
	{
		UE_LOG(
			SkinEquipmentComponentLog,
			Error,
			TEXT("Skin actor class '%s' was unavailable after preload for '%s'."),
			*SkinDefinition->ActorClass.ToString(),
			*GetPathNameSafe(GetOwner()));
		return nullptr;
	}

	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	if (SkinDefinition->AttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(SkinDefinition->AttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(SkinDefinition->AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CharacterOwner;
	SpawnParams.Instigator = CharacterOwner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SkinActor = World->SpawnActor<AActor>(SkinActorClass, SpawnTransform, SpawnParams);
	if (!SkinActor)
	{
		return nullptr;
	}

	SkinActor->SetReplicates(false);
	SkinActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		SkinDefinition->AttachSocketName);

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	SkinActor->GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);
	for (USkeletalMeshComponent* SkinMeshComponent : SkeletalMeshComponents)
	{
		if (!SkinMeshComponent)
		{
			continue;
		}

		if (SkinDefinition->bUseOwnerMeshAsLeaderPose)
		{
			SkinMeshComponent->SetLeaderPoseComponent(OwnerMesh);
		}

		SkinMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkinMeshComponent->SetGenerateOverlapEvents(false);
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	SkinActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent)
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
		}
	}

	return SkinActor;
}
