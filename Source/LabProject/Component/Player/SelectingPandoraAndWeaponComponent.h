#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "SelectingPandoraAndWeaponComponent.generated.h"

/**
 * 현재 경기에서 선택한 무기·판도라 슬롯을 관리한다.
 *
 * 서버에서 선택 번호를 확정하고 무기와 판도라 적용을 조율한다.
 * Pawn이나 관련 컴포넌트가 나중에 준비되면 확정된 선택을 다시 적용할 수 있다.
 */
UCLASS()
class LABPROJECT_API USelectingPandoraAndWeaponComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	USelectingPandoraAndWeaponComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//--------------------------------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//--------------------------------------------------------------------------------------------------------------------------------------------
	int32 GetSelectedPandoraAndWeaponNumber() const { return SelectedPandoraAndWeaponNumber; }

	void RequestSelectPandoraAndWeapon(int32 PandoraAndWeaponNumber);

	/** 서버 권한에서만 호출한다. 준비된 컴포넌트에 적용하며, Pawn이나 슬롯 내용이 준비되면 다시 호출할 수 있다. */
	void ApplySelectedPandoraAndWeapon();

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectPandoraAndWeapon(int32 PandoraAndWeaponNumber);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!PandoraAndWeapon", meta = (AllowPrivateAccess = "true"))
	int32 SelectedPandoraAndWeaponNumber = 0;
};
