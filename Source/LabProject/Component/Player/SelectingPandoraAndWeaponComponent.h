#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "Components/PlayerStateComponent.h"
#include "UObject/ObjectKey.h"
#include "SelectingPandoraAndWeaponComponent.generated.h"

class UPandoraDefinition;

/**
 * 현재 경기에서 선택한 무기·판도라 슬롯을 관리한다.
 *
 * 서버에서 선택 번호를 확정하고 선택 슬롯의 무기와 판도라를 Pawn에 적용한다. 인벤토리·판도라는 로드아웃 변경을
 * 알리기만 하고, 이 컴포넌트가 그 알림을 구독해 선택 슬롯의 내용이 바뀌었을 때만 다시 적용한다.
 * Pawn은 적용을 받을 준비가 되면(런타임 초기화·빙의) ApplySelectedPandoraAndWeapon을 요청한다.
 */
UCLASS()
class LABPROJECT_API USelectingPandoraAndWeaponComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	USelectingPandoraAndWeaponComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	int32 GetSelectedPandoraAndWeaponNumber() const { return SelectedPandoraAndWeaponNumber; }

	void RequestSelectPandoraAndWeapon(int32 PandoraAndWeaponNumber);

	/** 서버 권한에서만 호출한다. 준비된 컴포넌트에 적용하며, Pawn이나 슬롯 내용이 준비되면 다시 호출할 수 있다. */
	void ApplySelectedPandoraAndWeapon();

private:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerSelectPandoraAndWeapon(int32 PandoraAndWeaponNumber);

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandlePandoraLoadoutChanged();
	void ReapplyIfSelectedLoadoutChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	EEnum_Direction GetSelectedDirection() const;
	FGuid GetSelectedWeaponId() const;
	const UPandoraDefinition* GetSelectedPandoraDefinition() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!PandoraAndWeapon", meta = (AllowPrivateAccess = "true"))
	int32 SelectedPandoraAndWeaponNumber = 0;

	/** 마지막으로 Pawn에 적용한 선택 슬롯의 내용. 로드아웃이 바뀌어도 이 값과 같으면 다시 적용하지 않는다. */
	FGuid AppliedWeaponId;
	FObjectKey AppliedPandoraDefinition;
	bool bHasAppliedLoadout = false;
	FDelegateHandle WeaponLoadoutChangedHandle;
};
