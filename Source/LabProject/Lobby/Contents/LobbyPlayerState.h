#pragma once

#include "CoreMinimal.h"
#include "Mode/PdPlayerState.h"
#include "LobbyPlayerState.generated.h"

class UBasicAttributeSet;
class ULobbyPlayerStateComponent;

/**
 * 로비 플레이어의 상태와 프리뷰용 기본 속성을 소유한다.
 *
 * 표시 이름과 팀은 경기 정보 컴포넌트를 원본으로 사용하며,
 * 화면 갱신은 상태 변경을 구독하는 HUD가 담당한다.
 */
UCLASS()
class LABPROJECT_API ALobbyPlayerState : public APdPlayerState
{
	GENERATED_BODY()

public:
	ALobbyPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;

	//------------------------------------------------------------------------------------------------------------------
	ULobbyPlayerStateComponent* GetLobbyPlayerStateComponent() const { return LobbyPlayerStateComponent.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetLeavingLobby(bool bInLeavingLobby);

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool IsLeavingLobby() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetDefaultNickname(const FText& InNickname);

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void ClearCustomNickname();

	UFUNCTION()
	FText GetNickname() const;

	UFUNCTION()
	FText GetNicknameHint() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby")
	bool IsUsingNicknameHint() const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Team")
	void SetTeamColorIndex(int32 InTeamColorIndex);

	UFUNCTION()
	int32 GetTeamColorIndex() const;

protected:
	virtual void ReceiveMatchIdentityFromCopyProperties(const FPlayerMatchIdentity& Identity) override;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Lobby|State", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULobbyPlayerStateComponent> LobbyPlayerStateComponent;

	//------------------------------------------------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "!Lobby|Preview", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBasicAttributeSet> LobbyBasicAttributeSet;
};
