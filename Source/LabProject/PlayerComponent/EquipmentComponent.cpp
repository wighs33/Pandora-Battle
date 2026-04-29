#include "PlayerComponent/EquipmentComponent.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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
	// =================================================================================================================
	// === 기본 설정

	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

/** 복제 프로퍼티를 등록합니다. */
void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// =================================================================================================================
	// === 복제 파라미터 설정

	FDoRepLifetimeParams CurrentWeaponParams;
	CurrentWeaponParams.bIsPushBased = true;

	FDoRepLifetimeParams CurrentItemIdParams;
	CurrentItemIdParams.bIsPushBased = true;
	CurrentItemIdParams.Condition = COND_OwnerOnly;

	// =================================================================================================================
	// === 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentWeaponActor, CurrentWeaponParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UEquipmentComponent, CurrentItemId, CurrentItemIdParams);
}

/** 요청된 아이템 정의를 반환합니다. */
const UItemDefinition* UEquipmentComponent::GetRequestedItemDefinition() const
{
	// =================================================================================================================
	// === 요청 아이템 조회

	if (RequestedItemId.IsValid())
	{
		if (const UItemInstance* ItemInstance = FindOwnedItemInstanceById(RequestedItemId))
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

	const UItemDefinition* ItemDefinition = GetRequestedItemDefinition();
	if (!ensure(ItemDefinition))
	{
		return false;
	}

	UAnimMontage* EquipMontage = ItemDefinition->EquipMontage.LoadSynchronous();
	if (!ensure(EquipMontage))
	{
		return false;
	}
	
	// =================================================================================================================
	// === 장착 데이터 구성
	
	OutEquipData.ItemDefinition = ItemDefinition;
	OutEquipData.EquipMontage = EquipMontage;
	OutEquipData.EquipAnimLayer = ItemDefinition->EquipAnimLayer.LoadSynchronous();
	return true;
}

/** 장착 해제 데이터를 반환합니다. */
bool UEquipmentComponent::GetUnequipData(FUnequipData& OutUnequipData) const
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	OutUnequipData = FUnequipData();

	const UItemDefinition* ItemDefinition = GetCurrentItemDefinition();
	if (!ItemDefinition)
	{
		return false;
	}

	UAnimMontage* UnequipMontage = ItemDefinition->UnequipMontage.LoadSynchronous();
	if (!ensure(UnequipMontage))
	{
		return false;
	}
		
	// =================================================================================================================
	// === 장착 해제 데이터 구성

	OutUnequipData.ItemDefinition = ItemDefinition;
	OutUnequipData.UnequipMontage = UnequipMontage;
	return true;
}

/** 요청 아이템을 저장합니다. */
bool UEquipmentComponent::SetRequestedItemInstance(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!IsValid(ItemInstance) || !InventoryComponent)
	{
		ClearRequestedItem();
		return false;
	}

	const FGuid ItemId = InventoryComponent->GetOrCreateItemId(ItemInstance);
	if (!ItemId.IsValid())
	{
		ClearRequestedItem();
		return false;
	}
	
	// =================================================================================================================
	// === 요청 상태 갱신

	RequestedItemId = ItemId;
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetRequestedItem(ItemId);
	}

	return true;
}

/** 요청된 아이템을 장착합니다. */
bool UEquipmentComponent::EquipRequestedItem()
{
	if (!ensure(RequestedItemId.IsValid()))
	{
		return false;
	}

	const FGuid RequestedItem = RequestedItemId;
	ClearRequestedItem();
	return EquipItemById(RequestedItem);
}

/** 아이템을 즉시 장착합니다. */
bool UEquipmentComponent::EquipItem(UItemInstance* ItemInstance)
{
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!ensure(ItemInstance) || !ensure(ItemInstance->ItemDefinition) || !ensure(InventoryComponent))
	{
		return false;
	}

	const FGuid ItemId = InventoryComponent->GetOrCreateItemId(ItemInstance);
	if (!ensure(ItemId.IsValid()))
	{
		return false;
	}

	return EquipItemById(ItemId);
}

/** 현재 아이템을 해제합니다. */
bool UEquipmentComponent::UnequipCurrentItem()
{
	if (!ensure(GetOwner()))
	{
		return false;
	}

	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	
	return UnequipCurrentItemInternal();
}

/** 서버에 장착 아이템 ID를 전달합니다. */
void UEquipmentComponent::ServerEquipItem_Implementation(FGuid ItemId)
{
	if (!ensure(ItemId.IsValid()))
	{
		return;
	}
	
	EquipItemById(ItemId);
}

/** 서버에 요청 아이템 ID를 전달합니다. */
void UEquipmentComponent::ServerSetRequestedItem_Implementation(FGuid ItemId)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	if (!ensure(ItemId.IsValid()))
	{
		ClearRequestedItem();
		return;
	}

	// =================================================================================================================
	// === 요청 아이디 설정
	
	RequestedItemId = ItemId;
	ensure(FindOwnedItemInstanceById(ItemId));
}

/** ID 기준으로 장착을 처리합니다. */
bool UEquipmentComponent::EquipItemById(FGuid ItemId)
{
	if (!ensure(ItemId.IsValid()) || !ensure(GetOwner()))
	{
		return false;
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerEquipItem(ItemId);
		return true;
	}

	UItemInstance* ItemInstance = FindOwnedItemInstanceById(ItemId);
	if (!ensure(ItemInstance))
	{
		return false;
	}

	return EquipItemInternal(ItemInstance);
}

/** 실제 장착 로직을 처리합니다. */
bool UEquipmentComponent::EquipItemInternal(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	ACharacter* CharacterOwner = GetCharacterOwner();
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!ensure(CharacterOwner) || !ensure(ItemDefinition))
	{
		return false;
	}

	// =================================================================================================================
	// === 중복 장착 검사
	
	if (CurrentItemId.IsValid() && ItemInstance && CurrentWeaponActor)
	{
		if (InventoryComponent)
		{
			const FGuid RequestedEquipItemId = InventoryComponent->GetOrCreateItemId(ItemInstance);
			if (RequestedEquipItemId.IsValid() && CurrentItemId == RequestedEquipItemId)
			{
				return true;
			}
		}
	}

	// =================================================================================================================
	// === 메시 확보
	
	USkeletalMeshComponent* OwnerMesh = CharacterOwner->GetMesh();
	if (!ensure(OwnerMesh))
	{
		return false;
	}

	// =================================================================================================================
	// === 무기 클래스 로드
	
	TSubclassOf<AWeaponBase> WeaponClass = ItemDefinition->EquippedActorClass.LoadSynchronous();
	if (!ensure(WeaponClass))
	{
		return false;
	}

	// =================================================================================================================
	// === 스탯 스냅샷 준비
	
	FEquippedItemStatSnapshot PendingStatSnapshot;
	if (!BuildItemStatSnapshot(ItemInstance, PendingStatSnapshot))
	{
		return false;
	}

	UnequipCurrentItemInternal();

	// =================================================================================================================
	// === 스폰 위치 계산
	
	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	if (ItemDefinition->EquipAttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(ItemDefinition->EquipAttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(ItemDefinition->EquipAttachSocketName);
	}
	
	// =================================================================================================================
	// === 월드 검사

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	// =================================================================================================================
	// === 무기 스폰 및 부착

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = CharacterOwner;
	SpawnParams.Instigator = CharacterOwner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* SpawnedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, SpawnTransform, SpawnParams);
	if (!ensure(SpawnedWeapon))
	{
		return false;
	}

	SpawnedWeapon->SetReplicates(true);
	AttachWeaponToOwner(SpawnedWeapon, ItemDefinition);

	// =================================================================================================================
	// === 스탯 적용

	if (!ApplyItemStatSnapshot(PendingStatSnapshot, 1.f))
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("EquipItemInternal: failed to apply item stat snapshot for '%s', but keeping weapon equipped."),
			*GetNameSafe(ItemDefinition));
	}
	else
	{
		CurrentEquippedItemStatSnapshot = MoveTemp(PendingStatSnapshot);
		bHasAppliedCurrentEquippedItemStatSnapshot = true;
	}
	
	// =================================================================================================================
	// === 장착 상태 확정

	const FGuid NewCurrentItemId = InventoryComponent ? InventoryComponent->GetOrCreateItemId(ItemInstance) : FGuid();
	const bool bCurrentItemChanged = CurrentItemId != NewCurrentItemId;
	const bool bCurrentWeaponChanged = CurrentWeaponActor != SpawnedWeapon;
	CurrentItemId = NewCurrentItemId;
	CurrentWeaponActor = SpawnedWeapon;

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bCurrentItemChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentItemId, this);
		}

		if (bCurrentWeaponChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponActor, this);
		}
	}
	return true;
}

/** 실제 장착 해제 로직을 처리합니다. */
bool UEquipmentComponent::UnequipCurrentItemInternal()
{
	// =================================================================================================================
	// === 해제 가능 여부 검사
	
	if (!CurrentItemId.IsValid() && !CurrentWeaponActor)
	{
		return false;
	}

	// =================================================================================================================
	// === 스탯 되돌리기

	if (bHasAppliedCurrentEquippedItemStatSnapshot)
	{
		if (!ApplyItemStatSnapshot(CurrentEquippedItemStatSnapshot, -1.f))
		{
			UE_LOG(EquipmentComponentLog, Warning, TEXT("UnequipCurrentItemInternal: failed to revert current equipped item stat snapshot, but continuing unequip."));
		}
	}

	// =================================================================================================================
	// === 무기 제거
	
	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->Destroy();
	}
	
	// =================================================================================================================
	// === 상태 정리

	const bool bCurrentWeaponChanged = CurrentWeaponActor != nullptr;
	const bool bCurrentItemChanged = CurrentItemId.IsValid();
	CurrentWeaponActor = nullptr;
	CurrentItemId.Invalidate();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (bCurrentWeaponChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentWeaponActor, this);
		}

		if (bCurrentItemChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UEquipmentComponent, CurrentItemId, this);
		}
	}

	ClearRequestedItem();
	CurrentEquippedItemStatSnapshot.Reset();
	bHasAppliedCurrentEquippedItemStatSnapshot = false;
	return true;
}

/** 공격 데이터를 반환합니다. */
bool UEquipmentComponent::GetAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();

	const UItemDefinition* ItemDefinition = GetCurrentItemDefinition();
	if (!ensure(ItemDefinition))
	{
		return false;
	}

	UAnimMontage* AttackMontage = ItemDefinition->AttackMontage.LoadSynchronous();
	if (!ensure(AttackMontage))
	{
		return false;
	}

	OutAttackData.ItemDefinition = ItemDefinition;
	OutAttackData.AttackMontage = AttackMontage;
	return true;
}

/** 요청 아이템을 초기화합니다. */
UAnimMontage* UEquipmentComponent::GetCurrentHitReactMontage() const
{
	const UItemDefinition* ItemDefinition = GetCurrentItemDefinition();
	if (!ItemDefinition || ItemDefinition->HitReactMontage.IsNull())
	{
		return nullptr;
	}

	return ItemDefinition->HitReactMontage.LoadSynchronous();
}

void UEquipmentComponent::ClearRequestedItem()
{
	RequestedItemId.Invalidate();
}

/** 현재 아이템 정의를 반환합니다. */
const UItemDefinition* UEquipmentComponent::GetCurrentItemDefinition() const
{
	if (CurrentItemId.IsValid())
	{
		if (const UItemInstance* EquippedItemInstance = FindOwnedItemInstanceById(CurrentItemId))
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

/** 스탯 스냅샷을 적용합니다. */
bool UEquipmentComponent::ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const
{
	if (!StatSnapshot.HasAnyMagnitude())
	{
		return true;
	}

	UPdAbilitySystemComponent* ASC = FindOwnerPdAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(EquipmentComponentLog, Warning, TEXT("ApplyItemStatSnapshot failed: '%s' has no valid PdAbilitySystemComponent."), *GetNameSafe(GetOwner()));
		return false;
	}

	bool bAppliedAnyModifier = false;

	const auto ApplyMagnitudeMap = [ASC, MagnitudeScale, &bAppliedAnyModifier](const TMap<FGameplayTag, float>& StatMagnitudes)
	{
		for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
		{
			const float ScaledMagnitude = Pair.Value * MagnitudeScale;
			if (!Pair.Key.IsValid() || FMath::IsNearlyZero(ScaledMagnitude))
			{
				continue;
			}

			FGameplayAttribute Attribute;
			if (!ASC->ResolveAttributeFromTag(Pair.Key, Attribute))
			{
				UE_LOG(EquipmentComponentLog, Warning, TEXT("ApplyItemStatSnapshot skipped unresolved stat tag '%s'."), *Pair.Key.ToString());
				continue;
			}

			const float NewBaseValue = ASC->GetNumericAttributeBase(Attribute) + ScaledMagnitude;
			ASC->SetNumericAttributeBase(Attribute, NewBaseValue);
			bAppliedAnyModifier = true;
		}
	};

	ApplyMagnitudeMap(StatSnapshot.BaseStatMagnitudes);
	ApplyMagnitudeMap(StatSnapshot.EnhancedStatMagnitudes);
	return bAppliedAnyModifier;
}

/** 소유 인벤토리에서 아이템 인스턴스를 찾습니다. */
UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(FGuid ItemId) const
{
	// =================================================================================================================
	// === 인벤토리 조회
	
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!ensure(InventoryComponent))
	{
		return nullptr;
	}

	return InventoryComponent->FindItemInstanceById(ItemId);
}

/** 소유 인벤토리 컴포넌트를 반환합니다. */
UInventoryComponent* UEquipmentComponent::FindInventoryComponent() const
{
	// =================================================================================================================
	// === PlayerState 기준 조회
	
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PawnOwner ? PawnOwner->GetPlayerState() : nullptr);
	return PdPlayerState ? PdPlayerState->GetInventoryComponent() : nullptr;
}

/** 소유 ASC를 반환합니다. */
UPdAbilitySystemComponent* UEquipmentComponent::FindOwnerPdAbilitySystemComponent() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PawnOwner ? PawnOwner->GetPlayerState() : nullptr);
	return PdPlayerState ? PdPlayerState->GetPdAbilitySystemComponent() : nullptr;
}

/** 소유 캐릭터를 반환합니다. */
ACharacter* UEquipmentComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

/** 무기를 소유자 메시에 부착합니다. */
void UEquipmentComponent::AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const
{
	ACharacter* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!WeaponActor || !OwnerMesh)
	{
		return;
	}

	// =================================================================================================================
	// === 소켓 부착

	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ItemDefinition ? ItemDefinition->EquipAttachSocketName : NAME_None);
}
