#include "Component/Player/SelectingPandoraAndWeaponComponent.h"

#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectingPandoraAndWeaponComponent)

// 매 프레임 처리 없이 선택 요청에 반응하고, 선택 상태를 네트워크로 공유하도록 구성한다.
USelectingPandoraAndWeaponComponent::USelectingPandoraAndWeaponComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 서버가 확정한 무기·판도라 선택 번호를 클라이언트에도 전달하도록 복제 항목에 등록한다.
void USelectingPandoraAndWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(USelectingPandoraAndWeaponComponent, SelectedPandoraAndWeaponNumber, Params);
}

// 무기·판도라 변경 요청을 서버에 전달하며, 클라이언트에서 선택 번호를 미리 바꾸지는 않는다.
void USelectingPandoraAndWeaponComponent::RequestSelectPandoraAndWeapon(const int32 PandoraAndWeaponNumber)
{
	// 서버에서 호출한 서버 RPC는 서버에서 바로 실행된다.
	ServerSelectPandoraAndWeapon(PandoraAndWeaponNumber);
}

// 요청한 슬롯 번호를 검증해 서버의 선택 상태를 확정하고 해당 무기와 판도라의 적용을 요청한다.
void USelectingPandoraAndWeaponComponent::ServerSelectPandoraAndWeapon_Implementation(const int32 PandoraAndWeaponNumber)
{
	const int32 SanitizedPandoraAndWeaponNumber = PandoraLoadout::NormalizeLoadoutNumber(PandoraAndWeaponNumber);
	if (SelectedPandoraAndWeaponNumber != SanitizedPandoraAndWeaponNumber)
	{
		SelectedPandoraAndWeaponNumber = SanitizedPandoraAndWeaponNumber;
		MARK_PROPERTY_DIRTY_FROM_NAME(USelectingPandoraAndWeaponComponent, SelectedPandoraAndWeaponNumber, this);
		GetOwner()->ForceNetUpdate();
	}

	// 선택 번호가 같아도 슬롯 내용이나 Pawn이 바뀌었을 수 있으므로 다시 적용한다.
	ApplySelectedPandoraAndWeapon();
}

// 서버에서 선택 슬롯의 무기와 판도라를 준비된 컴포넌트에 적용하며, Pawn이나 슬롯 내용이 바뀌면 다시 적용할 수 있다.
void USelectingPandoraAndWeaponComponent::ApplySelectedPandoraAndWeapon()
{
	APlayerState* PlayerState = GetPlayerState<APlayerState>();
	if (!PlayerState || !HasAuthority())
	{
		return;
	}

	const EEnum_Direction SelectedDirection = PandoraLoadout::GetDirectionFromLoadoutNumber(SelectedPandoraAndWeaponNumber);
	APawn* Pawn = PlayerState->GetPawn();
	UEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;

	// * 방향에 맞는 무기를 장착한다.
	if (Equipment)
	{
		const UInventoryComponent* Inventory = PlayerState->FindComponentByClass<UInventoryComponent>();
		UItemInstance* SelectedWeapon = Inventory ? Inventory->FindWeaponForLoadoutSlot(SelectedDirection) : nullptr;
		if (SelectedWeapon)
		{
			Equipment->RequestWeaponSelectionForDirection(SelectedDirection, SelectedWeapon);
		}
		else
		{
			Equipment->RequestWeaponUnequip();
		}
	}

	// * 방향에 맞는 판도라를 장착한다.
	if (UPandoraComponent* PandoraComponent = PlayerState->FindComponentByClass<UPandoraComponent>())
	{
		const UPandoraDefinition* PandoraDefinition = PandoraComponent->GetPandoraLoadoutDefinition(SelectedDirection);

		// 빈 슬롯도 선택한 방향은 유지하고, 판도라와 방향이 같으면 다시 적용하지 않는다.
		if (PandoraComponent->GetCurrentPandoraDefinition() != PandoraDefinition
			|| PandoraComponent->GetCurrentPandoraLoadoutDirection() != SelectedDirection)
		{
			PandoraComponent->RequestPandoraSelectionForDirection(SelectedDirection, PandoraDefinition);
		}
	}
}
