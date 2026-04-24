#include "Equipment/EquipmentComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Item/InventoryComponent.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentComponent)

DEFINE_LOG_CATEGORY(EquipmentComponentLog);

/**
 * 초기화
 */
UEquipmentComponent::UEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	// 리플리케이션 true
	SetIsReplicatedByDefault(true);
}

/**
 * 엔진API : 네트워크 시작 시점
 */
void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// =================================================================================================================
	// ### [현재 무기 액터] 모든 클라에 동기화, [현재 아이템 아이디] 소유자만 동기화
	DOREPLIFETIME(UEquipmentComponent, CurrentWeaponActor);
	DOREPLIFETIME_CONDITION(UEquipmentComponent, CurrentItemId, COND_OwnerOnly);
}

/**
 * 장착 요청된 아이템 정보 반환
 */
const UItemDefinition* UEquipmentComponent::GetRequestedItemDefinition() const
{
	// =================================================================================================================
	// ### [요청된 아이템 ID]로 [아이템 정의] 조회
	if (RequestedItemId.IsValid())
	{
		if (const UItemInstance* ItemInstance = FindOwnedItemInstanceById(RequestedItemId))
		{
			return ItemInstance->ItemDefinition.Get();
		}
	}

	return nullptr;
}

/**
 * 인수를 통해 [장착 데이터] 반환
 */
bool UEquipmentComponent::GetEquipData(FEquipData& OutEquipData) const
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard
	
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
	// ### 장착 데이터 구성
	
	OutEquipData.ItemDefinition = ItemDefinition;
	OutEquipData.EquipMontage = EquipMontage;
	OutEquipData.EquipAnimLayer = ItemDefinition->EquipAnimLayer.LoadSynchronous();
	OutEquipData.EquipEffectClass = ItemDefinition->EquipEffectClass.LoadSynchronous();
	OutEquipData.EquipCueTag = ItemDefinition->EquipCueTag;
	return true;
}

/**
 * 인수를 통해 [장착 해제 데이터] 반환
 */
bool UEquipmentComponent::GetUnequipData(FUnequipData& OutUnequipData) const
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard
	
	OutUnequipData = FUnequipData();

	const UItemDefinition* ItemDefinition = GetCurrentItemDefinition();
	if (!ItemDefinition)
	{
		// 현재 장착 아이템 없으면 바로 리턴
		return false;
	}

	UAnimMontage* UnequipMontage = ItemDefinition->UnequipMontage.LoadSynchronous();
	if (!ensure(UnequipMontage))
	{
		return false;
	}
		
	// =================================================================================================================
	// ### 장착 해제 데이터 구성

	OutUnequipData.ItemDefinition = ItemDefinition;
	OutUnequipData.UnequipMontage = UnequipMontage;
	OutUnequipData.UnequipEffectClass = ItemDefinition->UnequipEffectClass.LoadSynchronous();
	return true;
}

/**
 * 요청된 아이템 인스턴스 저장
 */
bool UEquipmentComponent::SetRequestedItemInstance(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard
	
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
	// ### 로컬 요청 상태 갱신 & 동기화

	RequestedItemId = ItemId;
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		ServerSetRequestedItem(ItemId);
	}

	return true;
}

/**
 * 요청된 아이템 장착 (요청된 아이템 : 판도라&무기 선택 창에서 선택한 무기)
 */
bool UEquipmentComponent::EquipRequestedItem()
{
	const FGuid RequestedItem = RequestedItemId;
	ClearRequestedItem();
	if (!ensure(RequestedItem.IsValid()))
	{
		return false;
	}

	// 장착하고 성공여부 반환
	return EquipItemById(RequestedItem);
}

/**
 * 아이템 장착
 */
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

	// 장착하고 성공여부 반환
	return EquipItemById(ItemId);
}

/**
 * 장착 중인 아이템 해제
 */
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
	
	// 장착 해제하고 성공여부 반환
	return UnequipCurrentItemInternal();
}

void UEquipmentComponent::ServerEquipItem_Implementation(FGuid ItemId)
{
	if (!ensure(ItemId.IsValid()))
	{
		return;
	}
	
	// 장착
	EquipItemById(ItemId);
}

void UEquipmentComponent::ServerSetRequestedItem_Implementation(FGuid ItemId)
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard
	
	if (!ensure(ItemId.IsValid()))
	{
		ClearRequestedItem();
		return;
	}

	// =================================================================================================================
	// ### 아이디 설정
	
	RequestedItemId = ItemId;
	ensure(FindOwnedItemInstanceById(ItemId));
}

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


/**
 * 실제 아이템 장착 로직
 */
bool UEquipmentComponent::EquipItemInternal(UItemInstance* ItemInstance)
{
	// =================================================================================================================
	// ### 초기화 & 안전성 guard
	
	ACharacter* CharacterOwner = GetCharacterOwner();
	const UItemDefinition* ItemDefinition = ItemInstance ? ItemInstance->ItemDefinition.Get() : nullptr;
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!ensure(CharacterOwner) || !ensure(ItemDefinition))
	{
		return false;
	}

	// =================================================================================================================
	// ### 이미 같은 아이템이 장착된 상태라면 중복 처리 생략
	
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
	// ### 무기를 부착할 소유자 메쉬 확보
	
	USkeletalMeshComponent* OwnerMesh = CharacterOwner->GetMesh();
	if (!ensure(OwnerMesh))
	{
		return false;
	}

	// =================================================================================================================
	// ### [아이템 정의]에서 실제 무기 액터 클래스 로드
	
	TSubclassOf<AWeaponBase> WeaponClass = ItemDefinition->EquippedActorClass.LoadSynchronous();
	if (!ensure(WeaponClass))
	{
		return false;
	}

	// =================================================================================================================
	// ### 새 무기 장착 전 기존 장비 제거
	
	UnequipCurrentItemInternal();

	// =================================================================================================================
	// ### 기본 스폰 위치 설정
	
	FTransform SpawnTransform = OwnerMesh->GetComponentTransform();
	if (ItemDefinition->EquipAttachSocketName != NAME_None && OwnerMesh->DoesSocketExist(ItemDefinition->EquipAttachSocketName))
	{
		SpawnTransform = OwnerMesh->GetSocketTransform(ItemDefinition->EquipAttachSocketName);
	}
	
	// =================================================================================================================
	// ### 월드 유효성 검사

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	// =================================================================================================================
	// ### 무기 액터 스폰 후 메시에 부착

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
	// ### 현재 장착 상태 확정

	CurrentItemId = InventoryComponent ? InventoryComponent->GetOrCreateItemId(ItemInstance) : FGuid();
	CurrentWeaponActor = SpawnedWeapon;
	return true;
}

/**
 * 실제 아이템 장착 해제 로직
 */
bool UEquipmentComponent::UnequipCurrentItemInternal()
{
	// =================================================================================================================
	// ### 해제 연출용 정의 데이터 보존
	
	if (!CurrentItemId.IsValid() && !CurrentWeaponActor)
	{
		return false;
	}

	// =================================================================================================================
	// ### 현재 무기 액터 제거
	
	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->Destroy();
	}
	
	// =================================================================================================================
	// ### 장착/요청 상태 정리

	CurrentWeaponActor = nullptr;
	CurrentItemId.Invalidate();
	ClearRequestedItem();
	return true;
}

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
	OutAttackData.AttackEffectClass = ItemDefinition->AttackEffectClass.LoadSynchronous();
	return true;
}

void UEquipmentComponent::ClearRequestedItem()
{
	// 아이디 무효화
	RequestedItemId.Invalidate();
}

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

UItemInstance* UEquipmentComponent::FindOwnedItemInstanceById(FGuid ItemId) const
{
	// =================================================================================================================
	// ### 소유자 PlayerState에서 인벤토리 컴포넌트 조회
	
	UInventoryComponent* InventoryComponent = FindInventoryComponent();
	if (!ensure(InventoryComponent))
	{
		return nullptr;
	}

	return InventoryComponent->FindItemInstanceById(ItemId);
}

UInventoryComponent* UEquipmentComponent::FindInventoryComponent() const
{
	// =================================================================================================================
	// ### PawnOwner로 부터 InventoryComponent 획득
	
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	const APlayerState* PlayerState = PawnOwner ? PawnOwner->GetPlayerState() : nullptr;
	return PlayerState ? PlayerState->FindComponentByClass<UInventoryComponent>() : nullptr;
}

ACharacter* UEquipmentComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

void UEquipmentComponent::AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const
{
	ACharacter* CharacterOwner = GetCharacterOwner();
	USkeletalMeshComponent* OwnerMesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
	if (!WeaponActor || !OwnerMesh)
	{
		return;
	}

	// =================================================================================================================
	// ### 지정 소켓에 무기 부착
	WeaponActor->AttachToComponent(
		OwnerMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ItemDefinition ? ItemDefinition->EquipAttachSocketName : NAME_None);
}
