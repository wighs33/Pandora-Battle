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
 * 로비 상태 또는 공통 식별 정보가 바뀌면 구독자에게 알린다.
 */
UCLASS(BlueprintType, ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerStateComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	ULobbyPlayerStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//------------------------------------------------------------------------------------------------------------------
	FOnLobbyRuntimeStateChanged OnLobbyRuntimeStateChanged;

	void SetLeavingLobby(bool bInLeavingLobby);
	bool IsLeavingLobby() const { return bLeavingLobby; }

	void SetNickname(const FText& InNickname);
	void SetDefaultNickname(const FText& InNickname);
	void ClearCustomNickname();
	FText GetNickname() const;
	const FText& GetNicknameHint() const { return NicknameHint; }
	bool IsUsingNicknameHint() const { return bUsingNicknameHint && GetNickname().EqualTo(NicknameHint); }

	void SetTeamColorIndex(int32 InTeamColorIndex);
	int32 GetTeamColorIndex() const;

private:
	bool HasAuthority() const;
	UPlayerMatchComponent* GetPlayerMatchComponent() const;
	void SetNicknameInternal(const FText& InNickname, const FText& InNicknameHint, bool bInUsingNicknameHint);
	void HandleMatchDisplayNameChanged(const FText& NewDisplayName);
	void HandleMatchTeamColorChanged(int32 NewTeamColorIndex);
	void NotifyLobbyRuntimeStateChanged();

	UFUNCTION()
	void OnRep_LobbyState();

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
