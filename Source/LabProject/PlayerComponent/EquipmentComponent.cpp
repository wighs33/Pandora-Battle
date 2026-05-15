#include "PlayerComponent/EquipmentComponent.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/PdCharacterBase.h"
#include "Common/Enum_Operation.h"
#include "Common/ProjectTagConfig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentComponent)

DEFINE_LOG_CATEGORY(EquipmentComponentLog);

/** 장비 컴포넌트 기본 상태를 초기화합니다. */
UEquipmentComponent::UEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// === 기본 설정

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

/** 시작 시 캐시를 갱신합니다. */
void UEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedReferences();
}

/** 소유 캐릭터, ASC, Inventory 캐시를 갱신합니다. */
void UEquipmentComponent::RefreshCachedReferences()
{
	CachedOwner = Cast<APdCharacterBase>(GetOwner());
	const APdPlayerState* PdPlayerState = CachedOwner ? Cast<APdPlayerState>(CachedOwner->GetPlayerState()) : nullptr;
	CachedASC = CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
	CachedInventory = PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;

	UE_LOG(EquipmentComponentLog, Log, TEXT("RefreshCachedReferences: owner=%s playerState=%s asc=%s inventory=%s"),
		*GetNameSafe(CachedOwner.Get()),
		*GetNameSafe(PdPlayerState),
		*GetNameSafe(CachedASC.Get()),
		*GetNameSafe(CachedInventory.Get()));
}

/** 복제 프로퍼티를 등록합니다. */
void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// =================================================================================================================
	// === 복제 파라미터 설정

	FDoRepLifetimeParams CurrentWeaponParams;
	CurrentWeaponParams.bIsPushBased = true;

	FDoRepLifetimeParams CurrentWeaponIdParams;
	CurrentWeaponIdParams.bIsPushBased = true;
	CurrentWeaponIdParams.Condition = COND_OwnerOnly;

	// =================================================================================================================
	// === 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponActor, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponId, CurrentWeaponIdParams);
}

/** 요청된 아이템 정의를 반환합니다. */
const UItemDefinition* UEquipmentComponent::GetRequestedWeaponDefinition() const
{
	// =================================================================================================================
	// === 요청 아이템 조회

	if (RequestedWeaponId.IsValid())
	{
		if (const UItemInstance* ItemInstance = FindOwnedItemInstanceById(RequestedWeaponId))
		{
			return ItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

/** 장착 데이터를 반환합니다. */
bool UEquipmentComponent::GetEquipData(FEquipData& OutEquipData) const
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	OutEquipData = FEquipData();

	const UItemDefinition* ItemDefinition = GetRequestedWeaponDefinition();
	if (!ensure(ItemDefinition))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("GetEquipData failed: requested weapon definition is null. owner=%s requestedId=%s"),
			*GetNameSafe(GetOwner()),
			*RequestedWeaponId.ToString());
		return false;
	}

	const FWeaponEquipDefinitionData& EquipDefinitionData = ItemDefinition->WeaponData.Equip;
	UAnimMontage* EquipMontage = EquipDefinitionData.EquipMontage.LoadSynchronous();
	if (!ensure(EquipMontage))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("GetEquipData failed: missing EquipMontage. definition=%s idTag=%s"),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString());
		return false;
	}
	
	// =================================================================================================================
	// === 장착 데이터 구성
	
	OutEquipData.ItemDefinition = ItemDefinition;
	OutEquipData.EquipMontage = EquipMontage;
	OutEquipData.EquipAnimLayer = EquipDefinitionData.AnimLayer.LoadSynchronous();
	return true;
}

/** 장착 해제 데이터를 반환합니다. */
bool UEquipmentComponent::GetUnequipData(FUnequipData& OutUnequipData) const
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	OutUnequipData = FUnequipData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("GetUnequipData failed: current weapon definition is null. owner=%s currentId=%s actor=%s"),
			*GetNameSafe(GetOwner()),
			*CurrentWeaponId.ToString(),
			*GetNameSafe(CurrentWeaponActor));
		return false;
	}

	const FWeaponEquipDefinitionData& EquipDefinitionData = ItemDefinition->WeaponData.Equip;
	UAnimMontage* UnequipMontage = EquipDefinitionData.UnequipMontage.LoadSynchronous();
	if (!ensure(UnequipMontage))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("GetUnequipData failed: missing UnequipMontage. definition=%s idTag=%s"),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString());
		return false;
	}
		
	// =================================================================================================================
	// === 장착 해제 데이터 구성

	OutUnequipData.ItemDefinition = ItemDefinition;
	OutUnequipData.UnequipMontage = UnequipMontage;
	return true;
}

/** 요청 아이템을 저장합니다. */
bool UEquipmentComponent::SetRequestedWeaponInstance(UItemInstance* WeaponInstance)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!IsValid(WeaponInstance) || !InventoryComponent)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("SetRequestedWeaponInstance failed: weapon=%s inventory=%s owner=%s"),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(InventoryComponent),
			*GetNameSafe(GetOwner()));
		ClearRequestedWeapon();
		return false;
	}

	const FGuid WeaponId = InventoryComponent->GetOrCreateItemId(WeaponInstance);
	if (!WeaponId.IsValid())
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("SetRequestedWeaponInstance failed: invalid item id. weapon=%s definition=%s"),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(WeaponInstance->ItemDefinition.Get()));
		ClearRequestedWeapon();
		return false;
	}
	
	// =================================================================================================================
	// === 요청 상태 갱신

	RequestedWeaponId = WeaponId;
	UE_LOG(EquipmentComponentLog, Log, TEXT("SetRequestedWeaponInstance succeeded: owner=%s weapon=%s definition=%s requestedId=%s hasAuthority=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(WeaponInstance),
		*GetNameSafe(WeaponInstance->ItemDefinition.Get()),
		*RequestedWeaponId.ToString(),
		GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		UE_LOG(EquipmentComponentLog, Log, TEXT("SetRequestedWeaponInstance sending ServerSetRequestedWeapon: id=%s"), *WeaponId.ToString());
		ServerSetRequestedWeapon(WeaponId);
	}

	return true;
}

void UEquipmentComponent::ClearRequestedWeaponInstance()
{
	ClearRequestedWeapon();
}

/** 요청된 무기를 장착합니다. */
bool UEquipmentComponent::RequestWeaponSelection(UItemInstance* WeaponInstance)
{
	RefreshCachedReferences();

	const FGameplayTag EquipAbilityTag = GetEquipAbilityTag();
	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();

	UE_LOG(EquipmentComponentLog, Log, TEXT("RequestWeaponSelection started: owner=%s weapon=%s definition=%s currentId=%s currentActor=%s equipTag=%s unequipTag=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(WeaponInstance),
		*GetNameSafe(WeaponInstance ? WeaponInstance->ItemDefinition.Get() : nullptr),
		*CurrentWeaponId.ToString(),
		*GetNameSafe(CurrentWeaponActor),
		*EquipAbilityTag.ToString(),
		*UnequipAbilityTag.ToString());

	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!IsValid(WeaponInstance) || !InventoryComponent)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("RequestWeaponSelection failed: weapon=%s inventory=%s"),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(InventoryComponent));
		return false;
	}

	const FGuid SelectedWeaponId = InventoryComponent->GetOrCreateItemId(WeaponInstance);
	if (!SelectedWeaponId.IsValid() || IsCurrentWeapon(SelectedWeaponId))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("RequestWeaponSelection failed: selectedId=%s valid=%s alreadyCurrent=%s"),
			*SelectedWeaponId.ToString(),
			SelectedWeaponId.IsValid() ? TEXT("true") : TEXT("false"),
			IsCurrentWeapon(SelectedWeaponId) ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (!SetRequestedWeaponInstance(WeaponInstance))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("RequestWeaponSelection failed: SetRequestedWeaponInstance returned false."));
		return false;
	}

	if (CurrentWeaponActor)
	{
		FGameplayTagContainer UnequipTagContainer;
		UnequipTagContainer.AddTag(UnequipAbilityTag);
		if (HasActiveAbilityWithTags(UnequipTagContainer))
		{
			UE_LOG(EquipmentComponentLog, Warning, TEXT("RequestWeaponSelection failed: unequip ability already active. tag=%s"),
				*UnequipAbilityTag.ToString());
			return false;
		}

		const bool bActivatedUnequip = TryActivateSingleAbilityTag(UnequipAbilityTag);
		UE_LOG(EquipmentComponentLog, Log, TEXT("RequestWeaponSelection activated unequip for weapon swap: tag=%s result=%s"),
			*UnequipAbilityTag.ToString(),
			bActivatedUnequip ? TEXT("true") : TEXT("false"));
		return bActivatedUnequip;
	}

	const bool bActivatedEquip = TryActivateSingleAbilityTag(EquipAbilityTag);
	UE_LOG(EquipmentComponentLog, Log, TEXT("RequestWeaponSelection activated equip: tag=%s result=%s"),
		*EquipAbilityTag.ToString(),
		bActivatedEquip ? TEXT("true") : TEXT("false"));
	return bActivatedEquip;
}

bool UEquipmentComponent::RequestWeaponUnequip()
{
	RefreshCachedReferences();
	ClearRequestedWeaponInstance();

	const FGameplayTag UnequipAbilityTag = GetUnequipAbilityTag();

	UE_LOG(EquipmentComponentLog, Log, TEXT("RequestWeaponUnequip started: owner=%s currentId=%s currentActor=%s unequipTag=%s"),
		*GetNameSafe(GetOwner()),
		*CurrentWeaponId.ToString(),
		*GetNameSafe(CurrentWeaponActor),
		*UnequipAbilityTag.ToString());

	FGameplayTagContainer UnequipTagContainer;
	UnequipTagContainer.AddTag(UnequipAbilityTag);
	if (HasActiveAbilityWithTags(UnequipTagContainer))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("RequestWeaponUnequip failed: unequip ability already active. tag=%s"),
			*UnequipAbilityTag.ToString());
		return false;
	}

	const bool bActivated = TryActivateSingleAbilityTag(UnequipAbilityTag);
	UE_LOG(EquipmentComponentLog, Log, TEXT("RequestWeaponUnequip activated unequip: tag=%s result=%s"),
		*UnequipAbilityTag.ToString(),
		bActivated ? TEXT("true") : TEXT("false"));
	return bActivated;
}

bool UEquipmentComponent::EquipWeapon()
{
	if (!RequestedWeaponId.IsValid())
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeapon failed: RequestedWeaponId is invalid. owner=%s"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (!ensure(GetOwner()))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeapon failed: owner is null."));
		return false;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(EquipmentComponentLog, Log, TEXT("EquipWeapon forwarding to server: owner=%s requestedId=%s"),
			*GetNameSafe(GetOwner()),
			*RequestedWeaponId.ToString());
		ServerEquipWeapon();
		ClearRequestedWeapon();
		return true;
	}

	const FGuid WeaponId = RequestedWeaponId;
	ClearRequestedWeapon();

	UItemInstance* WeaponInstance = FindOwnedItemInstanceById(WeaponId);
	if (!ensure(WeaponInstance))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeapon failed: could not find owned item by id=%s owner=%s inventory=%s"),
			*WeaponId.ToString(),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(CachedInventory.Get()));
		return false;
	}

	const bool bEquipped = EquipWeaponInternal(WeaponInstance);
	UE_LOG(EquipmentComponentLog, Log, TEXT("EquipWeapon completed: id=%s weapon=%s result=%s"),
		*WeaponId.ToString(),
		*GetNameSafe(WeaponInstance),
		bEquipped ? TEXT("true") : TEXT("false"));
	return bEquipped;
}

/** 현재 아이템을 해제합니다. */
bool UEquipmentComponent::UnequipCurrentWeapon()
{
	if (!ensure(GetOwner()))
	{
		return false;
	}

	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	
	return UnequipCurrentWeaponInternal();
}

/** 서버에 요청된 무기 장착을 요청합니다. */
void UEquipmentComponent::ServerEquipWeapon_Implementation()
{
	UE_LOG(EquipmentComponentLog, Log, TEXT("ServerEquipWeapon received: owner=%s requestedId=%s"),
		*GetNameSafe(GetOwner()),
		*RequestedWeaponId.ToString());
	EquipWeapon();
}

/** 서버에 요청 아이템 ID를 전달합니다. */
void UEquipmentComponent::ServerSetRequestedWeapon_Implementation(FGuid WeaponId)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	if (!ensure(WeaponId.IsValid()))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ServerSetRequestedWeapon failed: invalid weapon id. owner=%s"),
			*GetNameSafe(GetOwner()));
		ClearRequestedWeapon();
		return;
	}

	// =================================================================================================================
	// === 요청 아이디 설정
	
	RequestedWeaponId = WeaponId;
	UItemInstance* FoundItem = FindOwnedItemInstanceById(WeaponId);
	UE_LOG(EquipmentComponentLog, Log, TEXT("ServerSetRequestedWeapon applied: owner=%s requestedId=%s foundItem=%s definition=%s"),
		*GetNameSafe(GetOwner()),
		*WeaponId.ToString(),
		*GetNameSafe(FoundItem),
		*GetNameSafe(FoundItem ? FoundItem->ItemDefinition.Get() : nullptr));
	ensure(FoundItem);
}

/** 실제 장착 로직을 처리합니다. */
bool UEquipmentComponent::EquipWeaponInternal(UItemInstance* WeaponInstance)
{
	UE_LOG(EquipmentComponentLog, Log, TEXT("EquipWeaponInternal started: owner=%s weapon=%s definition=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(WeaponInstance),
		*GetNameSafe(WeaponInstance ? WeaponInstance->ItemDefinition.Get() : nullptr));

	const UItemDefinition* ItemDefinition = nullptr;
	FGuid NewCurrentWeaponId;
	if (!ResolveWeaponEquipRequest(WeaponInstance, ItemDefinition, NewCurrentWeaponId))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeaponInternal failed: ResolveWeaponEquipRequest returned false."));
		return false;
	}

	if (IsCurrentWeapon(NewCurrentWeaponId))
	{
		UE_LOG(EquipmentComponentLog, Log, TEXT("EquipWeaponInternal skipped: weapon is already current. id=%s actor=%s"),
			*NewCurrentWeaponId.ToString(),
			*GetNameSafe(CurrentWeaponActor));
		return true;
	}

	TSubclassOf<AWeaponBase> WeaponClass = LoadWeaponActorClass(ItemDefinition);
	if (!WeaponClass)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeaponInternal failed: weapon actor class load failed. definition=%s idTag=%s"),
			*GetNameSafe(ItemDefinition),
			ItemDefinition ? *ItemDefinition->IdTag.ToString() : TEXT("None"));
		return false;
	}

	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemStatSnapshot(WeaponInstance, PendingStatSnapshot))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeaponInternal failed: BuildItemStatSnapshot returned false. weapon=%s definition=%s"),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	UnequipCurrentWeaponInternal();

	AWeaponBase* SpawnedWeapon = SpawnAndAttachWeaponActor(WeaponClass, ItemDefinition);
	if (!SpawnedWeapon)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeaponInternal failed: SpawnAndAttachWeaponActor returned null. class=%s definition=%s"),
			*GetNameSafe(WeaponClass.Get()),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	ApplyAndStoreWeaponStats(ItemDefinition, PendingStatSnapshot);
	CommitCurrentWeaponState(NewCurrentWeaponId, SpawnedWeapon);
	UE_LOG(EquipmentComponentLog, Log, TEXT("EquipWeaponInternal succeeded: id=%s weaponActor=%s definition=%s"),
		*NewCurrentWeaponId.ToString(),
		*GetNameSafe(SpawnedWeapon),
		*GetNameSafe(ItemDefinition));
	return true;
}

/** 장착에 필요한 아이템, ID, 액터 클래스, 스탯 정보를 구성합니다. */
bool UEquipmentComponent::ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const
{
	OutItemDefinition = nullptr;
	OutWeaponId.Invalidate();

	APdCharacterBase* CharacterOwner = CachedOwner.Get();
	const UItemDefinition* ItemDefinition = WeaponInstance ? WeaponInstance->ItemDefinition.Get() : nullptr;
	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!ensure(CharacterOwner) || !ensure(ItemDefinition) || !ensure(InventoryComponent))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ResolveWeaponEquipRequest failed: character=%s weapon=%s definition=%s inventory=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(ItemDefinition),
			*GetNameSafe(InventoryComponent));
		return false;
	}

	const FGuid NewCurrentWeaponId = InventoryComponent->GetOrCreateItemId(WeaponInstance);
	if (!ensure(NewCurrentWeaponId.IsValid()))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ResolveWeaponEquipRequest failed: invalid item id. weapon=%s definition=%s"),
			*GetNameSafe(WeaponInstance),
			*GetNameSafe(ItemDefinition));
		return false;
	}

	OutItemDefinition = ItemDefinition;
	OutWeaponId = NewCurrentWeaponId;
	return true;
}

/** 이미 현재 장착된 무기인지 반환합니다. */
bool UEquipmentComponent::IsCurrentWeapon(FGuid WeaponId) const
{
	return CurrentWeaponActor
		&& CurrentWeaponId.IsValid()
		&& CurrentWeaponId == WeaponId;
}

bool UEquipmentComponent::TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const
{
	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || !AbilityTag.IsValid())
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("TryActivateSingleAbilityTag failed: asc=%s tag=%s owner=%s"),
			*GetNameSafe(ASC),
			*AbilityTag.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FGameplayTagContainer AbilityTagContainer;
	AbilityTagContainer.AddTag(AbilityTag);
	const bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTagContainer, true);
	UE_LOG(EquipmentComponentLog, Log, TEXT("TryActivateSingleAbilityTag: owner=%s asc=%s tag=%s result=%s abilityCount=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ASC),
		*AbilityTag.ToString(),
		bActivated ? TEXT("true") : TEXT("false"),
		ASC->GetActivatableAbilities().Num());
	return bActivated;
}

bool UEquipmentComponent::HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC || AbilityTags.IsEmpty())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability)
		{
			continue;
		}

		if (AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return true;
		}
	}

	return false;
}

FGameplayTag UEquipmentComponent::GetEquipAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetEquipmentEquipAbilityTag();
}

FGameplayTag UEquipmentComponent::GetUnequipAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetEquipmentUnequipAbilityTag();
}

TSubclassOf<AWeaponBase> UEquipmentComponent::LoadWeaponActorClass(const UItemDefinition* ItemDefinition) const
{
	if (!ensure(ItemDefinition))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("LoadWeaponActorClass failed: item definition is null."));
		return nullptr;
	}

	TSubclassOf<AWeaponBase> WeaponClass = ItemDefinition->WeaponData.Equip.ActorClass.LoadSynchronous();
	if (!ensure(WeaponClass))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("LoadWeaponActorClass failed: ActorClass is empty or failed to load. definition=%s idTag=%s"),
			*GetNameSafe(ItemDefinition),
			*ItemDefinition->IdTag.ToString());
		return nullptr;
	}

	UE_LOG(EquipmentComponentLog, Log, TEXT("LoadWeaponActorClass succeeded: definition=%s class=%s"),
		*GetNameSafe(ItemDefinition),
		*GetNameSafe(WeaponClass.Get()));
	return WeaponClass;
}

/** 무기 액터를 생성하고 소유자 메시에 부착합니다. */
AWeaponBase* UEquipmentComponent::SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const
{
	APdCharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!ensure(WeaponClass) || !ensure(ItemDefinition) || !ensure(CharacterOwner) || !ensure(OwnerMesh) || !World)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("SpawnAndAttachWeaponActor failed: class=%s definition=%s character=%s mesh=%s world=%s"),
			*GetNameSafe(WeaponClass.Get()),
			*GetNameSafe(ItemDefinition),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OwnerMesh),
			*GetNameSafe(World));
		return nullptr;
	}

	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	const FName AttachSocketName = ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName();
	if (AttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(AttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(AttachSocketName);
	}
	else if (AttachSocketName != NAME_None)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("SpawnAndAttachWeaponActor: attach socket does not exist. character=%s mesh=%s socket=%s definition=%s"),
			*GetNameSafe(CharacterOwner),
			*GetNameSafe(OwnerMesh),
			*AttachSocketName.ToString(),
			*GetNameSafe(ItemDefinition));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CharacterOwner;
	SpawnParams.Instigator = CharacterOwner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* SpawnedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);
	if (!ensure(SpawnedWeapon))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("SpawnAndAttachWeaponActor failed: SpawnActor returned null. class=%s definition=%s"),
			*GetNameSafe(WeaponClass.Get()),
			*GetNameSafe(ItemDefinition));
		return nullptr;
	}

	SpawnedWeapon->InitializeFromItemDefinition(ItemDefinition);
	SpawnedWeapon->SetReplicates(true);
	AttachWeaponToOwner(SpawnedWeapon, ItemDefinition);
	UE_LOG(EquipmentComponentLog, Log, TEXT("SpawnAndAttachWeaponActor succeeded: actor=%s class=%s socket=%s definition=%s"),
		*GetNameSafe(SpawnedWeapon),
		*GetNameSafe(WeaponClass.Get()),
		*AttachSocketName.ToString(),
		*GetNameSafe(ItemDefinition));
	return SpawnedWeapon;
}

/** 장착 스탯 GameplayEffect를 적용하고 현재 스냅샷으로 저장합니다. */
void UEquipmentComponent::ApplyAndStoreWeaponStats(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& PendingStatSnapshot)
{
	if (!ApplyItemStatSnapshot(PendingStatSnapshot, 1.f))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipWeaponInternal: failed to apply item stat snapshot for '%s', but keeping weapon equipped."),
			*GetNameSafe(ItemDefinition));
	}
	else if (PendingStatSnapshot.HasAnyMagnitude())
	{
		CurrentWeaponStatSnapshot = MoveTemp(PendingStatSnapshot);
	}
}

/** 현재 장착 스탯 GameplayEffect를 역적용합니다. */
void UEquipmentComponent::RemoveCurrentWeaponStats()
{
	if (!CurrentWeaponStatSnapshot.HasAnyMagnitude())
	{
		return;
	}

	if (!ApplyItemStatSnapshot(CurrentWeaponStatSnapshot, -1.f))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("RemoveCurrentWeaponStats failed: equipment stat GameplayEffect was not reverted."));
	}

	CurrentWeaponStatSnapshot.Reset();
}

/** 현재 무기 상태를 확정하고 복제 dirty 플래그를 표시합니다. */
void UEquipmentComponent::CommitCurrentWeaponState(FGuid NewCurrentWeaponId, AWeaponBase* NewWeaponActor)
{
	const bool bCurrentWeaponChanged = CurrentWeaponActor != NewWeaponActor;
	const bool bCurrentWeaponIdChanged = CurrentWeaponId != NewCurrentWeaponId;
	CurrentWeaponActor = NewWeaponActor;
	CurrentWeaponId = NewCurrentWeaponId;

	UE_LOG(EquipmentComponentLog, Log, TEXT("CommitCurrentWeaponState: owner=%s id=%s actor=%s changedActor=%s changedId=%s"),
		*GetNameSafe(GetOwner()),
		*CurrentWeaponId.ToString(),
		*GetNameSafe(CurrentWeaponActor),
		bCurrentWeaponChanged ? TEXT("true") : TEXT("false"),
		bCurrentWeaponIdChanged ? TEXT("true") : TEXT("false"));

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bCurrentWeaponIdChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponId, this);
		}

		if (bCurrentWeaponChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponActor, this);
		}
	}
}

/** 실제 장착 해제 로직을 처리합니다. */
bool UEquipmentComponent::UnequipCurrentWeaponInternal()
{
	// =================================================================================================================
	// === 해제 가능 여부 검사
	
	if (!CurrentWeaponId.IsValid() && !CurrentWeaponActor)
	{
		UE_LOG(EquipmentComponentLog, Log, TEXT("UnequipCurrentWeaponInternal skipped: no current weapon. owner=%s"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	UE_LOG(EquipmentComponentLog, Log, TEXT("UnequipCurrentWeaponInternal started: owner=%s currentId=%s currentActor=%s"),
		*GetNameSafe(GetOwner()),
		*CurrentWeaponId.ToString(),
		*GetNameSafe(CurrentWeaponActor));

	RemoveCurrentWeaponStats();

	// =================================================================================================================
	// === 무기 제거
	
	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->Destroy();
	}
	
	// =================================================================================================================
	// === 상태 정리

	const bool bCurrentWeaponChanged = CurrentWeaponActor != nullptr;
	const bool bCurrentWeaponIdChanged = CurrentWeaponId.IsValid();
	CurrentWeaponActor = nullptr;
	CurrentWeaponId.Invalidate();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bCurrentWeaponIdChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponId, this);
		}

		if (bCurrentWeaponChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponActor, this);
		}

	}
	UE_LOG(EquipmentComponentLog, Log, TEXT("UnequipCurrentWeaponInternal succeeded: owner=%s"), *GetNameSafe(GetOwner()));
	return true;
}

/** 공격 데이터를 반환합니다. */
bool UEquipmentComponent::GetAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ensure(ItemDefinition))
	{
		return false;
	}

	const FWeaponAttackDefinitionData& AttackDefinitionData = ItemDefinition->WeaponData.Attack;
	UAnimMontage* AttackMontage = AttackDefinitionData.AttackMontage.LoadSynchronous();
	if (!ensure(AttackMontage))
	{
		return false;
	}

	OutAttackData.ItemDefinition = ItemDefinition;
	OutAttackData.AttackMontage = AttackMontage;
	return true;
}

bool UEquipmentComponent::AllowsMovementDuringAttack() const
{
	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	return !ItemDefinition || ItemDefinition->WeaponData.Attack.bAllowMovementDuringAttack;
}

/** 피격 리액션 데이터를 반환합니다. */
bool UEquipmentComponent::GetHitReactData(FHitReactData& OutHitReactData) const
{
	OutHitReactData = FHitReactData();

	const UItemDefinition* ItemDefinition = GetCurrentWeaponDefinition();
	if (!ItemDefinition || ItemDefinition->WeaponData.HitReact.HitReactMontage.IsNull())
	{
		return false;
	}

	UAnimMontage* HitReactMontage = ItemDefinition->WeaponData.HitReact.HitReactMontage.LoadSynchronous();
	if (!HitReactMontage)
	{
		return false;
	}

	OutHitReactData.ItemDefinition = ItemDefinition;
	OutHitReactData.HitReactMontage = HitReactMontage;
	return true;
}

void UEquipmentComponent::ClearRequestedWeapon()
{
	RequestedWeaponId.Invalidate();
}

/** 현재 아이템 정의를 반환합니다. */
const UItemDefinition* UEquipmentComponent::GetCurrentWeaponDefinition() const
{
	if (CurrentWeaponId.IsValid())
	{
		if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentWeaponId))
		{
			return EquippedItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

/** 아이템 스탯 스냅샷을 구성합니다. */
bool UEquipmentComponent::BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const
{
	OutSnapshot.Reset();

	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	if (!ItemDefinition)
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemDefinition->Map_Stat_Magnitude)
	{
		if (!Pair.Key.IsValid() || FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.BaseStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	for (const TPair<FGameplayTag, float>& Pair : ItemInstance->Map_EnhancedStat_Magnitude)
	{
		if (!Pair.Key.IsValid() || FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		OutSnapshot.EnhancedStatMagnitudes.FindOrAdd(Pair.Key) += Pair.Value;
	}

	return true;
}

/** 스탯 스냅샷을 GameplayEffect로 적용합니다. */
bool UEquipmentComponent::ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const
{
	if (!StatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	UPdAbilitySystemComponent* ASC = CachedASC.Get();
	if (!ASC)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ApplyItemStatSnapshot failed: '%s' has no valid PdAbilitySystemComponent."), *GetNameSafe(GetOwner()));
		return false;
	}

	TMap<FGameplayTag, float> CombinedStatMagnitudes;
	const auto AddMagnitudeMap = [MagnitudeScale, &CombinedStatMagnitudes](const TMap<FGameplayTag, float>& StatMagnitudes)
	{
		for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
		{
			const float ScaledMagnitude = Pair.Value * MagnitudeScale;
			if (!Pair.Key.IsValid() || FMath::IsNearlyZero(ScaledMagnitude))
			{
				continue;
			}

			CombinedStatMagnitudes.FindOrAdd(Pair.Key) += ScaledMagnitude;
		}
	};

	AddMagnitudeMap(StatSnapshot.BaseStatMagnitudes);
	AddMagnitudeMap(StatSnapshot.EnhancedStatMagnitudes);
	if (CombinedStatMagnitudes.IsEmpty())
	{
		return true;
	}

	if (!StatUpGameplayEffectClass)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ApplyItemStatSnapshot failed: missing StatUpGameplayEffectClass."));
		return false;
	}

	if (!ASC->ApplyStatUpEffectByTags(StatUpGameplayEffectClass, CombinedStatMagnitudes, EEnum_Operation::Add))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ApplyItemStatSnapshot failed: equipment stat GameplayEffect was not applied."));
		return false;
	}

	return true;
}

/** 소유 인벤토리에서 아이템 인스턴스를 찾습니다. */
UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(FGuid ItemId) const
{
	// =================================================================================================================
	// === 인벤토리 조회
	
	UInventoryComponent* InventoryComponent = CachedInventory.Get();
	if (!ensure(InventoryComponent))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("FindOwnedItemInstanceById failed: inventory is null. owner=%s itemId=%s"),
			*GetNameSafe(GetOwner()),
			*ItemId.ToString());
		return nullptr;
	}

	UItemInstance* FoundItem = InventoryComponent->FindItemInstanceById(ItemId);
	if (!FoundItem)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("FindOwnedItemInstanceById failed: item not found. inventory=%s itemId=%s"),
			*GetNameSafe(InventoryComponent),
			*ItemId.ToString());
	}
	return FoundItem;
}
/** 무기를 소유자 메시에 부착합니다. */
void UEquipmentComponent::AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const
{
	APdCharacterBase* CharacterOwner = CachedOwner.Get();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!WeaponActor || !OwnerMesh)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("AttachWeaponToOwner failed: weapon=%s mesh=%s owner=%s"),
			*GetNameSafe(WeaponActor),
			*GetNameSafe(OwnerMesh),
			*GetNameSafe(GetOwner()));
		return;
	}

	// =================================================================================================================
	// === 소켓 부착

	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ItemDefinition ? ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName() : NAME_None);
	UE_LOG(EquipmentComponentLog, Log, TEXT("AttachWeaponToOwner: weapon=%s owner=%s mesh=%s socket=%s"),
		*GetNameSafe(WeaponActor),
		*GetNameSafe(CharacterOwner),
		*GetNameSafe(OwnerMesh),
		ItemDefinition ? *ItemDefinition->WeaponData.Equip.GetResolvedAttachSocketName().ToString() : TEXT("None"));
}
