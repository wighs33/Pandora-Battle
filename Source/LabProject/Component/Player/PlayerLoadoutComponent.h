#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "PlayerLoadoutComponent.generated.h"

/**
 * 현재 경기에서 선택한 로드아웃 슬롯을 관리한다.
 *
 * 서버에서 선택 번호를 확정하고 무기와 판도라 적용을 조율한다.
 * Pawn이나 관련 컴포넌트가 나중에 준비되면 확정된 선택을 다시 적용할 수 있다.
 */
UCLASS(BlueprintType, ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerLoadoutComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UPlayerLoadoutComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//--------------------------------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//--------------------------------------------------------------------------------------------------------------------------------------------
	int32 GetSelectedLoadoutNumber() const { return SelectedLoadoutNumber; }

	void RequestSelectLoadout(int32 LoadoutNumber);

	/** 서버 권한에서만 호출한다. 준비된 컴포넌트에 적용하며, Pawn이나 슬롯 내용이 준비되면 다시 호출할 수 있다. */
	void ApplySelectedLoadout();

private:
	UFUNCTION(Server, Reliable)
	void ServerSelectLoadout(int32 LoadoutNumber);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "!Loadout", meta = (AllowPrivateAccess = "true"))
	int32 SelectedLoadoutNumber = 0;
};
