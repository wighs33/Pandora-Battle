#include "Skin/SkinEquipmentComponent.h"

#include "Character/PdCharacterBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Skin/SkinComponent.h"
#include "Skin/SkinDefinition.h"
#include "Skin/SkinInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinEquipmentComponent)

DEFINE_LOG_CATEGORY(SkinEquipmentComponentLog);

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
		UE_LOG(SkinEquipmentComponentLog, Warning, TEXT("RequestEquipSkin failed: owner=%s skin=%s definition=%s slot=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(SkinInstance),
			*GetNameSafe(SkinDefinition),
			*SlotTag.ToString());
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		ServerEquipSkin(const_cast<USkinDefinition*>(SkinDefinition), SlotTag);
		return true;
	}

	return EquipSkinDefinition(SkinDefinition, SlotTag);
}

bool USkinEquipmentComponent::RequestUnequipSkinSlot(const FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return false;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
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

void USkinEquipmentComponent::ServerEquipSkin_Implementation(USkinDefinition* SkinDefinition, const FGameplayTag SlotTag)
{
	EquipSkinDefinition(SkinDefinition, SlotTag);
}

void USkinEquipmentComponent::ServerUnequipSkinSlot_Implementation(const FGameplayTag SlotTag)
{
	UnequipSkinSlotInternal(SlotTag);
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

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	}

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();

	UE_LOG(SkinEquipmentComponentLog, Log, TEXT("EquipSkinDefinition succeeded: owner=%s slot=%s definition=%s actorClass=%s"),
		*GetNameSafe(GetOwner()),
		*SlotTag.ToString(),
		*GetNameSafe(SkinDefinition),
		SkinDefinition ? *SkinDefinition->ActorClass.ToString() : TEXT("None"));
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
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(USkinEquipmentComponent, EquippedSkins, this);
	}

	RebuildEquippedSkinActors();
	OnEquippedSkinsChanged.Broadcast();
	return true;
}

bool USkinEquipmentComponent::CanEquipSkinDefinition(const USkinDefinition* SkinDefinition, const FGameplayTag SlotTag) const
{
	if (!GetOwner() || !IsValid(SkinDefinition) || !SlotTag.IsValid())
	{
		return false;
	}

	if (!SkinDefinition->IdTag.MatchesTag(SlotTag))
	{
		UE_LOG(SkinEquipmentComponentLog, Warning, TEXT("CanEquipSkinDefinition failed: skin tag does not match slot. definition=%s idTag=%s slot=%s"),
			*GetNameSafe(SkinDefinition),
			*SkinDefinition->IdTag.ToString(),
			*SlotTag.ToString());
		return false;
	}

	if (GetOwner()->HasAuthority())
	{
		const APdCharacterBase* CharacterOwner = GetCharacterOwner();
		const APdPlayerState* PlayerState = CharacterOwner ? CharacterOwner->GetPlayerState<APdPlayerState>() : nullptr;
		const USkinComponent* SkinComponent = PlayerState ? PlayerState->GetSkinComponent() : nullptr;
		if (!SkinComponent || !SkinComponent->HasSkinDefinition(SkinDefinition))
		{
			UE_LOG(SkinEquipmentComponentLog, Warning, TEXT("CanEquipSkinDefinition failed: owner does not have skin. owner=%s definition=%s skinComponent=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(SkinDefinition),
				*GetNameSafe(SkinComponent));
			return false;
		}
	}

	return true;
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

APdCharacterBase* USkinEquipmentComponent::GetCharacterOwner() const
{
	return Cast<APdCharacterBase>(GetOwner());
}

void USkinEquipmentComponent::RebuildEquippedSkinActors()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	DestroyEquippedSkinActors();

	for (const FEquippedSkinSlot& EquippedSkin : EquippedSkins)
	{
		if (AActor* SpawnedActor = SpawnAndAttachSkinActor(EquippedSkin.SkinDefinition.Get()))
		{
			EquippedSkinActors.Add(EquippedSkin.SlotTag, SpawnedActor);
		}
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

AActor* USkinEquipmentComponent::SpawnAndAttachSkinActor(const USkinDefinition* SkinDefinition) const
{
	APdCharacterBase* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	TSubclassOf<AActor> SkinActorClass = SkinDefinition ? SkinDefinition->ActorClass.LoadSynchronous() : nullptr;
	if (!CharacterOwner || !OwnerMesh || !World || !SkinDefinition || !SkinActorClass)
	{
		UE_LOG(SkinEquipmentComponentLog, Warning, TEXT("SpawnAndAttachSkinActor failed: owner=%s mesh=%s world=%s definition=%s class=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OwnerMesh),
			*GetNameSafe(World),
			*GetNameSafe(SkinDefinition),
			*GetNameSafe(SkinActorClass.Get()));
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
