#pragma once

#include "Components/PlayerStateComponent.h"
#include "CoreMinimal.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "LobbyPlayerStateComponent.generated.h"

class UBasicAttributeSet;
class ULobbyPreviewDefinition;
class UPdAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE(FOnLobbyRuntimeStateChanged);

/**
 * Replicated lobby presence and identity state owned by ALobbyPlayerState.
 */
UCLASS(BlueprintType, ClassGroup = (Lobby))
class LABPROJECT_API ULobbyPlayerStateComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	ULobbyPlayerStateComponent(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FOnLobbyRuntimeStateChanged OnLobbyRuntimeStateChanged;

	void SetReady(bool bInReady);
	bool IsReady() const { return bReady; }

	void SetLeavingLobby(bool bInLeavingLobby);
	bool IsLeavingLobby() const { return bLeavingLobby; }

	void SetNickname(const FText& InNickname);
	void SetDefaultNickname(const FText& InNickname);
	void ClearCustomNickname();
	const FText& GetNickname() const { return Nickname; }
	const FText& GetNicknameHint() const { return NicknameHint; }
	bool IsUsingNicknameHint() const { return bUsingNicknameHint; }

	void SetTeamColorIndex(int32 InTeamColorIndex);
	int32 GetTeamColorIndex() const;

	void ImportPlayerMatchIdentity(
		const FPlayerMatchIdentity& InMatchIdentity);
	FPlayerMatchIdentity BuildConfirmedPlayerMatchIdentity() const;

	void InitializePreviewAbilitySystem(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		UBasicAttributeSet* BasicAttributeSet,
		const ULobbyPreviewDefinition* PreviewDefinition);

private:
	bool HasAuthority() const;
	UPlayerMatchComponent* GetPlayerMatchComponent() const;
	void SetNicknameInternal(
		const FText& InNickname,
		const FText& InNicknameHint,
		bool bInUsingNicknameHint);
	void HandleMatchTeamColorChanged(int32 NewTeamColorIndex);
	void NotifyLobbyRuntimeStateChanged();

	UFUNCTION()
	void OnRep_Ready();

	UFUNCTION()
	void OnRep_LeavingLobby();

	UFUNCTION()
	void OnRep_Nickname();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Ready,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	bool bReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		ReplicatedUsing = OnRep_LeavingLobby, Category = "!Lobby",
		meta = (AllowPrivateAccess = "true"))
	bool bLeavingLobby = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Nickname,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Nickname,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	FText NicknameHint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Nickname,
		Category = "!Lobby", meta = (AllowPrivateAccess = "true"))
	bool bUsingNicknameHint = false;
};
