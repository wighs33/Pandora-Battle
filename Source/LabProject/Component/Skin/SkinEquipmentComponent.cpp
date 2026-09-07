#include "Component/Skin/SkinEquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/Common/ProjectTagConfig.h"
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
			|| SkinDefinition->IdTag.MatchesTag(UProjectTagConfig::GetDefaultConfig()->GetSkinPetTypeTag())
			|| SlotTag == UProjectTagConfig::GetDefaultConfig()->GetSkinPetTypeTag());
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
	bEndingPlay = false;
	Super::BeginPlay();
	RebuildEquippedSkinActors();
}

void USkinEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	CancelActiveGestureMontage(0.0f);
	bGesturePlayRequestPending = false;
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

// UI의 소유 인스턴스에서 Definition을 꺼내 공통 장착 요청으로 전달한다.
bool USkinEquipmentComponent::RequestEquipSkin(USkinInstance* SkinInstance, const FGameplayTag SlotTag)
{
	USkinDefinition* Definition = IsValid(SkinInstance) ? const_cast<USkinDefinition*>(SkinInstance->SkinDefinition.Get()) : nullptr;
	return RequestEquipSkinDefinition(Definition, SlotTag);
}

bool USkinEquipmentComponent::RequestUnequipSkinSlot(const FGameplayTag SlotTag)
{
	if (bEndingPlay || !GetOwner() || !IsSupportedSkinSlot(SlotTag))
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

// 서버가 장착한 제스처와 현재 행동 상태를 확인한 뒤 재생을 승인한다.
bool USkinEquipmentComponent::RequestPlayGestureSlot(const int32 GestureSlotIndex)
{
	const FGameplayTag SlotTag = ResolveGestureSlotTag(GestureSlotIndex);
	const USkinDefinition* Definition = GetEquippedSkinDefinition(SlotTag);
	UAnimMontage* Montage = Definition ? Definition->GestureMontage.Get() : nullptr;
	if (!SlotTag.IsValid() || !Montage || !CanPlayGesture() || !TryConsumeGesturePlayRequest())
	{
		return false;
	}
	if (!HasSkinEquipmentAuthority())
	{
		bGesturePlayRequestPending = true;
		ServerPlayGestureSlot(GestureSlotIndex);
		return true;
	}
	MulticastPlayGestureMontage(Montage);
	return true;
}

// 이동 입력은 슬롯 내용이 바뀌었어도 직전에 재생한 제스처만 중단한다.
bool USkinEquipmentComponent::RequestCancelActiveGestureMontage(const float BlendOutTime)
{
	const float SafeBlendOutTime = GetClampedGestureBlendOutTime(BlendOutTime);
	if (!HasSkinEquipmentAuthority())
	{
		const bool bHadPendingRequest = bGesturePlayRequestPending;
		bGesturePlayRequestPending = false;
		if (!CancelActiveGestureMontage(SafeBlendOutTime) && !bHadPendingRequest)
		{
			return false;
		}
		ServerCancelActiveGestureMontage(SafeBlendOutTime);
		return true;
	}
	if (!ActiveGestureMontage)
	{
		return false;
	}
	MulticastCancelActiveGestureMontage(SafeBlendOutTime);
	return true;
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
	RequestPlayGestureSlot(GestureSlotIndex);
}

// 취소는 재생 상태를 비우므로 반복 요청이 자동으로 무시된다. 시간 제한으로 필요한 취소를 버리지 않는다.
void USkinEquipmentComponent::ServerCancelActiveGestureMontage_Implementation(const float BlendOutTime)
{
	RequestCancelActiveGestureMontage(BlendOutTime);
}

void USkinEquipmentComponent::MulticastPlayGestureMontage_Implementation(UAnimMontage* GestureMontage)
{
	bGesturePlayRequestPending = false;
	PlayGestureMontage(GestureMontage);
}

void USkinEquipmentComponent::MulticastCancelActiveGestureMontage_Implementation(const float BlendOutTime)
{
	bGesturePlayRequestPending = false;
	CancelActiveGestureMontage(GetClampedGestureBlendOutTime(BlendOutTime));
}

void USkinEquipmentComponent::OnRep_EquippedSkins()
{
	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();
}

bool USkinEquipmentComponent::EquipSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	if (!HasSkinEquipmentAuthority() || !CanEquipSkinDefinition(SkinDefinition, SlotTag))
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

	MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	GetOwner()->ForceNetUpdate();

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();

	return true;
}

bool USkinEquipmentComponent::UnequipSkinSlotInternal(const FGameplayTag SlotTag)
{
	if (bEndingPlay || !HasSkinEquipmentAuthority() || !IsSupportedSkinSlot(SlotTag))
	{
		return false;
	}
	const int32 ExistingSlotIndex = FindEquippedSkinSlotIndex(SlotTag);
	if (ExistingSlotIndex == INDEX_NONE)
	{
		return false;
	}

	EquippedSkins.RemoveAt(ExistingSlotIndex);
	MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	GetOwner()->ForceNetUpdate();

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();
	return true;
}

bool USkinEquipmentComponent::CanEquipSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag) const
{
	if (bEndingPlay || !GetOwner() || !CanReferenceSkinDefinition(SkinDefinition) || !IsSupportedSkinSlot(SlotTag))
	{
		return false;
	}

	if (!SkinDefinition->IdTag.IsValid())
	{
		return false;
	}

	const FGameplayTag RequiredSkinTag = SlotTag.MatchesTag(LabGameplayTags::Skin_Gesture)
		? UProjectTagConfig::Get(this)->GetSkinGestureTypeTag()
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

bool USkinEquipmentComponent::TryConsumeGesturePlayRequest()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const double CurrentTime = World->GetTimeSeconds();
	if (LastGesturePlayRequestTime >= 0.0 && CurrentTime - LastGesturePlayRequestTime < FMath::Max(GesturePlayRequestMinInterval, 0.0))
	{
		return false;
	}
	LastGesturePlayRequestTime = CurrentTime;
	return true;
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

// 다른 몽타주 전체를 중단하지 않고 제스처 재생 대상을 별도로 기억한다.
bool USkinEquipmentComponent::PlayGestureMontage(UAnimMontage* GestureMontage)
{
	ACharacterBase* Character = GetCharacterOwner();
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!GestureMontage)
	{
		return false;
	}
	CancelActiveGestureMontage(DefaultGestureCancelBlendOutTime);
	const bool bPlayed = AnimInstance
		&& AnimInstance->Montage_Play(GestureMontage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false) > 0.f;
	// 로컬 AnimInstance 준비 여부와 무관하게 서버가 승인한 취소 대상을 기억한다.
	ActiveGestureMontage = GestureMontage;
	return bPlayed;
}

bool USkinEquipmentComponent::CancelActiveGestureMontage(const float BlendOutTime)
{
	UAnimMontage* Montage = ActiveGestureMontage;
	ActiveGestureMontage = nullptr;
	ACharacterBase* Character = GetCharacterOwner();
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Montage && AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, Montage);
	}
	return Montage != nullptr;
}

// 변경되지 않은 장식과 펫은 유지하고, 변경된 슬롯에서 아직 없는 클래스만 비동기로 준비한다.
void USkinEquipmentComponent::RebuildEquippedSkinActors()
{
	if (bEndingPlay)
	{
		return;
	}
	ReleaseSkinPresentationLoad();
	const uint32 RequestGeneration = SkinPresentationRequestGeneration;
	TArray<FGameplayTag> PreviousSlots;
	AppliedSkinDefinitions.GetKeys(PreviousSlots);
	for (const FGameplayTag SlotTag : PreviousSlots)
	{
		if (FindEquippedSkinSlotIndex(SlotTag) != INDEX_NONE)
		{
			continue;
		}
		AActor* PreviousActor = EquippedSkinActors.FindRef(SlotTag);
		EquippedSkinActors.Remove(SlotTag);
		AppliedSkinDefinitions.Remove(SlotTag);
		if (IsValid(PreviousActor))
		{
			PreviousActor->Destroy();
		}
		if (RequestGeneration != SkinPresentationRequestGeneration)
		{
			return;
		}
	}

	TSet<FSoftObjectPath> MissingPaths;
	const TArray<FEquippedSkinSlot> DesiredSlots = EquippedSkins;
	for (const FEquippedSkinSlot& Slot : DesiredSlots)
	{
		const USkinDefinition* Definition = Slot.SkinDefinition;
		if (!CanReferenceSkinDefinition(Definition))
		{
			continue;
		}
		const bool bIsPet = IsPetSkinDefinition(Definition, Slot.SlotTag);
		const TSoftClassPtr<AActor> ActorClass = bIsPet ? Definition->PetActorClass : Definition->ActorClass;
		const bool bNeedsActor = bIsPet ? HasSkinEquipmentAuthority() : GetNetMode() != NM_DedicatedServer;
		if (bNeedsActor && !ActorClass.IsNull() && !ActorClass.IsValid())
		{
			MissingPaths.Add(ActorClass.ToSoftObjectPath());
		}
		else
		{
			ApplyEquippedSkinSlot(Slot);
		}
		if (RequestGeneration != SkinPresentationRequestGeneration)
		{
			return;
		}
	}
	if (MissingPaths.IsEmpty())
	{
		return;
	}
	SkinPresentationLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(MissingPaths.Array(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::RebuildEquippedSkinActorsFromLoadedContent, RequestGeneration));
	if (!SkinPresentationLoadHandle.IsValid())
	{
		UE_LOG(SkinEquipmentComponentLog, Error, TEXT("Failed to preload changed skins for '%s'."), *GetPathNameSafe(GetOwner()));
	}
}

// 로딩 중 다른 선택이나 종료가 발생했다면 이전 결과를 적용하지 않는다.
void USkinEquipmentComponent::RebuildEquippedSkinActorsFromLoadedContent(const uint32 RequestGeneration)
{
	const TArray<FEquippedSkinSlot> DesiredSlots = EquippedSkins;
	for (const FEquippedSkinSlot& Slot : DesiredSlots)
	{
		if (bEndingPlay || RequestGeneration != SkinPresentationRequestGeneration || !IsValid(GetOwner()))
		{
			return;
		}
		ApplyEquippedSkinSlot(Slot);
	}
}

// 새 외형을 만들 수 있을 때만 이전 외형을 교체한다. 로딩·생성 실패 시 기존 펫과 장식은 남긴다.
void USkinEquipmentComponent::ApplyEquippedSkinSlot(const FEquippedSkinSlot& Slot)
{
	const USkinDefinition* Definition = Slot.SkinDefinition;
	if (!CanReferenceSkinDefinition(Definition) || GetEquippedSkinDefinition(Slot.SlotTag) != Definition)
	{
		return;
	}
	const bool bIsPet = IsPetSkinDefinition(Definition, Slot.SlotTag);
	const TSoftClassPtr<AActor> ActorClass = bIsPet ? Definition->PetActorClass : Definition->ActorClass;
	const bool bNeedsActor = !ActorClass.IsNull() && (bIsPet ? HasSkinEquipmentAuthority() : GetNetMode() != NM_DedicatedServer);
	AActor* PreviousActor = EquippedSkinActors.FindRef(Slot.SlotTag);
	if (AppliedSkinDefinitions.FindRef(Slot.SlotTag) == Definition && (!bNeedsActor || IsValid(PreviousActor)))
	{
		return;
	}
	if (bNeedsActor && !ActorClass.IsValid())
	{
		UE_LOG(SkinEquipmentComponentLog, Error, TEXT("Skin class '%s' was unavailable after preload."), *ActorClass.ToString());
		return;
	}

	const uint32 RequestGeneration = SkinPresentationRequestGeneration;
	AActor* NewActor = bNeedsActor ? (bIsPet ? SpawnPetSkinActor(Definition) : SpawnAndAttachSkinActor(Definition)) : nullptr;
	if (bNeedsActor && !NewActor)
	{
		return;
	}
	if (bEndingPlay || RequestGeneration != SkinPresentationRequestGeneration || GetEquippedSkinDefinition(Slot.SlotTag) != Definition)
	{
		if (IsValid(NewActor))
		{
			NewActor->Destroy();
		}
		return;
	}
	AppliedSkinDefinitions.Add(Slot.SlotTag, Definition);
	if (NewActor)
	{
		EquippedSkinActors.Add(Slot.SlotTag, NewActor);
	}
	else
	{
		EquippedSkinActors.Remove(Slot.SlotTag);
	}
	if (IsValid(PreviousActor))
	{
		PreviousActor->Destroy();
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

// 캐릭터 종료 시에만 모든 슬롯의 생성 Actor를 정리한다.
void USkinEquipmentComponent::DestroyEquippedSkinActors()
{
	TMap<FGameplayTag, TObjectPtr<AActor>> ActorsToDestroy = MoveTemp(EquippedSkinActors);
	EquippedSkinActors.Reset();
	AppliedSkinDefinitions.Reset();
	for (const TPair<FGameplayTag, TObjectPtr<AActor>>& Entry : ActorsToDestroy)
	{
		if (IsValid(Entry.Value))
		{
			Entry.Value->Destroy();
		}
	}
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

// 장착 가능 여부와 별개로 전투·사망·빙결 중에는 제스처로 행동 몽타주를 덮어쓰지 못하게 한다.
bool USkinEquipmentComponent::CanPlayGesture() const
{
	const ACharacterBase* Character = GetCharacterOwner();
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (bEndingPlay || !Character || !ASC || Character->IsStatusFrozen()
		|| ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
		|| ASC->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_Active))
	{
		return false;
	}
	if (ASC->HasAttributeSetForAttribute(UBasicAttributeSet::GetHealthAttribute())
		&& ASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) <= 0.f)
	{
		return false;
	}
	const UAnimInstance* AnimInstance = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	return !AnimInstance || !AnimInstance->IsAnyMontagePlaying()
		|| (ActiveGestureMontage && AnimInstance->Montage_IsPlaying(ActiveGestureMontage));
}

// 스킨 분류의 상위 태그가 아니라 프로젝트에서 지원하는 정확한 슬롯만 허용한다.
bool USkinEquipmentComponent::IsSupportedSkinSlot(const FGameplayTag SlotTag) const
{
	TArray<FGameplayTag> SupportedSlots;
	UProjectTagConfig::Get(this)->GetSkinEquipmentSlotTags(SupportedSlots);
	return SupportedSlots.Contains(SlotTag);
}
