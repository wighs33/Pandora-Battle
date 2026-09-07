#include "Component/Player/PlayerLoadoutComponent.h"

#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerLoadoutComponent)

// 매 프레임 처리 없이 선택 요청에 반응하고, 선택 상태를 네트워크로 공유하도록 구성한다.
UPlayerLoadoutComponent::UPlayerLoadoutComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 서버가 확정한 로드아웃 선택 번호를 클라이언트에도 전달하도록 복제 항목에 등록한다.
void UPlayerLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPlayerLoadoutComponent, SelectedLoadoutNumber);
}

// 로드아웃 변경 요청을 서버에 전달하며, 클라이언트에서 선택 번호를 미리 바꾸지는 않는다.
void UPlayerLoadoutComponent::RequestSelectLoadout(const int32 LoadoutNumber)
{
	// 서버에서 호출한 서버 RPC는 서버에서 바로 실행된다.
	ServerSelectLoadout(LoadoutNumber);
}

// 요청한 슬롯 번호를 검증해 서버의 선택 상태를 확정하고 해당 무기와 판도라의 적용을 요청한다.
void UPlayerLoadoutComponent::ServerSelectLoadout_Implementation(const int32 LoadoutNumber)
{
	const int32 SanitizedLoadoutNumber = PandoraLoadout::NormalizeLoadoutNumber(LoadoutNumber);
	if (SelectedLoadoutNumber != SanitizedLoadoutNumber)
	{
		SelectedLoadoutNumber = SanitizedLoadoutNumber;
		GetOwner()->ForceNetUpdate();
	}

	// 선택 번호가 같아도 슬롯 내용이나 Pawn이 바뀌었을 수 있으므로 다시 적용한다.
	ApplySelectedLoadout();
}

// 서버에서 선택 슬롯의 무기와 판도라를 준비된 컴포넌트에 적용하며, Pawn이나 슬롯 내용이 바뀌면 다시 적용할 수 있다.
void UPlayerLoadoutComponent::ApplySelectedLoadout()
{
	APlayerState* PlayerState = GetPlayerState<APlayerState>();
	if (!PlayerState || !HasAuthority())
	{
		return;
	}

	const EEnum_Direction SelectedDirection =
		PandoraLoadout::GetDirectionFromLoadoutNumber(SelectedLoadoutNumber);
	APawn* Pawn = PlayerState->GetPawn();
	UEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UEquipmentComponent>() : nullptr;

	if (Equipment)
	{
		const UInventoryComponent* Inventory = PlayerState->FindComponentByClass<UInventoryComponent>();
		UItemInstance* SelectedWeapon = Inventory ? Inventory->GetPandoraWeaponLoadoutItem(SelectedDirection) : nullptr;
		if (SelectedWeapon)
		{
			Equipment->RequestWeaponSelectionForDirection(SelectedDirection, SelectedWeapon);
		}
		else
		{
			Equipment->RequestWeaponUnequip();
		}
	}

	if (UPandoraComponent* PandoraComponent = PlayerState->FindComponentByClass<UPandoraComponent>())
	{
		const UPandoraDefinition* PandoraDefinition = SelectedDirection == EEnum_Direction::Center
			? nullptr
			: PandoraComponent->GetPandoraLoadoutDefinition(SelectedDirection);
		const EEnum_Direction PandoraDirection = PandoraDefinition ? SelectedDirection : EEnum_Direction::Center;
		// 판도라와 슬롯 방향이 모두 같으면 능력을 해제하고 다시 부여하지 않는다.
		if (PandoraComponent->GetCurrentPandoraDefinition() != PandoraDefinition
			|| PandoraComponent->GetCurrentPandoraLoadoutDirection() != PandoraDirection)
		{
			PandoraComponent->RequestPandoraSelectionForDirection(PandoraDirection, PandoraDefinition);
		}
	}
}
