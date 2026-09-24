#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "LobbyPlayerStateComponent.generated.h"

class UPlayerMatchComponent;

DECLARE_MULTICAST_DELEGATE(FOnLobbyRuntimeStateChanged);

/**
 * 로비의 퇴장 여부와 닉네임 입력 상태를 복제한다.
 *
 * 실제 표시 이름과 팀은 PlayerMatchComponent에서 조회하며,
 * 퇴장 여부와 닉네임 힌트의 변경만 알린다. 공통 식별 정보는 GameState가 직접 구독한다.
 */
UCLASS(BlueprintType, ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerStateComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	ULobbyPlayerStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetLeavingLobby(bool bInLeavingLobby);
	bool IsLeavingLobby() const { return bLeavingLobby; }

	void SetNickname(const FText& InNickname);
	void SetDefaultNickname(const FText& InNickname);
	void ClearCustomNickname();
	const FText& GetNicknameHint() const { return NicknameHint; }
	bool IsUsingNicknameHint() const;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_LobbyState();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool HasAuthority() const;
	UPlayerMatchComponent* GetPlayerMatchComponent() const;
	void SetNicknameInternal(const FText& InNickname, const FText& InNicknameHint, bool bInUsingNicknameHint);

public:
	FOnLobbyRuntimeStateChanged OnLobbyRuntimeStateChanged;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_LobbyState,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	bool bLeavingLobby = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_LobbyState,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	FText NicknameHint;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_LobbyState,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	bool bUsingNicknameHint = false;
};
